#include "WebServer.h"
#include "../Config.h"
#include "web_assets.h"
#include <ArduinoJson.h>

WebServer::WebServer()
    : server(nullptr), ws(nullptr), commandQueue(nullptr),
      motionQueue(nullptr) {}

WebServer::~WebServer() {
  if (ws) {
    delete ws;
  }
  if (server) {
    delete server;
  }
}

void WebServer::init(QueueHandle_t cmdQueue, QueueHandle_t motQueue) {
  commandQueue = cmdQueue;
  motionQueue = motQueue;

  server = new AsyncWebServer(80);
  ws = new AsyncWebSocket("/ws");

  // WebSocket event handler
  ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                     AwsEventType type, void *arg, uint8_t *data, size_t len) {
    this->onWebSocketEvent(server, client, type, arg, data, len);
  });

  // HTTP routes
  server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->handleRoot(request);
  });

  server->on("/move", HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->handleMove(request);
  });

  server->on("/home", HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->handleHome(request);
  });

  server->on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->handleStatus(request);
  });

  // Add WebSocket handler
  server->addHandler(ws);
}

void WebServer::begin() {
  if (server) {
    server->begin();
    Serial.println("Web server started");
  }
}

// HTTP handlers
void WebServer::handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html; charset=UTF-8", WEB_HTML);
}

void WebServer::handleMove(AsyncWebServerRequest *request) {
  if (request->hasParam("x") && request->hasParam("y")) {
    float x = request->getParam("x")->value().toFloat();
    float y = request->getParam("y")->value().toFloat();
    float z = request->hasParam("z") ? request->getParam("z")->value().toFloat()
                                     : 0.0f;
    float spd = request->hasParam("speed")
                    ? request->getParam("speed")->value().toFloat()
                    : DEFAULT_SPEED;
    bool tool = request->hasParam("tool")
                    ? (request->getParam("tool")->value() == "1")
                    : false;

    Command cmd(Command::MOVE_TO, x, y, spd, z, tool);

    if (commandQueue) {
      if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        request->send(200, "text/plain", "OK");
      } else {
        request->send(503, "text/plain", "Command queue full");
      }
    } else {
      request->send(500, "text/plain", "Command queue not initialized");
    }
  } else {
    request->send(400, "text/plain", "Missing parameters (x, y required)");
  }
}

void WebServer::handleHome(AsyncWebServerRequest *request) {
  if (commandQueue) {
    Command cmd(Command::HOME, 0.0f, 0.0f);
    if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      request->send(200, "text/plain", "Homing started");
    } else {
      request->send(503, "text/plain", "Command queue full");
    }
  } else {
    request->send(500, "text/plain", "Command queue not initialized");
  }
}

void WebServer::handleStatus(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(512);
  doc["status"] = "running";
  if (commandQueue) {
    doc["cmdFree"] = uxQueueSpacesAvailable(commandQueue);
  }
  if (motionQueue) {
    doc["motFree"] = uxQueueSpacesAvailable(motionQueue);
  }

  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

// WebSocket handler
void WebServer::onWebSocketEvent(AsyncWebSocket *server,
                                 AsyncWebSocketClient *client,
                                 AwsEventType type, void *arg, uint8_t *data,
                                 size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(),
                  client->remoteIP().toString().c_str());

    // Send initial buffer status on connect
    if (commandQueue) {
      int freeSlots = uxQueueSpacesAvailable(commandQueue);
      DynamicJsonDocument doc(256);
      doc["type"] = "BUFFER";
      doc["cmdFree"] = freeSlots;
      if (motionQueue) {
        doc["motFree"] = uxQueueSpacesAvailable(motionQueue);
      }
      String json;
      serializeJson(doc, json);
      client->text(json);
    }

  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    // Validate frame completeness (ignore fragmented messages)
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (!info->final || info->index != 0 || info->len != len) {
      Serial.printf("WS: Ignoring fragmented frame (final=%d, idx=%u, "
                    "flen=%u, len=%u)\n",
                    info->final, (unsigned)info->index, (unsigned)info->len,
                    (unsigned)len);
      return;
    }

    // Build String with explicit length (data is NOT null-terminated!)
    String message;
    message.reserve(len + 1);
    message = String((char *)data, len);
    Command cmd;

    if (parseCommand(message, cmd) && commandQueue) {
      if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        // ACK with current buffer status — only if outbound queue has room
        // This prevents "Too many messages queued" which kills the connection
        if (client->canSend()) {
          int freeSlots = uxQueueSpacesAvailable(commandQueue);
          DynamicJsonDocument doc(256);
          doc["type"] = "ACK";
          doc["cmdFree"] = freeSlots;
          String json;
          serializeJson(doc, json);
          client->text(json);
        }
      } else {
        if (client->canSend()) {
          client->text("{\"type\":\"ERROR\",\"msg\":\"Buffer Full\"}");
        }
      }
    }
  }
}

