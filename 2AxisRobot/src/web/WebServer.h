#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "../core/Types.h"

/**
 * @file WebServer.h
 * @brief Web server and WebSocket handling for robot control
 * 
 * Handles HTTP requests and WebSocket connections for the web UI.
 * Parses incoming commands and pushes them to the command queue.
 */

class WebServer {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    
    // Command queue reference (FreeRTOS queue)
    QueueHandle_t commandQueue;
    
    // WebSocket message handler
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    // HTTP route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleMove(AsyncWebServerRequest* request);
    void handleHome(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    
    // Parse JSON command
    bool parseCommand(const String& json, Command& cmd);
    
public:
    WebServer();
    ~WebServer();
    
    /**
     * @brief Initialize the web server
     * @param cmdQueue FreeRTOS queue handle for incoming commands
     */
    void init(QueueHandle_t cmdQueue);
    
    /**
     * @brief Start the web server (call after WiFi is connected)
     */
    void begin();
    
    /**
     * @brief Broadcast robot status to all WebSocket clients
     * @param state Current robot state
     */
    void broadcastStatus(const RobotState& state);
    
    /**
     * @brief Cleanup WebSocket clients (call in loop)
     */
    void cleanup();
};

#endif // WEB_SERVER_H
