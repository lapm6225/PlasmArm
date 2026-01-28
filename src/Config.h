#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// WiFi Configuration
// ============================================================================
#define WIFI_SSID "204-2381-Belvedere"
#define WIFI_PASSWORD "AmandeRose92"
#define WIFI_TIMEOUT_MS 10000

// ============================================================================
// Robot Physical Parameters
// ============================================================================
// Arm lengths in millimeters
#define ARM_LENGTH_1 150.0f  // Length of first link (base to elbow)
#define ARM_LENGTH_2 150.0f  // Length of second link (elbow to end effector)

// Workspace limits (in mm)
#define WORKSPACE_X_MIN -200.0f
#define WORKSPACE_X_MAX 200.0f
#define WORKSPACE_Y_MIN 50.0f
#define WORKSPACE_Y_MAX 300.0f

// ============================================================================
// Motor Configuration
// ============================================================================
// Motor 1 (Base Joint) - Stepper Motor Pins
#define MOTOR1_STEP_PIN 19
#define MOTOR1_DIR_PIN 18
#define MOTOR1_ENABLE_PIN 27

// Motor 2 (Elbow Joint) - Stepper Motor Pins
#define MOTOR2_STEP_PIN 14
#define MOTOR2_DIR_PIN 12
#define MOTOR2_ENABLE_PIN 13

// Stepper motor parameters
#define STEPS_PER_REVOLUTION 200  // 1.8° per step
#define MICROSTEPS 16             // Microstepping factor
#define STEPS_PER_DEGREE ((STEPS_PER_REVOLUTION * MICROSTEPS) / 360.0f)

// Servo motor parameters (if using servos instead)
#define SERVO1_PIN 20
#define SERVO2_PIN 21
#define SERVO_MIN_PULSE 500   // microseconds
#define SERVO_MAX_PULSE 2500  // microseconds

// ============================================================================
// Motion Control Parameters
// ============================================================================
#define DEFAULT_SPEED 50.0f        // mm/s
#define MAX_SPEED 100.0f            // mm/s
#define ACCELERATION 200.0f         // mm/s²
#define JERK_LIMIT 500.0f           // mm/s³

// Interpolation parameters
#define INTERPOLATION_INTERVAL_MS 10  // Time between interpolated points
#define MIN_SEGMENT_LENGTH 0.1f       // Minimum segment length in mm

// ============================================================================
// FreeRTOS Task Configuration
// ============================================================================
// Stack sizes (in words, 1 word = 4 bytes on ESP32)
#define TASK_WEB_HANDLER_STACK_SIZE 8192      // 32 KB
#define TASK_PLANNER_STACK_SIZE 4096          // 16 KB
#define TASK_MOTION_CONTROL_STACK_SIZE 4096   // 16 KB

// Task priorities (higher number = higher priority)
#define TASK_WEB_HANDLER_PRIORITY 1
#define TASK_PLANNER_PRIORITY 2
#define TASK_MOTION_CONTROL_PRIORITY 3

// Queue sizes
#define COMMAND_QUEUE_SIZE 10
#define MOTION_QUEUE_SIZE 50

// Motion control loop frequency (Hz)
#define MOTION_CONTROL_FREQUENCY 100  // 100 Hz = 10ms loop

// ============================================================================
// Debug Configuration
// ============================================================================
#define SERIAL_BAUD_RATE 115200
#define DEBUG_KINEMATICS false
#define DEBUG_PLANNER false
#define DEBUG_MOTOR false

// ============================================================================
// Test Mode Configuration
// ============================================================================
// Set to true to run unit tests on startup instead of normal operation
#define RUN_UNIT_TESTS false
// Set to true to run only visual tests (detailed output, no pass/fail)
#define RUN_VISUAL_TESTS true

#endif // CONFIG_H
