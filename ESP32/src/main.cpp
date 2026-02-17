/**
 * @file main.cpp
 * @brief ESP32 SCARA Robot Controller - Main Entry Point
 *
 * This file sets up FreeRTOS tasks, queues, and hardware initialization
 * for the 2-DOF SCARA robotic arm controller with synchronized tool control.
 *
 * Core Assignment:
 * - Core 0: Web Server, Trajectory Planner
 * - Core 1: Real-Time Motion Control
 *
 * Data Flow:
 *   Host/WebSocket → commandQueue (Command) → Planner → motionQueue
 * (TargetState) → MotionControl MotionControl → IK(X,Y) → Motors, Z-Servo, Tool
 * GPIO
 */

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
QueueHandle_t commandQueue; // Commands from web interface → Planner
QueueHandle_t motionQueue;  // Interpolated TargetState points → MotionControl

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
// Hardware: Tool & Z-axis helpers
// ============================================================================
static void initToolAndZ() {
  // Tool relay pin
  pinMode(TOOL_PIN, OUTPUT);
  digitalWrite(TOOL_PIN, LOW); // Tool OFF at startup

  // Z-axis servo pin (placeholder — configure as needed for your hardware)
  pinMode(Z_AXIS_PIN, OUTPUT);
  digitalWrite(Z_AXIS_PIN, LOW);
}

static void setToolState(bool active) {
  digitalWrite(TOOL_PIN, active ? HIGH : LOW);
}

static void setZPosition(float z) {
  // Basic servo mapping: Z position (mm) → PWM value
  // TODO: Replace with proper servo library call or stepper control
  // For now, map Z range to servo pulse range
  float clamped = constrain(z, Z_AXIS_MIN, Z_AXIS_MAX);
  int pwm = map((int)(clamped * 100), (int)(Z_AXIS_MIN * 100),
                (int)(Z_AXIS_MAX * 100), SERVO_MIN_PULSE, SERVO_MAX_PULSE);
  // Write to servo — replace with actual servo library call
  // servo.writeMicroseconds(pwm);
  (void)pwm; // Suppress unused warning until servo is wired
}

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

  Serial.println("\n\n========================================");
  Serial.println("ESP32 SCARA Robot Controller");
  Serial.println("  Synchronized 4D Motion (X,Y,Z,Tool)");
  Serial.println("========================================\n");

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

  // Initialize tool and Z-axis
  initToolAndZ();

  Serial.println("Motors + Tool + Z-axis initialized");

  // Create FreeRTOS queues
  // Note: motionQueue now carries TargetState (4D) instead of Point2D
  commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(Command));
  motionQueue = xQueueCreate(MOTION_QUEUE_SIZE, sizeof(TargetState));

  Serial.println("Queues created");
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

    // Initialize web server with both queues (for buffer feedback)
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
  Serial.println("\nSystem ready!\n");
}

