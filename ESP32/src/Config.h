#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// WiFi Configuration
// ============================================================================
// Set to true to create an Access Point, false to connect to an existing WiFi
#define WIFI_AP_MODE true

// AP Mode Settings (When WIFI_AP_MODE is true)
#define WIFI_AP_SSID "PlasmArm_ESP32"
#define WIFI_AP_PASSWORD "12345678"  // Must be at least 8 characters

// Station Mode Settings (When WIFI_AP_MODE is false)
#define WIFI_STA_SSID "nom du mot du wifi"
#define WIFI_STA_PASSWORD "mot de passe"

#define WIFI_TIMEOUT_MS 10000

// ============================================================================
// Robot Physical Parameters
// ============================================================================
// Arm lengths in millimeters
#define ARM_LENGTH_1 220.05f  // Length of first link (base to elbow)
#define ARM_LENGTH_2 217.65f  // Length of second link (elbow to end effector)

// Joint angle limits (degrees, measured from +X axis, CCW positive)
#define THETA1_MIN 0.0f     // Slightly past +X axis (clockwise)
#define THETA1_MAX 180.0f   // Slightly past -X axis (counter-clockwise)
#define THETA2_MIN -150.0f  // Fully extended
#define THETA2_MAX 150.0f   // Practical folding limit

// Workspace limits (SCARA half-circle workspace)
// The robot sits at the origin (0,0) and can reach a half-annulus
// in the upper half-plane (y > 0).
//
//               (0, R_MAX)            Outer boundary: r = L1+L2 = R_MAX
//                  |                Inner boundary: r =
//                 / \               Half-plane:     y > 0
//    (-R_MAX,0)----+---+----(R_MAX,0)   Theta1 range:   0° to 180°
//               (BASE)
//
#define WORKSPACE_R_MAX (ARM_LENGTH_1 + ARM_LENGTH_2)  // max reach
#define WORKSPACE_R_MIN ARM_LENGTH_1 - std::cos(THETA2_MAX) * ARM_LENGTH_2  // Minimum reach - prevents arm folding on itself

// Home position: arm fully extended upward (+Y direction)
#define HOME_X WORKSPACE_R_MAX
#define HOME_Y 0.0f
#define HOME_THETA1 90.0f  // Pointing up
#define HOME_THETA2 0.0f   // Fully extended

// ============================================================================
// Motor Configuration
// ============================================================================
// Motor 1 (Base Joint) - Stepper Motor Pins
#define MOTOR1_STEP_PIN 19
#define MOTOR1_DIR_PIN 18
#define MOTOR1_ENABLE_PIN 27

// Motor 2 (Elbow Joint) - Stepper Motor Pins
// WARNING: GPIO12 is a STRAPPING PIN! It controls boot flash voltage.
// If motor driver holds GPIO12 HIGH during boot, ESP32 won't boot.
// Ensure motor driver has pulldown or is LOW during power-on.
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

// Tool output (relay, plasma torch trigger, etc.)
#define TOOL_SERVO_PIN 26   // pin pour donner les positions du servo
#define TOOL_SWITCH_PIN 32  // pin pour lire l'état du switch de pression

// Tool actuation parameters
#define TOOL_STEP_DEG 1.0f            // degrees per step (non-blocking speed control)
#define TOOL_ACTUATE_TIMEOUT_MS 5000  // max time for down motion before timeout

// Serial communication with Dynamixels
#define RXD2 16
#define TXD2 17

// ============================================================================
// Motion Control Parameters
// ============================================================================
#define DEFAULT_SPEED 50.0f  // mm/s
#define MAX_SPEED 100.0f     // mm/s
#define ACCELERATION 200.0f  // mm/s²
#define JERK_LIMIT 500.0f    // mm/s³

// Interpolation parameters
#define INTERPOLATION_INTERVAL_MS 10  // Time between interpolated points
#define MIN_SEGMENT_LENGTH 0.1f       // Minimum segment length in mm

// ============================================================================
// FreeRTOS Task Configuration
// ============================================================================
// Stack sizes (in words, 1 word = 4 bytes on ESP32)
#define TASK_WEB_HANDLER_STACK_SIZE 8192     // 32 KB
#define TASK_PLANNER_STACK_SIZE 4096         // 16 KB
#define TASK_MOTION_CONTROL_STACK_SIZE 4096  // 16 KB

// Task priorities (higher number = higher priority)
#define TASK_WEB_HANDLER_PRIORITY 1
#define TASK_PLANNER_PRIORITY 2
#define TASK_MOTION_CONTROL_PRIORITY 3

// Queue sizes
#define COMMAND_QUEUE_SIZE 30  // High-level commands from host (tune for streaming)

// Motion control loop frequency (Hz)
#define MOTION_CONTROL_FREQUENCY 100  // 100 Hz = 10ms loop

// ============================================================================
// Debug Configuration
// ============================================================================
#define SERIAL_BAUD_RATE 115200
#define SERIAL_OPENRB_BAUD_RATE 500000
#define DEBUG_KINEMATICS false
#define DEBUG_PLANNER false
#define DEBUG_MOTOR false

// ============================================================================
// Test Mode Configuration
// ============================================================================
// Set to true to run unit tests on startup instead of normal operation
#define RUN_UNIT_TESTS false
// Set to true to run only visual tests (detailed output, no pass/fail)
#define RUN_VISUAL_TESTS false
// Set to true to run interactive integration test with real motors
#define RUN_INTERACTIVE_TEST false
// Set to true to run interactive integration test with state machine
#define RUN_INTERACTIVE_TEST2 false
// Set to true to run the simple dynamixel communication test
#define RUN_DYNAMIXEL_COMM_TEST false
// Set to true to run the simple effector test
#define RUN_EFFECTOR_TEST false

#endif  // CONFIG_H
