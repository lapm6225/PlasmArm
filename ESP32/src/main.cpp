/**
 * @file main.cpp
 * @brief ESP32 SCARA Robot Controller - Main Entry Point
 *
 * This file sets up FreeRTOS tasks, queues, and hardware initialization
 * for the 2-DOF SCARA robotic arm controller with synchronized 4D motion.
 *
 * Core Assignment:
 * - Core 0: Web Server, Trajectory Planner
 * - Core 1: Real-Time Motion Control
 *
 * Motion Pipeline:
 *   WebSocket -> commandQueue(30) -> Planner -> motionQueue(1000) ->
 * MotionControl (4D interpolation)               (IK + motors)
 */

// modification king_boubou

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


#include "Config.h"
#include "core/Kinematics.h"
#include "core/Planner.h"
#include "core/Types.h"
#include "hardware/IMotor.h"
#include "hardware/ServoMotor.h"
#include "hardware/StepperMotor.h"
#include "web/WebServer.h"
#include <queue> // For std::queue in planner task


// Test mode includes
#if RUN_UNIT_TESTS || RUN_VISUAL_TESTS || RUN_INTERACTIVE_TEST
#include "test/RunTests.h"
#endif

// ============================================================================
// Global Objects
// ============================================================================
Kinematics kinematics(ARM_LENGTH_1, ARM_LENGTH_2);
Planner planner(DEFAULT_SPEED, ACCELERATION);
RobotState robotState;

// Motor instances (using StepperMotor by default, can be changed to ServoMotor)
IMotor *motor1 = nullptr;
IMotor *motor2 = nullptr;

WebServer webServer;

// ============================================================================
// FreeRTOS Queues
// ============================================================================
QueueHandle_t commandQueue; // Commands from web interface
QueueHandle_t motionQueue;  // Interpolated 4D motion points (TargetState)

// ============================================================================
// FreeRTOS Task Handles
// ============================================================================
TaskHandle_t taskWebHandlerHandle = nullptr;
TaskHandle_t taskPlannerHandle = nullptr;
TaskHandle_t taskMotionControlHandle = nullptr;

// ============================================================================
// Task Function Prototypes
// ============================================================================
void taskWebHandler(void *parameter);
void taskTrajectoryPlanner(void *parameter);
void taskMotionControl(void *parameter);

// ============================================================================
// Setup Function
// ============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

#if RUN_INTERACTIVE_TEST
  // Run interactive integration test with real motors
  runInteractiveTest();
  return; // Exit setup - don't initialize robot
#endif

#if RUN_VISUAL_TESTS
  // Run visual tests only (detailed output)
  runVisualTestsOnly();
  return; // Exit setup - don't initialize robot
#endif

#if RUN_UNIT_TESTS
  // Run unit tests instead of normal operation
  runAllUnitTests();
  return; // Exit setup - don't initialize robot