// ============================================================================
// Loop Function (not used in FreeRTOS, but kept for compatibility)
// ============================================================================
void loop() {
  // FreeRTOS tasks handle everything
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
 * Converts Commands to interpolated TargetState stream in motionQueue.
 *
 * This is the bridge between high-level commands and the real-time
 * motion control loop. It handles:
 *   - MOVE_TO: interpolate path, push TargetState points
 *   - TOOL_CONTROL: push a single TargetState with updated tool/Z
 *   - HOME: interpolate path back to origin
 *   - SET_SPEED: update planner speed
 *   - STOP: clear motion queue
 */
void taskTrajectoryPlanner(void *parameter) {
  Serial.println("Task Planner started on Core 0");

  Command cmd;
  // Track current state as TargetState (full 4D)
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
        if (cmd.speed > 0) {
          planner.setSpeed(cmd.speed);
        }

        // Generate interpolated 4D points
        std::queue<TargetState> localQueue;
        int numPoints = planner.planPath(currentState, target, localQueue);

        Serial.printf("Planner:   -> %d interpolation points\n", numPoints);

        // Push all points to motion queue
        while (!localQueue.empty()) {
          TargetState point = localQueue.front();
          localQueue.pop();

          if (xQueueSend(motionQueue, &point, pdMS_TO_TICKS(100)) != pdTRUE) {
            Serial.println("Planner: WARNING - Motion queue full!");
          }
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
        // Tool state change without movement
        // Update current state's tool and Z, push a single point
        currentState.toolActive = cmd.toolState;
        currentState.z = cmd.z;

        // Push one TargetState so MotionControl applies the change
        if (xQueueSend(motionQueue, &currentState, pdMS_TO_TICKS(100)) !=
            pdTRUE) {
          Serial.println("Planner: WARNING - Motion queue full!");
        }

        robotState.toolActive = cmd.toolState;
        robotState.toolZ = cmd.z;
        break;
      }

      case Command::HOME: {
        Serial.println("Planner: HOME -> moving to (0, 0, 0) tool=OFF");
        // Home sequence: move to origin with tool OFF
        TargetState homeState(0, 0, 0, false);
        std::queue<TargetState> localQueue;
        int numPoints = planner.planPath(currentState, homeState, localQueue);

        Serial.printf("Planner:   -> %d interpolation points\n", numPoints);

        while (!localQueue.empty()) {
          TargetState point = localQueue.front();
          localQueue.pop();
          xQueueSend(motionQueue, &point, portMAX_DELAY);
        }

        currentState = homeState;
        robotState.currentPosition = Point2D(0, 0);
        robotState.toolZ = 0;
        robotState.toolActive = false;
        robotState.isHomed = true;
        Serial.println("Planner: HOME complete");
        break;
      }

      case Command::STOP: {
        Serial.println("Planner: STOP! Clearing motion queue, tool OFF");
        // Clear motion queue
        xQueueReset(motionQueue);
        robotState.isMoving = false;
        robotState.toolActive = false;
        currentState.toolActive = false;
        setToolState(false); // Safety: turn tool OFF on stop
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
 * Pops TargetState from motionQueue, calculates inverse kinematics (XY),
 * commands XY motors, sets Z position, and actuates tool GPIO.
 */
void taskMotionControl(void *parameter) {
  Serial.println("Task MotionControl started on Core 1");

  const TickType_t loopDelay = pdMS_TO_TICKS(1000 / MOTION_CONTROL_FREQUENCY);
  TargetState target;
  JointAngles targetAngles;

  while (true) {
    TickType_t lastWakeTime = xTaskGetTickCount();

    // Check motion queue for the next 4D target
    if (xQueueReceive(motionQueue, &target, 0) == pdTRUE) {
      // New target point received
      robotState.isMoving = true;

      // 1. Inverse Kinematics for X,Y
      Point2D xyTarget = target.toPoint2D();
      if (kinematics.inverse(xyTarget, targetAngles)) {
        // Command XY motors
        motor1->moveToAngle(targetAngles.theta1);
        motor2->moveToAngle(targetAngles.theta2);

        robotState.currentAngles = targetAngles;
        robotState.currentPosition = xyTarget;
      } else {
        Serial.printf("Motion: IK failed for (%.2f, %.2f)\n", target.x,
                      target.y);
      }

      // 2. Z-axis
      setZPosition(target.z);
      robotState.toolZ = target.z;

      // 3. Tool GPIO
      setToolState(target.toolActive);
      robotState.toolActive = target.toolActive;

#if DEBUG_MOTOR
      Serial.printf("Motion: (%.2f,%.2f,%.2f) T=%d -> θ1=%.2f° θ2=%.2f°\n",
                    target.x, target.y, target.z, target.toolActive,
                    targetAngles.theta1, targetAngles.theta2);
#endif
    } else {
      // No new target, maintain current position
      robotState.isMoving = (motor1->isMoving() || motor2->isMoving());
    }

    // Update motors (generate steps for steppers, update PWM for servos)
    motor1->update();
    motor2->update();

    // Periodic logging & status broadcast (every 50 loops = 500ms at 100Hz)
    static int statusCounter = 0;
    if (++statusCounter >= 50) {
      statusCounter = 0;

      // Serial monitor: print current position + queue state
      int cmdUsed = COMMAND_QUEUE_SIZE - uxQueueSpacesAvailable(commandQueue);
      int motUsed = MOTION_QUEUE_SIZE - uxQueueSpacesAvailable(motionQueue);
      Serial.printf(
          "Motion: pos(%.1f, %.1f) z=%.1f tool=%s | cmdQ=%d/%d motQ=%d/%d\n",
          robotState.currentPosition.x, robotState.currentPosition.y,
          robotState.toolZ, robotState.toolActive ? "ON" : "OFF", cmdUsed,
          COMMAND_QUEUE_SIZE, motUsed, MOTION_QUEUE_SIZE);

      // WebSocket broadcast
      webServer.broadcastStatus(robotState);
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