bool WebServer::parseCommand(const String &json, Command &cmd) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    return false;
  }

  String typeStr = doc["type"] | "MOVE_TO";

  if (typeStr == "MOVE_TO") {
    float x = doc["x"] | 0.0f;
    float y = doc["y"] | 0.0f;
    float z = doc["z"] | 0.0f;
    float speed = doc["speed"] | DEFAULT_SPEED;
    bool tool = doc["tool"] | false;
    cmd = Command(Command::MOVE_TO, x, y, speed, z, tool);
  } else if (typeStr == "TOOL") {
    // Tool control: {"type":"TOOL","state":true,"z":5.0}
    bool state = doc["state"] | false;
    float z = doc["z"] | 0.0f;
    cmd = Command(Command::TOOL_CONTROL, state, z);
  } else if (typeStr == "HOME") {
    cmd = Command(Command::HOME, 0.0f, 0.0f);
  } else if (typeStr == "STOP") {
    cmd = Command(Command::STOP, 0.0f, 0.0f);
  } else if (typeStr == "SET_SPEED") {
    float speed = doc["speed"] | DEFAULT_SPEED;
    cmd = Command(Command::SET_SPEED, 0.0f, 0.0f, speed);
  } else {
    Serial.printf("WebServer: Unknown command type: %s\n", typeStr.c_str());
    return false;
  }

  return true;
}

void WebServer::broadcastStatus(const RobotState &state) {
  if (!ws || ws->count() == 0)
    return; // No clients connected

  DynamicJsonDocument doc(512);
  doc["type"] = "STATUS";
  doc["x"] = state.currentPosition.x;
  doc["y"] = state.currentPosition.y;
  doc["z"] = state.toolZ;
  doc["tool"] = state.toolActive;
  doc["theta1"] = state.currentAngles.theta1;
  doc["theta2"] = state.currentAngles.theta2;
  doc["isMoving"] = state.isMoving;
  doc["isHomed"] = state.isHomed;

  // Include buffer status in every status broadcast
  if (commandQueue) {
    doc["cmdFree"] = uxQueueSpacesAvailable(commandQueue);
  }
  if (motionQueue) {
    doc["motFree"] = uxQueueSpacesAvailable(motionQueue);
  }

  String json;
  serializeJson(doc, json);

  safeTextAll(json);
}

void WebServer::broadcastBufferStatus(int cmdFreeSlots) {
  if (!ws || ws->count() == 0)
    return; // No clients connected

  DynamicJsonDocument doc(256);
  doc["type"] = "BUFFER";
  doc["cmdFree"] = cmdFreeSlots;
  if (motionQueue) {
    doc["motFree"] = uxQueueSpacesAvailable(motionQueue);
  }

  String json;
  serializeJson(doc, json);

  safeTextAll(json);
}

void WebServer::safeTextAll(const String &message) {
  // Send to each connected client individually, skipping those with full
  // queues. Unlike ws->textAll(), this won't trigger "Too many messages queued"
  // disconnects.
  for (auto &client : ws->getClients()) {
    if (client.status() == WS_CONNECTED && client.canSend()) {
      client.text(message);
    }
  }
}

void WebServer::cleanup() {
  if (ws) {
    ws->cleanupClients();
  }
}