#endif

  Serial.println(
      "\n\n============================================================");
  Serial.println("ESP32 SCARA Robot Controller");
  Serial.println("  Synchronized 4D Motion (X,Y,Z,Tool)");
  Serial.println(
      "============================================================\n");

  // Initialize hardware
  Serial.println("Initializing hardware...");

  // Create motor instances (StepperMotor implementation)
  motor1 = new StepperMotor(MOTOR1_STEP_PIN, MOTOR1_DIR_PIN, MOTOR1_ENABLE_PIN);
  motor2 = new StepperMotor(MOTOR2_STEP_PIN, MOTOR2_DIR_PIN, MOTOR2_ENABLE_PIN);

  // Initialize motors
  motor1->init();
  motor2->init();
  motor1->enable();
  motor2->enable();

  Serial.println("Motors initialized");

  // Tool/Z GPIO setup
  pinMode(TOOL_PIN, OUTPUT);
  digitalWrite(TOOL_PIN, LOW);
  Serial.println("Tool GPIO initialized (pin " + String(TOOL_PIN) + ")");

  // Create FreeRTOS queues
  commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(Command));
  motionQueue = xQueueCreate(MOTION_QUEUE_SIZE, sizeof(TargetState));

  Serial.printf("Queues created:\n");
  Serial.printf("  commandQueue: %d slots (%d bytes each)\n",
                COMMAND_QUEUE_SIZE, sizeof(Command));
  Serial.printf("  motionQueue:  %d slots (%d bytes each)\n", MOTION_QUEUE_SIZE,
                sizeof(TargetState));

  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Initialize web server with both queues
    webServer.init(commandQueue, motionQueue);
    webServer.begin();
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("Continuing without web interface...");
  }

  // Create FreeRTOS tasks

  // Task 1: Web Handler (Core 0, Low Priority)
  xTaskCreatePinnedToCore(taskWebHandler, "WebHandler",
                          TASK_WEB_HANDLER_STACK_SIZE, nullptr,
                          TASK_WEB_HANDLER_PRIORITY, &taskWebHandlerHandle,
                          0 // Core 0
  );

  // Task 2: Trajectory Planner (Core 0, Medium Priority)
  xTaskCreatePinnedToCore(taskTrajectoryPlanner, "Planner",
                          TASK_PLANNER_STACK_SIZE, nullptr,
                          TASK_PLANNER_PRIORITY, &taskPlannerHandle,
                          0 // Core 0
  );

  // Task 3: Motion Control (Core 1, High Priority)
  xTaskCreatePinnedToCore(
      taskMotionControl, "MotionControl", TASK_MOTION_CONTROL_STACK_SIZE,
      nullptr, TASK_MOTION_CONTROL_PRIORITY, &taskMotionControlHandle,
      1 // Core 1
  );

  Serial.println("\nFreeRTOS tasks created:");
  Serial.println("  - WebHandler (Core 0, Priority 1)");
  Serial.println("  - Planner (Core 0, Priority 2)");
  Serial.println("  - MotionControl (Core 1, Priority 3)");
  Serial.printf("\nInitial position: (%.1f, %.1f) z=%.1f tool=%s\n",
                robotState.currentPosition.x, robotState.currentPosition.y,
                robotState.toolZ, robotState.toolActive ? "ON" : "OFF");
  Serial.println("\nSystem ready!\n");
}

// ============================================================================
// Loop Function (not used in FreeRTOS, but kept for compatibility)
// ============================================================================
void loop() {
  // FreeRTOS tasks handle everything
  // This loop can be used for low-priority maintenance tasks
  webServer.cleanup();
  delay(1000);
}

// ============================================================================
// Task Implementations
// ============================================================================

/**
 * Task A: Web Handler
 * Core: 0
 * Priority: Low
 *
 * Handles HTTP requests and WebSockets.
 * Parses incoming commands and pushes them to commandQueue.
 */
void taskWebHandler(void *parameter) {
  Serial.println("Task WebHandler started on Core 0");

  while (true) {
    // Web server handles requests asynchronously
    // This task mainly serves as a placeholder for future synchronous
    // operations

    // Cleanup WebSocket clients periodically
    webServer.cleanup();

    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
  }
}

/**
 * Task B: Trajectory Planner
 * Core: 0
 * Priority: Medium
 *
 * Blocks waiting for commands in commandQueue.
 * Performs 4D interpolation (X, Y, Z, Tool) and pushes TargetState to
 * motionQueue.
 */
