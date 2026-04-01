#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "../core/Types.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


/**
 * @file WebServer.h
 * @brief Web server and WebSocket handling for robot control
 *
 * Handles HTTP requests and WebSocket connections for the web UI.
 * Parses incoming commands and pushes them to the command queue.
 *
 * Buffer Feedback: Reports available command queue slots via WebSocket
 * so the host can drip-feed commands without overflowing ESP32 RAM.
 */

class WebServer {
private:
  AsyncWebServer *server;
  AsyncWebSocket *ws;
  uint32_t processedCount; // Tracks total WebSocket messages processed

  // Queue references (FreeRTOS queues)
  QueueHandle_t commandQueue;
  QueueHandle_t motionQueue; // For reporting motion queue status

  // WebSocket message handler
  void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                        AwsEventType type, void *arg, uint8_t *data,
                        size_t len);

  // HTTP route handlers
  void handleRoot(AsyncWebServerRequest *request);
  void handleMove(AsyncWebServerRequest *request);
  void handleHome(AsyncWebServerRequest *request);
  void handleStatus(AsyncWebServerRequest *request);

  // Parse JSON command
  bool parseCommand(const String &json, Command &cmd);

  // Send to all connected clients that can accept messages (prevents queue
  // overflow)
  void safeTextAll(const String &message);

public:
  WebServer();
  ~WebServer();

  /**
   * @brief Initialize the web server
   * @param cmdQueue FreeRTOS queue handle for incoming commands
   * @param motQueue FreeRTOS queue handle for motion queue (for status
   * reporting)
   */
  void init(QueueHandle_t cmdQueue, QueueHandle_t motQueue = nullptr);

  /**
   * @brief Start the web server (call after WiFi is connected)
   */
  void begin();

  /**
   * @brief Load a batch of commands from a JSON string and enqueue them.
   * @param json JSON content (array or object with "commands" array)
   * @return true if at least one command enqueued successfully
   */
  bool loadCommandsFromJson(const String &json);

  /**
   * @brief Load a JSON command file from SPIFFS, parse and enqueue commands.
   * @param path SPIFFS path to JSON file, e.g. "/commands.json"
   * @return true if file read + at least one command enqueued
   */
  bool loadCommandsFromSPIFFS(const char *path);

  /**
   * @brief Broadcast robot status to all WebSocket clients
   * @param state Current robot state
   */
  void broadcastStatus(const RobotState &state);

  /**
   * @brief Broadcast buffer availability to all WebSocket clients
   *
   * Sends: {"type":"BUFFER","cmdFree":<n>,"motFree":<m>}
   * The host uses this to know when it can send more commands.
   *
   * @param cmdFreeSlots Number of free slots in the command queue
   */
  void broadcastBufferStatus(int cmdFreeSlots);

  /**
   * @brief Cleanup WebSocket clients (call in loop)
   */
  void cleanup();
};

#endif // WEB_SERVER_H
