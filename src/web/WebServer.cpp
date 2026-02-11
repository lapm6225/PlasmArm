#include "WebServer.h"
#include "web_assets.h"
#include "../Config.h"
#include <ArduinoJson.h>

WebServer::WebServer() : server(nullptr), ws(nullptr), commandQueue(nullptr) {
}

WebServer::~WebServer() {
    if (ws) {
        delete ws;
    }
    if (server) {
        delete server;
    }
}

void WebServer::init(QueueHandle_t cmdQueue) {
    commandQueue = cmdQueue;
    
    server = new AsyncWebServer(80);
    ws = new AsyncWebSocket("/ws");
    
    // WebSocket event handler
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    
    // HTTP routes
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleRoot(request);
    });
    
    server->on("/move", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleMove(request);
    });
    
    server->on("/home", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleHome(request);
    });
    
    server->on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
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

void WebServer::handleRoot(AsyncWebServerRequest* request) {
    // Send the HTML page from PROGMEM with UTF-8 charset
    request->send(200, "text/html; charset=UTF-8", WEB_HTML);
}

void WebServer::handleMove(AsyncWebServerRequest* request) {
    if (request->hasParam("x") && request->hasParam("y")) {
        float x = request->getParam("x")->value().toFloat();
        float y = request->getParam("y")->value().toFloat();
        
        Point2D target(x, y);
        Command cmd(Command::MOVE_TO, target);
        
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

void WebServer::handleHome(AsyncWebServerRequest* request) {
    if (commandQueue) {
        Command cmd(Command::HOME, Point2D(0, 0));
        if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            request->send(200, "text/plain", "Homing started");
        } else {
            request->send(503, "text/plain", "Command queue full");
        }
    } else {
        request->send(500, "text/plain", "Command queue not initialized");
    }
}

void WebServer::handleStatus(AsyncWebServerRequest* request) {
    // Return JSON status (simplified - would need robot state)
    String json = "{\"status\":\"running\"}";
    request->send(200, "application/json", json);
}

void WebServer::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                 AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected from %s\n", 
                     client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // Parse incoming message
        String message = String((char*)data);
        Command cmd;
        
        if (parseCommand(message, cmd) && commandQueue) {
            if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
                Serial.printf("WebSocket: Command received and queued\n");
            } else {
                Serial.println("WebSocket: WARNING - Command queue full!");
            }
        }
    }
}

bool WebServer::parseCommand(const String& json, Command& cmd) {
    // Simple JSON parsing (could use ArduinoJson for more complex cases)
    // Expected format: {"type":"MOVE_TO","x":100,"y":50,"speed":50}
    
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
        float speed = doc["speed"] | DEFAULT_SPEED;
        cmd = Command(Command::MOVE_TO, Point2D(x, y), speed);
    } else if (typeStr == "HOME") {
        cmd = Command(Command::HOME, Point2D(0, 0));
    } else if (typeStr == "STOP") {
        cmd = Command(Command::STOP, Point2D(0, 0));
    } else {
        return false;
    }
    
    return true;
}

void WebServer::broadcastStatus(const RobotState& state) {
    if (!ws) return;
    
    // Create JSON status message
    DynamicJsonDocument doc(512);
    doc["x"] = state.currentPosition.x;
    doc["y"] = state.currentPosition.y;
    doc["theta1"] = state.currentAngles.theta1;
    doc["theta2"] = state.currentAngles.theta2;
    doc["isMoving"] = state.isMoving;
    doc["isHomed"] = state.isHomed;
    
    String json;
    serializeJson(doc, json);
    
    ws->textAll(json);
}

void WebServer::cleanup() {
    if (ws) {
        ws->cleanupClients();
    }
}