void taskTrajectoryPlanner(void *parameter) {
  Serial.println("Task Planner started on Core 0");

  Command cmd;
  TargetState currentState(robotState.currentPosition.x,
                           robotState.currentPosition.y, robotState.toolZ,
                           robotState.toolActive);

  unsigned long lastBufferBroadcast = 0;
  const unsigned long BUFFER_BROADCAST_INTERVAL_MS =
      500; // Rate-limit to 2/sec max

  while (true) {
    // Wait for command from queue (blocking)
    if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY) == pdTRUE) {

      // Rate-limited buffer status broadcast (prevents WS queue overflow)
      unsigned long now = millis();
      if (now - lastBufferBroadcast >= BUFFER_BROADCAST_INTERVAL_MS) {
        lastBufferBroadcast = now;
        int freeSlots = uxQueueSpacesAvailable(commandQueue);
        webServer.broadcastBufferStatus(freeSlots);
      }

      switch (cmd.type) {
      case Command::MOVE_TO: {
        // Build target state from command
        TargetState target(cmd.x, cmd.y, cmd.z, cmd.toolState);

        Serial.printf("Planner: MOVE_TO (%.2f, %.2f) z=%.2f spd=%.1f tool=%s\n",
                      cmd.x, cmd.y, cmd.z, cmd.speed,
                      cmd.toolState ? "ON" : "OFF");

        // Check if XY target is reachable
        Point2D targetXY = target.toPoint2D();
        if (!kinematics.isReachable(targetXY)) {
          Serial.printf("Planner: UNREACHABLE! (%.2f, %.2f)\n", target.x,
                        target.y);
          break;
        }

        // Set planner speed if provided
        float moveSpeed = (cmd.speed > 0) ? cmd.speed : DEFAULT_SPEED;
        planner.setSpeed(moveSpeed);

        // Generate XY interpolated points via existing planner
        Point2D startXY = currentState.toPoint2D();
        std::queue<Point2D> xyQueue;
        int numPoints = planner.planPath(startXY, targetXY, xyQueue);

        Serial.printf("Planner:   -> %d interpolation points\n", numPoints);

        // 4D interpolation: distribute Z and tool changes across the path
        int i = 0;
        while (!xyQueue.empty()) {
          Point2D pt = xyQueue.front();
          xyQueue.pop();

          float t = (numPoints > 1) ? (float)i / (numPoints - 1) : 1.0f;
          float z = currentState.z + t * (target.z - currentState.z);
          bool tool = (t >= 0.5f) ? target.toolActive : currentState.toolActive;

          TargetState point(pt.x, pt.y, z, tool);
          if (xQueueSend(motionQueue, &point, pdMS_TO_TICKS(100)) != pdTRUE) {
            Serial.println("Planner: WARNING - Motion queue full!");
          }
          i++;
        }

        // Update current state
        currentState = target;
        robotState.currentPosition = targetXY;
        robotState.toolZ = target.z;
        robotState.toolActive = target.toolActive;
        break;
      }

      case Command::TOOL_CONTROL: {
        Serial.printf("Planner: TOOL %s z=%.2f\n", cmd.toolState ? "ON" : "OFF",
                      cmd.z);
        // Immediate tool/Z change (single point)
        TargetState point(currentState.x, currentState.y, cmd.z, cmd.toolState);
        xQueueSend(motionQueue, &point, portMAX_DELAY);
        currentState.z = cmd.z;
        currentState.toolActive = cmd.toolState;
        robotState.toolZ = cmd.z;
        robotState.toolActive = cmd.toolState;
        break;
      }

      case Command::HOME: {
        Serial.println("Planner: HOME -> moving to (0, 0, 0) tool=OFF");
        Point2D homePos(0, 0);
        Point2D startXY = currentState.toPoint2D();
        std::queue<Point2D> xyQueue;
        planner.planPath(startXY, homePos, xyQueue);

        int numPts = xyQueue.size();
        int i = 0;
        while (!xyQueue.empty()) {
          Point2D pt = xyQueue.front();
          xyQueue.pop();
          float t = (numPts > 1) ? (float)i / (numPts - 1) : 1.0f;
          float z = currentState.z * (1.0f - t);
          TargetState point(pt.x, pt.y, z, false);
          xQueueSend(motionQueue, &point, portMAX_DELAY);
          i++;
        }

        currentState = TargetState(0, 0, 0, false);
        robotState.currentPosition = homePos;
        robotState.toolZ = 0;
        robotState.toolActive = false;
        robotState.isHomed = true;
        Serial.printf("Planner:   -> %d interpolation points\n", numPts);
        Serial.println("Planner: HOME complete");
        break;
      }

      case Command::STOP: {
        Serial.println("Planner: STOP! Clearing motion queue, tool OFF");
        xQueueReset(motionQueue);
        // Safety: turn tool OFF on stop
        TargetState safeState(currentState.x, currentState.y, 0, false);
        xQueueSend(motionQueue, &safeState, portMAX_DELAY);
        currentState.toolActive = false;
        currentState.z = 0;
        robotState.isMoving = false;
        robotState.toolActive = false;
        robotState.toolZ = 0;
        break;
      }

      case Command::SET_SPEED: {
        Serial.printf("Planner: SET_SPEED %.1f mm/s\n", cmd.speed);
        planner.setSpeed(cmd.speed);
        break;
      }

      default:
        Serial.printf("Planner: Unknown command type %d\n", cmd.type);
        break;
      }
    }
  }
}

/**
 * Task C: Motion Control (Critical Loop)
 * Core: 1
 * Priority: High (Real-time)
 * Frequency: 100 Hz (10ms loop)
 *
 * Pops TargetState from motionQueue, calculates inverse kinematics,
 * commands motors, and actuates Z-axis + tool GPIO.
 */
