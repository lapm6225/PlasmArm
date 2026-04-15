/**
 * @file main.cpp
 * @brief ESP32 SCARA Robot Controller - G-Code Style State Machine
 *
 * Professional G-code controller (GRBL/Marlin style) using FreeRTOS
 * dual-core architecture:
 *
 *   Core 0: Web Server (HTTP, WebSocket, ACK)
 *   Core 1: G-Code State Machine (100Hz loop)
 *
 * Motion Pipeline:
 *   WebSocket -> commandQueue(30) -> [STATE MACHINE on Core 1]
 *                                     ↓
 *                                     IDLE / EXECUTING / TOOL_ACTUATING / DELAYING
 *                                     ↓
 *                                     syncWriteAngles -> Dynamixel motors
 *
 * Commands are processed SEQUENTIALLY. Each command must complete (motion
 * finished, tool actuated, delay elapsed) before the next one starts.
 * This guarantees TOOL_DOWN completes before the next MOVE_TO begins.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "Config.h"
#include "core/Kinematics.h"
#include "core/Planner.h"
#include "core/Types.h"

#include "hardware/DynamixelController.h"
#include "web/WebServer.h"
#include "hardware/SG90.h"
#include <queue>  // For std::queue in state machine

// Test mode includes
#if RUN_UNIT_TESTS || RUN_VISUAL_TESTS || RUN_INTERACTIVE_TEST || RUN_INTERACTIVE_TEST2 || \
    RUN_DYNAMIXEL_COMM_TEST || RUN_EFFECTOR_TEST
#include "test/RunTests.h"
#endif

// ============================================================================
// Global Objects
// ============================================================================
Kinematics kinematics(ARM_LENGTH_1, ARM_LENGTH_2);
Planner planner(DEFAULT_SPEED, ACCELERATION);
RobotState robotState;

// Motor instance
DynamixelController* dxlCtrl = nullptr;

WebServer webServer;
SG90 toolServo;  // Global servo instance

// ============================================================================
// FreeRTOS Queues
// ============================================================================
QueueHandle_t commandQueue;  // Commands from web interface (30 slots)

// ============================================================================
// Stop flag (set by STOP command, checked by state machine)
// ============================================================================
volatile bool stopRequested = false;

// ============================================================================
// FreeRTOS Task Handles
// ============================================================================
TaskHandle_t taskWebHandlerHandle = nullptr;
TaskHandle_t taskStateMachineHandle = nullptr;

// ============================================================================
// Task Function Prototypes
// ============================================================================
void taskWebHandler(void* parameter);
void taskStateMachine(void* parameter);

// ============================================================================
// Setup Function
// ============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial2.begin(SERIAL_OPENRB_BAUD_RATE, SERIAL_8N1, RXD2, TXD2);
    delay(1000);

#if RUN_INTERACTIVE_TEST
    runInteractiveTest();
    return;
#endif

#if RUN_INTERACTIVE_TEST2
    runInteractiveTest2();
    return;
#endif

#if RUN_DYNAMIXEL_COMM_TEST
    runDynamixelCommTest();
    return;
#endif

#if RUN_EFFECTOR_TEST
    runEffectorTest();
    return;
#endif

#if RUN_VISUAL_TESTS
    runVisualTestsOnly();
    return;
#endif

#if RUN_UNIT_TESTS
    runAllUnitTests();
    return;
#endif

    Serial.println("\n\n============================================================");
    Serial.println("ESP32 SCARA Robot Controller — G-Code State Machine");
    Serial.println("  Sequential Command Processing (GRBL/Marlin style)");
    Serial.println("============================================================");
    Serial.printf("  Arms:      L1=%.0fmm  L2=%.0fmm\n", ARM_LENGTH_1, ARM_LENGTH_2);
    Serial.printf("  Workspace: r=[%.0f, %.0f]mm\n", WORKSPACE_R_MIN, WORKSPACE_R_MAX);
    Serial.printf("  Theta1:    [%.0f, %.0f] deg\n", THETA1_MIN, THETA1_MAX);
    Serial.printf("  Theta2:    [%.0f, %.0f] deg\n", THETA2_MIN, THETA2_MAX);
    Serial.printf("  Home:      (%.0f, %.0f)\n", HOME_X, HOME_Y);
    Serial.println("============================================================\n");

    // Initialize hardware
    Serial.println("Initializing hardware...");
    dxlCtrl = new DynamixelController(Serial2);
    dxlCtrl->init();

    // Load home offsets from NVS (flash persistence)
    Preferences prefs;
    prefs.begin("plasmarm", true);  // read-only
    float savedOff1 = prefs.getFloat("off1", -1.0f);
    float savedOff2 = prefs.getFloat("off2", -1.0f);
    prefs.end();
    if (savedOff1 >= 0.0f && savedOff2 >= 0.0f) {
        dxlCtrl->setOffsets(savedOff1, savedOff2);
        Serial.printf("Home offsets loaded from NVS: (%.2f, %.2f)\n", savedOff1, savedOff2);
    } else {
        Serial.println("No saved home offsets, using defaults");
    }

    dxlCtrl->setTorque(true);
    Serial.println("Motors initialized");

    // Tool/Z GPIO setup
    toolServo = SG90(TOOL_SERVO_PIN, TOOL_SWITCH_PIN, 0);
    Serial.println("Tool servo initialized");

    // Create FreeRTOS queue
    commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(Command));
    Serial.printf("commandQueue: %d slots (%d bytes each)\n", COMMAND_QUEUE_SIZE, sizeof(Command));

    // Connect to WiFi or create AP
    if (WIFI_AP_MODE) {
        Serial.print("Creating Access Point: ");
        Serial.println(WIFI_AP_SSID);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
        Serial.println("\nAccess Point started!");
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());
        webServer.init(commandQueue);
        webServer.begin();
    } else {
        Serial.print("Connecting to WiFi: ");
        Serial.println(WIFI_STA_SSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);
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
            webServer.init(commandQueue);
            webServer.begin();
        } else {
            Serial.println("\nWiFi connection failed!");
        }
    }

    // Create FreeRTOS tasks

    // Task 1: Web Handler (Core 0, Low Priority)
    xTaskCreatePinnedToCore(taskWebHandler, "WebHandler", TASK_WEB_HANDLER_STACK_SIZE, nullptr,
                            TASK_WEB_HANDLER_PRIORITY, &taskWebHandlerHandle,
                            0  // Core 0
    );

    // Task 2: G-Code State Machine (Core 1, Medium Priority)
    xTaskCreatePinnedToCore(taskStateMachine, "GCodeSM", TASK_PLANNER_STACK_SIZE * 2, nullptr,
                            TASK_PLANNER_PRIORITY, &taskStateMachineHandle,
                            1  // Core 1
    );

    Serial.println("\nFreeRTOS tasks created:");
    Serial.println("  - WebHandler (Core 0, Priority 1)");
    Serial.println("  - GCodeSM   (Core 1, Priority 2) — 100Hz state machine");
    Serial.printf("\nInitial position: (%.1f, %.1f) z=%.1f tool=%s\n", robotState.currentPosition.x,
                  robotState.currentPosition.y, robotState.toolZ,
                  robotState.toolActive ? "ON" : "OFF");
    Serial.println("\nSystem ready!\n");
}

// ============================================================================
// Loop Function (not used — FreeRTOS tasks handle everything)
// ============================================================================
void loop() {
    webServer.cleanup();
    delay(1000);
}

// ============================================================================
// Task A: Web Handler (Core 0)
// ============================================================================
void taskWebHandler(void* parameter) {
    Serial.println("Task WebHandler started on Core 0");

    unsigned long lastStatusBroadcast = 0;
    const unsigned long STATUS_BROADCAST_INTERVAL_MS = 500;

    while (true) {
        webServer.cleanup();

        unsigned long now = millis();
        if (now - lastStatusBroadcast >= STATUS_BROADCAST_INTERVAL_MS) {
            lastStatusBroadcast = now;
            webServer.broadcastStatus(robotState);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
// Task B: G-Code State Machine (Core 1, 100Hz)
// ============================================================================
void taskStateMachine(void* parameter) {
    Serial.println("Task GCodeSM started on Core 1 (100Hz)");

    // --- Timing setup ---
    const TickType_t loopDelay = pdMS_TO_TICKS(1000 / MOTION_CONTROL_FREQUENCY);
    TickType_t lastWakeTime = xTaskGetTickCount();

    // --- State machine ---
    PlannerState state = PlannerState::IDLE;

    // --- Delay tracking (like TestInteractive2) ---
    uint32_t delayStartTime = 0;
    uint32_t delayDuration = 0;

    // --- Tool actuation tracking (non-blocking) ---
    bool toolMovingDown = false;
    bool toolMovingUp = false;
    float toolTargetAngle = 0.0f;
    uint32_t toolActuateStartTime = 0;

    // --- Motion point buffer (internal, not a FreeRTOS queue) ---
    std::queue<TargetState> pointBuffer;
    std::queue<TargetState> transitBuffer; // For JOINT mode safe transit paths
    ArmConfig currentConfig = ArmConfig::RIGHT_ELBOW;

    // --- Config switch tracking ---
    ArmConfig targetConfig = currentConfig;  // Config to switch to
    bool configSwitchPending = false;        // Trigger config switch
    TargetState switchTarget;                // Store target for path regeneration
    bool restoreToolAfterSwitch = false;

    // --- Current position tracking (XY only — tool state lives in servo/robotState) ---
    TargetState currentState(robotState.currentPosition.x, robotState.currentPosition.y);

    // --- Serial logging ---
    static unsigned long lastStatusLog = 0;
    const unsigned long STATUS_LOG_INTERVAL_MS = 500;

    while (true) {
        // ====================================================================
        // 1. CHECK STOP REQUEST (from any task via volatile flag)
        // ====================================================================
        if (stopRequested) {
            stopRequested = false;
            state = PlannerState::IDLE;
            robotState.plannerState = PlannerState::IDLE;
            robotState.isMoving = false;
            robotState.toolActive = false;
            robotState.toolZ = 0;

            // Clear pending commands
            xQueueReset(commandQueue);

            // Drain internal buffers
            while (!pointBuffer.empty())
                pointBuffer.pop();
            while (!transitBuffer.empty())
                transitBuffer.pop();

            // Safety: disable tool
            if (dxlCtrl)
                dxlCtrl->update();

            Serial.println("SM: STOP! Cleared all queues, tool OFF, state -> IDLE");
        }

        // ====================================================================
        // 2. STATE: DELAYING — check if delay has elapsed
        // ====================================================================
        if (state == PlannerState::DELAYING) {
            uint32_t elapsed = millis() - delayStartTime;
            if (elapsed >= delayDuration) {
                Serial.printf("SM: DELAY complete (%lu ms)\n", (unsigned long)delayDuration);
                if (!transitBuffer.empty() || !pointBuffer.empty()) {
                    state = PlannerState::EXECUTING;
                    robotState.plannerState = PlannerState::EXECUTING;
                } else {
                    state = PlannerState::IDLE;
                    robotState.plannerState = PlannerState::IDLE;
                }
            }
        }

        // ====================================================================
        // 3. STATE: EXECUTING — send interpolated points at 100Hz
        // ====================================================================
        if (state == PlannerState::EXECUTING) {
            std::queue<TargetState>* activeBuffer = nullptr;
            if (!transitBuffer.empty()) activeBuffer = &transitBuffer;
            else if (!pointBuffer.empty()) activeBuffer = &pointBuffer;
            
            if (activeBuffer != nullptr) {
                TargetState target = activeBuffer->front();

                if (target.mode == MoveMode::JOINT) {
                    // Bypass IK, move dynamically in joint space
                    activeBuffer->pop();
                    if (dxlCtrl) dxlCtrl->syncWriteAngles(target.x, target.y);
                    
                    JointAngles targetAngles(target.x, target.y);
                    robotState.currentAngles = targetAngles;
                    
                    // Maintain Cartesian position correctly via Forward Kinematics for UI
                    Point2D fkXY;
                    kinematics.forward(targetAngles, fkXY);
                    robotState.currentPosition = fkXY;
                    robotState.isMoving = true;
                    currentState = target; 
                } else if (target.mode == MoveMode::DELAY_MS) {
                    activeBuffer->pop();
                    delayDuration = target.x;
                    delayStartTime = millis();
                    state = PlannerState::DELAYING;
                    robotState.plannerState = PlannerState::DELAYING;
                } else if (target.mode == MoveMode::TOOL_UP_ASYNC) {
                    activeBuffer->pop();
                    Serial.println("SM: Async TOOL_UP from transit buffer");
                    toolTargetAngle = 175.0f;
                    toolMovingDown = false;
                    toolMovingUp = true;
                    toolActuateStartTime = millis();
                    state = PlannerState::TOOL_ACTUATING;
                    robotState.plannerState = PlannerState::TOOL_ACTUATING;
                    robotState.isMoving = true;
                } else if (target.mode == MoveMode::TOOL_DOWN_ASYNC) {
                    activeBuffer->pop();
                    Serial.println("SM: Async TOOL_DOWN from transit buffer");
                    toolMovingDown = true;
                    toolMovingUp = false;
                    toolActuateStartTime = millis();
                    state = PlannerState::TOOL_ACTUATING;
                    robotState.plannerState = PlannerState::TOOL_ACTUATING;
                    robotState.isMoving = true;
                } else {
                    // Mode is CARTESIAN
                    Point2D targetXY = target.toPoint2D();
                    JointAngles targetAngles;
                    ArmConfig usedConfig;

                    bool ikSuccess = kinematics.inverse(targetXY, targetAngles, currentConfig, usedConfig);

                    if (!ikSuccess) {
                        Serial.printf("SM: IK failed for (%.2f, %.2f)\n", targetXY.x, targetXY.y);
                        activeBuffer->pop();
                    } else if (usedConfig != currentConfig) {
                        Serial.printf("SM: Config switch needed: %s -> %s at (%.1f, %.1f)\n",
                                      currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT" : "LEFT",
                                      usedConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT" : "LEFT",
                                      targetXY.x, targetXY.y);

                        targetConfig = usedConfig;
                        configSwitchPending = true;
                        // Check actual servo position — robotState.toolActive may have
                        // been overwritten to false by interpolated MOVE_TO points
                        restoreToolAfterSwitch = (toolServo.getAngle() < 150);

                        state = PlannerState::SWITCH_RAISE_TOOL;
                        robotState.plannerState = PlannerState::SWITCH_RAISE_TOOL;
                        robotState.isMoving = true;
                        Serial.printf("SM:   servoAngle=%d, restoreTool=%s\n",
                                      toolServo.getAngle(),
                                      restoreToolAfterSwitch ? "YES" : "NO");
                        // Note: We deliberately DO NOT pop the buffer here. The point stays at the front for later execution.
                    } else {
                        activeBuffer->pop();
                        if (dxlCtrl) {
                            dxlCtrl->syncWriteAngles(targetAngles.theta1, targetAngles.theta2);
                        }
                        robotState.currentAngles = targetAngles;
                        robotState.currentPosition = targetXY;
                        robotState.isMoving = true;
                        currentState = TargetState(targetXY.x, targetXY.y);
                    }
                }
            } else {
                // No more points — check if motors have settled
                if (dxlCtrl)
                    dxlCtrl->update();
                bool motorsMoving = dxlCtrl ? dxlCtrl->isMoving() : false;
                robotState.isMoving = motorsMoving;

                if (!motorsMoving) {
                    Serial.println("SM: MOVE complete, motors idle");
                    state = PlannerState::IDLE;
                    robotState.plannerState = PlannerState::IDLE;
                    robotState.isMoving = false;
                }
            }
        }

        // ====================================================================
        // 4. STATE: TOOL_ACTUATING — non-blocking servo motion
        // ====================================================================
        if (state == PlannerState::TOOL_ACTUATING) {
            Serial.printf("SM: TOOL_ACTUATING loop (down=%d, up=%d, angle=%d, switch=%d)\n",
                          toolMovingDown, toolMovingUp, toolServo.getAngle(),
                          digitalRead(TOOL_SWITCH_PIN));
            bool done = false;
            if (toolMovingDown) {
                done = toolServo.stepDown(TOOL_STEP_DEG);
                uint32_t elapsed = millis() - toolActuateStartTime;
                if (elapsed > TOOL_ACTUATE_TIMEOUT_MS) {
                    Serial.println("SM: TOOL_DOWN timeout!");
                    done = true;
                }
            } else if (toolMovingUp) {
                done = toolServo.stepUp(TOOL_STEP_DEG, toolTargetAngle);
            }

            if (done) {
                Serial.println("SM: TOOL actuation complete");
                bool wasDown = toolMovingDown;
                bool wasUp = toolMovingUp;
                toolMovingDown = false;
                toolMovingUp = false;
                if (wasDown) {
                    robotState.toolZ = 5.0f;
                    robotState.toolActive = true;
                } else if (wasUp) {
                    robotState.toolZ = 0.0f;
                    robotState.toolActive = false;
                }
                
                if (!transitBuffer.empty() || !pointBuffer.empty()) {
                    state = PlannerState::EXECUTING;
                    robotState.plannerState = PlannerState::EXECUTING;
                } else {
                    state = PlannerState::IDLE;
                    robotState.plannerState = PlannerState::IDLE;
                    robotState.isMoving = false;
                }
            }
        }

        // ====================================================================
        // 5. STATE: SWITCH_RAISE_TOOL — raise tool non-blockingly before switch
        // ====================================================================
        if (state == PlannerState::SWITCH_RAISE_TOOL) {
            if (restoreToolAfterSwitch) {
                // Initialize servo upward motion if not already moving
                if (!toolMovingUp) {
                    Serial.println("SM:   Tool is DOWN - raising before config switch");
                    toolTargetAngle = 175.0f;
                    toolMovingDown = false;
                    toolMovingUp = true;
                    toolActuateStartTime = millis();
                }
                
                bool done = toolServo.stepUp(TOOL_STEP_DEG, toolTargetAngle);
                if (done) {
                    toolMovingUp = false;
                    Serial.println("SM:   Tool raised");
                    robotState.toolZ = 0.0f;
                    robotState.toolActive = false;
                    
                    state = PlannerState::SWITCHING_CONFIG;
                    robotState.plannerState = PlannerState::SWITCHING_CONFIG;
                }
            } else {
                // Tool already up, proceed immediately
                state = PlannerState::SWITCHING_CONFIG;
                robotState.plannerState = PlannerState::SWITCHING_CONFIG;
            }
        }

        // ====================================================================
        // 5b. STATE: SWITCHING_CONFIG — enqueue safe transit poses
        // ====================================================================
        if (state == PlannerState::SWITCHING_CONFIG) {
            ArmConfig newConfig = targetConfig;
            
            // The trigger point is at pointBuffer.front() — the transit will
            // deliver the arm to this exact position, so we pop it afterwards.
            TargetState nextTarget = pointBuffer.front();
            Point2D targetXY = nextTarget.toPoint2D();
            JointAngles switchAngles;
            
            if (kinematics.inverse(targetXY, switchAngles, newConfig)) {
                JointAngles current = robotState.currentAngles;

                Serial.printf("SM:   Config switch transit plan:\n");
                Serial.printf("SM:     FROM  t1=%.1f t2=%.1f [%s]\n",
                              current.theta1, current.theta2,
                              currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT" : "LEFT");
                Serial.printf("SM:     TO    t1=%.1f t2=%.1f [%s] -> XY(%.1f, %.1f)\n",
                              switchAngles.theta1, switchAngles.theta2,
                              newConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT" : "LEFT",
                              targetXY.x, targetXY.y);

                int transitSteps = 50;
                
                // Phase 0: Small delay to let vibration settle
                transitBuffer.push(TargetState(250, 0, MoveMode::DELAY_MS));

                // Phase 1: FOLD — theta2 → 0 (arm extends), theta1 constant
                // The arm sweeps to full extension at the current theta1.
                Serial.printf("SM:     Phase1 FOLD: t2 %.1f -> 0 (t1=%.1f fixed)\n",
                              current.theta2, current.theta1);
                for (int i = 1; i <= transitSteps; i++) {
                    float t = (float)i / transitSteps;
                    float t1 = current.theta1;
                    float t2 = current.theta2 * (1.0f - t);
                    transitBuffer.push(TargetState(t1, t2, MoveMode::JOINT));
                }
                
                // Phase 2: ROTATE — theta1 sweeps to target, theta2 stays 0
                // With theta2=0, the arm traces a clean arc at max reach (L1+L2).
                // No wild intermediate FK positions possible.
                Serial.printf("SM:     Phase2 ROTATE: t1 %.1f -> %.1f (t2=0 fixed)\n",
                              current.theta1, switchAngles.theta1);
                for (int i = 1; i <= transitSteps; i++) {
                    float t = (float)i / transitSteps;
                    float t1 = current.theta1 + t * (switchAngles.theta1 - current.theta1);
                    float t2 = 0.0f;
                    transitBuffer.push(TargetState(t1, t2, MoveMode::JOINT));
                }

                // Phase 3: UNFOLD — theta2 opens to target config, theta1 constant
                Serial.printf("SM:     Phase3 UNFOLD: t2 0 -> %.1f (t1=%.1f fixed)\n",
                              switchAngles.theta2, switchAngles.theta1);
                for (int i = 1; i <= transitSteps; i++) {
                    float t = (float)i / transitSteps;
                    float t1 = switchAngles.theta1;
                    float t2 = t * switchAngles.theta2;
                    transitBuffer.push(TargetState(t1, t2, MoveMode::JOINT));
                }

                // Phase 4: Settling delay
                transitBuffer.push(TargetState(250, 0, MoveMode::DELAY_MS));

                // Phase 5: Restore tool if it was down before switch
                if (restoreToolAfterSwitch) {
                    transitBuffer.push(TargetState(0, 0, MoveMode::TOOL_DOWN_ASYNC));
                    transitBuffer.push(TargetState(250, 0, MoveMode::DELAY_MS));
                }

                // Pop the trigger point — transit already delivers us there
                pointBuffer.pop();

                // Update currentState to the Cartesian position we arrived at
                currentState = TargetState(targetXY.x, targetXY.y);

                currentConfig = newConfig;
                configSwitchPending = false;
                
                state = PlannerState::EXECUTING;
                robotState.plannerState = PlannerState::EXECUTING;
                
                Serial.printf("SM:   -> 3-phase transit generated (%d pts/phase), resuming\n", transitSteps);
            } else {
                Serial.printf("SM: ERROR - target (%.1f, %.1f) not reachable with new config!\n",
                              targetXY.x, targetXY.y);
                // Discard the unreachable point
                pointBuffer.pop();
                state = PlannerState::IDLE;
                robotState.plannerState = PlannerState::IDLE;
                configSwitchPending = false;
            }
        }

        // ====================================================================
        // 6. STATE: IDLE — try to receive next command (blocking with timeout)
        // ====================================================================
        if (state == PlannerState::IDLE) {
            Command cmd;
            // Use short timeout so the 100Hz loop stays responsive for motor updates
            if (xQueueReceive(commandQueue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
                switch (cmd.type) {
                    // ──────────────────────────────────────────────────────
                    // MOVE_TO: interpolate path, fill pointBuffer
                    // ──────────────────────────────────────────────────────
                    case Command::MOVE_TO: {
                        Point2D targetXY(cmd.x, cmd.y);

                        Serial.printf("SM: MOVE_TO (%.2f, %.2f) spd=%.1f\n", cmd.x, cmd.y, cmd.speed);

                        if (!kinematics.isReachable(targetXY)) {
                            Serial.printf("SM: UNREACHABLE! (%.2f, %.2f)\n", cmd.x, cmd.y);
                            break;
                        }

                        float moveSpeed = (cmd.speed > 0) ? cmd.speed : DEFAULT_SPEED;
                        planner.setSpeed(moveSpeed);

                        Point2D startXY = currentState.toPoint2D();
                        std::queue<Point2D> xyQueue;
                        int numPoints = planner.planPath(startXY, targetXY, xyQueue);

                        // Fill point buffer with XY interpolation
                        while (!xyQueue.empty()) {
                            Point2D pt = xyQueue.front();
                            xyQueue.pop();
                            pointBuffer.push(TargetState(pt.x, pt.y));
                        }

                        currentState = TargetState(cmd.x, cmd.y);
                        state = PlannerState::EXECUTING;
                        robotState.plannerState = PlannerState::EXECUTING;
                        robotState.isMoving = true;

                        Serial.printf("SM:   -> %d interpolation points queued\n", numPoints);
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // TOOL_UP: raise Z, tool OFF (non-blocking)
                    // ──────────────────────────────────────────────────────
                    case Command::TOOL_UP: {
                        if (!robotState.toolActive && toolServo.getAngle() >= 170) {
                            Serial.println("SM: Tool already UP, ignoring");
                            break;
                        }
                        Serial.println("SM: TOOL_UP (z=0, tool=OFF)");
                        toolTargetAngle = 175.0f;  // Retracted position
                        toolMovingDown = false;
                        toolMovingUp = true;
                        toolActuateStartTime = millis();
                        state = PlannerState::TOOL_ACTUATING;
                        robotState.plannerState = PlannerState::TOOL_ACTUATING;
                        robotState.isMoving = true;
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // TOOL_DOWN: lower Z, tool ON (non-blocking)
                    // ──────────────────────────────────────────────────────
                    case Command::TOOL_DOWN: {
                        if (robotState.toolActive) {
                            Serial.println("SM: Tool already DOWN, ignoring");
                            break;
                        }
                        Serial.println("SM: TOOL_DOWN (z=5, tool=ON)");
                        toolMovingDown = true;
                        toolMovingUp = false;
                        toolActuateStartTime = millis();
                        state = PlannerState::TOOL_ACTUATING;
                        robotState.plannerState = PlannerState::TOOL_ACTUATING;
                        robotState.isMoving = true;
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // TOOL_CONTROL: legacy boolean -> real servo actuation
                    // ──────────────────────────────────────────────────────
                    case Command::TOOL_CONTROL: {
                        if (cmd.toolState == robotState.toolActive) {
                            Serial.println("SM: Tool state already matches, ignoring");
                            break;
                        }
                        Serial.printf("SM: TOOL_CONTROL %s\n", cmd.toolState ? "ON" : "OFF");
                        if (cmd.toolState) {
                            toolMovingDown = true;
                            toolMovingUp = false;
                        } else {
                            toolTargetAngle = 175.0f;
                            toolMovingDown = false;
                            toolMovingUp = true;
                        }
                        toolActuateStartTime = millis();
                        state = PlannerState::TOOL_ACTUATING;
                        robotState.plannerState = PlannerState::TOOL_ACTUATING;
                        robotState.isMoving = true;
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // HOME: G28 — tool OFF, raise Z, travel to home
                    // ──────────────────────────────────────────────────────
                    case Command::HOME: {
                        Serial.printf("SM: HOME -> (%.0f, %.0f) tool=OFF\n", HOME_X, HOME_Y);
                        Point2D homePos(HOME_X, HOME_Y);
                        Point2D startXY = currentState.toPoint2D();

                        // First: raise tool if physically down
                        if (toolServo.getAngle() < 150) {
                            pointBuffer.push(TargetState(0, 0, MoveMode::TOOL_UP_ASYNC));
                        }

                        // Then: travel to home
                        planner.setSpeed(DEFAULT_SPEED);
                        std::queue<Point2D> xyQueue;
                        int numPts = planner.planPath(startXY, homePos, xyQueue);

                        while (!xyQueue.empty()) {
                            Point2D pt = xyQueue.front();
                            xyQueue.pop();
                            pointBuffer.push(TargetState(pt.x, pt.y));
                        }

                        currentState = TargetState(HOME_X, HOME_Y);
                        robotState.isHomed = true;

                        state = PlannerState::EXECUTING;
                        robotState.plannerState = PlannerState::EXECUTING;
                        robotState.isMoving = true;

                        Serial.printf("SM:   -> %d interpolation points\n", numPts);
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // DELAY: G4 dwell
                    // ──────────────────────────────────────────────────────
                    case Command::DELAY: {
                        Serial.printf("SM: DELAY %lu ms\n", (unsigned long)cmd.delayMs);
                        delayStartTime = millis();
                        delayDuration = cmd.delayMs;
                        state = PlannerState::DELAYING;
                        robotState.plannerState = PlannerState::DELAYING;
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // CONFIG_CHANGE: switch arm config
                    // ──────────────────────────────────────────────────────
                    case Command::CONFIG_CHANGE: {
                        ArmConfig newCfg =
                            (cmd.newConfig == 0) ? ArmConfig::LEFT_ELBOW : ArmConfig::RIGHT_ELBOW;
                        Serial.printf("SM: CONFIG_CHANGE -> %s\n",
                                      cmd.newConfig == 0 ? "LEFT" : "RIGHT");
                        currentConfig = newCfg;
                        // Stay IDLE, next command will use new config
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // STOP: handled by stopRequested flag above
                    // ──────────────────────────────────────────────────────
                    case Command::STOP: {
                        stopRequested = true;
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // SET_SPEED: update planner speed
                    // ──────────────────────────────────────────────────────
                    case Command::SET_SPEED: {
                        Serial.printf("SM: SET_SPEED %.1f mm/s\n", cmd.speed);
                        planner.setSpeed(cmd.speed);
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // SET_HOME: disable torque for manual positioning
                    // ──────────────────────────────────────────────────────
                    case Command::SET_HOME: {
                        Serial.println("SM: SET_HOME — torque OFF, move arm by hand");
                        if (dxlCtrl)
                            dxlCtrl->setHomeMode();
                        break;
                    }

                    // ──────────────────────────────────────────────────────
                    // SAVE_HOME: read encoder positions, save as new (0,0)
                    // ──────────────────────────────────────────────────────
                    case Command::SAVE_HOME: {
                        Serial.println("SM: SAVE_HOME — saving current position as home");
                        if (dxlCtrl) {
                            dxlCtrl->saveHome();
                            // Persist to NVS
                            float off1, off2;
                            dxlCtrl->getOffsets(off1, off2);
                            Preferences prefs;
                            prefs.begin("plasmarm", false);  // read-write
                            prefs.putFloat("off1", off1);
                            prefs.putFloat("off2", off2);
                            prefs.end();
                            Serial.printf("SM: Home saved to NVS: (%.2f, %.2f)\n", off1, off2);
                        }
                        break;
                    }

                    default:
                        Serial.printf("SM: Unknown command type %d\n", cmd.type);
                        break;
                }
            }
        }

        // ====================================================================
        // 6. MOTOR UPDATE (always — for isMoving feedback)
        // ====================================================================
        if (dxlCtrl)
            dxlCtrl->update();

        // ====================================================================
        // 7. SERIAL STATUS LOG (throttled)
        // ====================================================================
        unsigned long now = millis();
        if (now - lastStatusLog >= STATUS_LOG_INTERVAL_MS) {
            lastStatusLog = now;
            int cmdUsed = COMMAND_QUEUE_SIZE - uxQueueSpacesAvailable(commandQueue);

            const char* stateStr = "IDLE";
            switch (state) {
                case PlannerState::IDLE:
                    stateStr = "IDLE";
                    break;
                case PlannerState::EXECUTING:
                    stateStr = "EXEC";
                    break;
                case PlannerState::TOOL_ACTUATING:
                    stateStr = "TOOL";
                    break;
                case PlannerState::DELAYING:
                    stateStr = "WAIT";
                    break;
                case PlannerState::SWITCHING_CONFIG:
                    stateStr = "SWCH";
                    break;
                case PlannerState::SWITCH_RAISE_TOOL:
                    stateStr = "SWTL";
                    break;
            }

            Serial.printf("SM[%s]: pos(%.1f,%.1f) t1=%.1f t2=%.1f moving=%s cmdQ=%d/%d\n", stateStr,
                          robotState.currentPosition.x, robotState.currentPosition.y,
                          robotState.currentAngles.theta1, robotState.currentAngles.theta2,
                          robotState.isMoving ? "Y" : "N", cmdUsed, COMMAND_QUEUE_SIZE);
        }

        // ====================================================================
        // 8. FIXED-FREQUENCY LOOP (100Hz)
        // ====================================================================
        vTaskDelayUntil(&lastWakeTime, loopDelay);
    }
}
