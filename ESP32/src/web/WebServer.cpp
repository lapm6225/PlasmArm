#include "WebServer.h"
#include "../Config.h"
#include "web_assets.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>

WebServer::WebServer()
    : server(nullptr), ws(nullptr), commandQueue(nullptr),
      motionQueue(nullptr), processedCount(0) {}

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

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed in WebServer::init");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }

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

  // Upload JSON file body (array/object) and enqueue commands
  server->on("/upload", HTTP_POST, nullptr, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // support chunked upload; we simply handle the full body at once
    static String body;
    if (index == 0) {
      body = String();
      body.reserve(total + 1);
    }
    body += String((char *)data, len);

    if (index + len == total) {
      if (loadCommandsFromJson(body)) {
        request->send(200, "application/json", "{\"status\":\"queued\"}");
      } else {
        request->send(400, "application/json", "{\"status\":\"invalid_or_empty\"}");
      }
    }
  });

  server->on("/run_file", HTTP_GET, [this](AsyncWebServerRequest *request) {
    const char *path = "/commands.json";
    bool ok = loadCommandsFromSPIFFS(path);
    if (ok) {
      request->send(200, "application/json", "{\"status\":\"file_loaded\"}");
    } else {
      request->send(404, "application/json", "{\"status\":\"file_not_found_or_invalid\"}");
    }
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

// partie http
void WebServer::handleRoot(AsyncWebServerRequest *request) {
  // Send the HTML page from PROGMEM with UTF-8 charset
  request->send(200, "text/html; charset=UTF-8", WEB_HTML);
}

void WebServer::handleMove(AsyncWebServerRequest *request) {
  if (request->hasParam("x") && request->hasParam("y")) {
    float x = request->getParam("x")->value().toFloat();
    float y = request->getParam("y")->value().toFloat();

    Command cmd(Command::MOVE_TO, x, y);

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
    request->send(400, "text/plain", "Missing parameters");
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
// fin partie http

// partie websocket
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
      doc["handled"] = processedCount;
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
      processedCount++;
      Serial.printf("WS: Ignoring fragmented frame (final=%d, idx=%u, "
                    "flen=%u, len=%u)\n",
                    info->final, (unsigned)info->index, (unsigned)info->len,
                    (unsigned)len);
      if (client->canSend()) {
        client->text("{\"type\":\"ERROR\",\"msg\":\"Fragmented ignored\",\"handled\":" + String(processedCount) + "}");
      }
      return;
    }

    // Build String with explicit length (data is NOT null-terminated!)
    String message;
    message.reserve(len + 1);
    message = String((char *)data, len);
    Command cmd;

    if (parseCommand(message, cmd)) {
      if (commandQueue && xQueueSend(commandQueue, &cmd, 0) == pdTRUE) {
        processedCount++; // Increment only when successfully queued

        // Always try to send an ACK so Python's in_flight counter stays accurate.
        // If the outbound queue is full, call cleanupClients() to free space first.
        if (!client->canSend()) {
          ws->cleanupClients(); // Flush stale/completed sends
        }
        if (client->canSend()) {
          int freeSlots = uxQueueSpacesAvailable(commandQueue);
          // Minimal ACK to save buffer space: ~45 bytes
          String ack = "{\"type\":\"ACK\",\"cmdFree\":" + String(freeSlots) +
                       ",\"handled\":" + String(processedCount) + "}";
          client->text(ack);
        } else {
          // Outbound queue still full: the next STATUS broadcast (which goes
          // through safeTextAll) will carry the latest handled/cmdFree, so
          // Python will eventually re-sync. Log for debugging.
          Serial.printf("WS: ACK dropped (outbound full) handled=%u\n", processedCount);
        }
      } else {
        // Command queue full: notify Python so it backs off
        if (!client->canSend()) {
          ws->cleanupClients();
        }
        if (client->canSend()) {
          client->text("{\"type\":\"ERROR\",\"msg\":\"Buffer Full\",\"handled\":" + String(processedCount) + "}");
        }
      }
    } else {
      // Return ERROR for invalid commands
      if (client->canSend()) {
        client->text("{\"type\":\"ERROR\",\"msg\":\"Invalid Command\",\"handled\":" + String(processedCount) + "}");
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
    cmd = Command(Command::MOVE_TO, x, y, z, speed, tool);

    // Optional precomputed joint angles
    if (doc.containsKey("theta1") && doc.containsKey("theta2")) {
      cmd.hasJointAngles = true;
      cmd.theta1 = doc["theta1"] | 0.0f;
      cmd.theta2 = doc["theta2"] | 0.0f;
    } else {
      cmd.hasJointAngles = false;
      cmd.theta1 = 0.0f;
      cmd.theta2 = 0.0f;
    }

    // Optional reachable flag from pre-processing (skip invalid)
    if (doc.containsKey("reachable") && !doc["reachable"]) {
      return false;
    }
  } else if (typeStr == "TOOL") {
    // Tool control: {"type":"TOOL","state":true,"z":5.0}
    bool state = doc["state"] | false;
    float z = doc["z"] | 0.0f;
    cmd = Command(Command::TOOL_CONTROL, state, z);
  } else if (typeStr == "HOME") {
    cmd = Command(Command::HOME, HOME_X, HOME_Y);
  } else if (typeStr == "STOP") {
    cmd = Command(Command::STOP, 0.0f, 0.0f);
  } else if (typeStr == "SET_SPEED") {
    float speed = doc["speed"] | DEFAULT_SPEED;
    cmd = Command(Command::SET_SPEED, 0.0f, 0.0f, 0.0f, speed);
  } else {
    Serial.printf("WebServer: Unknown command type: %s\n", typeStr.c_str());
    return false;
  }

  return true;
}

bool WebServer::loadCommandsFromJson(const String &jsonContent) {
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, jsonContent);
  if (error) {
    Serial.printf("loadCommandsFromJson: JSON error %s\n", error.c_str());
    return false;
  }

  JsonArray arr;
  if (doc.is<JsonArray>()) {
    arr = doc.as<JsonArray>();
  } else if (doc.containsKey("commands") && doc["commands"].is<JsonArray>()) {
    arr = doc["commands"].as<JsonArray>();
  } else {
    Serial.println("loadCommandsFromJson: invalid JSON structure");
    return false;
  }

  bool queuedAny = false;
  for (JsonVariant item : arr) {
    if (!item.is<JsonObject>())
      continue;

    String itemJson;
    serializeJson(item, itemJson);
    Command cmd;
    if (!parseCommand(itemJson, cmd)) {
      Serial.println("loadCommandsFromJson: skipping invalid command");
      continue;
    }

    if (commandQueue && xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      queuedAny = true;
    } else {
      Serial.println("loadCommandsFromJson: command queue full");
      break;
    }
  }

  return queuedAny;
}

bool WebServer::loadCommandsFromSPIFFS(const char *path) {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return false;
  }

  if (!SPIFFS.exists(path)) {
    Serial.printf("SPIFFS file not found: %s\n", path);
    return false;
  }

  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    Serial.printf("Unable to open SPIFFS file: %s\n", path);
    return false;
  }

  String content;
  while (file.available()) {
    content += char(file.read());
  }
  file.close();

  return loadCommandsFromJson(content);
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
  doc["handled"] = processedCount;

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
  doc["handled"] = processedCount;
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