void taskMotionControl(void *parameter) {
  Serial.println("Task MotionControl started on Core 1");

  const TickType_t loopDelay = pdMS_TO_TICKS(1000 / MOTION_CONTROL_FREQUENCY);
  TargetState target;
  JointAngles targetAngles;

  // Status broadcast throttle (500ms = 2/sec)
  static unsigned long lastStatusBroadcast = 0;
  const unsigned long STATUS_BROADCAST_INTERVAL_MS = 500;

  while (true) {
    TickType_t lastWakeTime = xTaskGetTickCount();

    // Check motion queue
    if (xQueueReceive(motionQueue, &target, 0) == pdTRUE) {
      // New target point received
      robotState.isMoving = true;

      // Calculate inverse kinematics for XY
      Point2D targetXY = target.toPoint2D();
      if (kinematics.inverse(targetXY, targetAngles)) {
        // Command motors to move to target angles
        motor1->moveToAngle(targetAngles.theta1);
        motor2->moveToAngle(targetAngles.theta2);

        robotState.currentAngles = targetAngles;
        robotState.currentPosition = targetXY;

        // Actuate Z-axis (placeholder - adapt to your Z hardware)
        robotState.toolZ = target.z;

        // Actuate tool GPIO
        robotState.toolActive = target.toolActive;
        digitalWrite(TOOL_PIN, target.toolActive ? HIGH : LOW);

#if DEBUG_MOTOR
        Serial.printf("Motion: Target (%.2f, %.2f) -> t1=%.2f, t2=%.2f\n",
                      targetXY.x, targetXY.y, targetAngles.theta1,
                      targetAngles.theta2);
#endif
      } else {
        Serial.printf("Motion: IK failed for (%.2f, %.2f)\n", targetXY.x,
                      targetXY.y);
      }
    } else {
      // No new target, maintain current position
      robotState.isMoving = (motor1->isMoving() || motor2->isMoving());
    }

    // Update motors (generate steps for steppers, update PWM for servos)
    motor1->update();
    motor2->update();

    // Broadcast status + position log (throttled to 2/sec)
    unsigned long now = millis();
    if (now - lastStatusBroadcast >= STATUS_BROADCAST_INTERVAL_MS) {
      lastStatusBroadcast = now;
      webServer.broadcastStatus(robotState);

      // Verbose position logging to serial
      int cmdUsed = COMMAND_QUEUE_SIZE - uxQueueSpacesAvailable(commandQueue);
      int motUsed = MOTION_QUEUE_SIZE - uxQueueSpacesAvailable(motionQueue);
      Serial.printf(
          "Motion: pos(%.1f, %.1f) z=%.1f tool=%s | cmdQ=%d/%d motQ=%d/%d\n",
          robotState.currentPosition.x, robotState.currentPosition.y,
          robotState.toolZ, robotState.toolActive ? "ON" : "OFF", cmdUsed,
          COMMAND_QUEUE_SIZE, motUsed, MOTION_QUEUE_SIZE);
    }

    // Fixed frequency loop
    vTaskDelayUntil(&lastWakeTime, loopDelay);
  }
}

// ============================================================================
// Notes on Stack Size Tuning
// ============================================================================
/*
 * Stack Size Tuning Guidelines:
 *
 * 1. Start with the default values in Config.h
 * 2. Monitor stack usage using FreeRTOS functions:
 *    - uxTaskGetStackHighWaterMark() - returns minimum free stack
 *    - Add this to each task to monitor:
 *      UBaseType_t stack = uxTaskGetStackHighWaterMark(nullptr);
 *      Serial.printf("Task stack free: %d bytes\n", stack *
 * sizeof(StackType_t));
 *
 * 3. If stack overflow occurs (watchdog reset or crash):
 *    - Increase stack size in Config.h
 *    - Common causes: large local arrays, deep recursion, large strings
 *
 * 4. Typical stack sizes:
 *    - WebHandler: 8-16 KB (handles HTTP/WebSocket buffers)
 *    - Planner: 4-8 KB (queue operations, math calculations)
 *    - MotionControl: 4-8 KB (motor updates, kinematics)
 *
 * 5. ESP32 has ~520 KB total RAM, so be mindful of total stack usage
 */
