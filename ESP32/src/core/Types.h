#ifndef TYPES_H
#define TYPES_H
#include "../Config.h"

/**
 * @file Types.h
 * @brief Common data structures for the SCARA robot controller
 */

/**
 * @brief Arm configuration for inverse kinematics
 *
 * A 2-DOF SCARA has two IK solutions for most points (elbow left vs right).
 * The active config is kept until IK fails in it (lazy switching).
 *   - RIGHT_ELBOW: theta2 ≤ 0 (natural for positive-x side)
 *   - LEFT_ELBOW:  theta2 ≥ 0 (natural for negative-x side)
 *   - AUTO: pick based on target x-sign (used only for initial config setup)
 */
enum class ArmConfig {
    RIGHT_ELBOW,  // theta2 ≤ 0
    LEFT_ELBOW,   // theta2 ≥ 0
    AUTO          // auto-select: x >= 0 → RIGHT_ELBOW, x < 0 → LEFT_ELBOW
};

/**
 * @brief G-code style planner state machine
 */
enum class PlannerState {
    IDLE,             // Waiting for next command
    EXECUTING,        // Processing interpolated points at 100Hz
    TOOL_ACTUATING,   // Waiting for tool Z motion to complete
    DELAYING,         // Waiting for millis()-based delay to elapse
    SWITCHING_CONFIG, // Transitioning to new arm config
    SWITCH_RAISE_TOOL // Waiting for tool to raise before config switch
};

// Mode of movement for the target state
enum class MoveMode {
    CARTESIAN,       // x, y are Cartesian coordinates (IK applies)
    JOINT,           // x=theta1, y=theta2 (raw joints, IK bypassed)
    DELAY_MS,        // x=milliseconds to wait
    TOOL_UP_ASYNC,   // trigger tool up
    TOOL_DOWN_ASYNC  // trigger tool down
};

// 2D Cartesian point
struct Point2D {
    float x;
    float y;

    Point2D() : x(0.0f), y(0.0f) {}
    Point2D(float x, float y) : x(x), y(y) {}

    bool operator==(const Point2D& other) const { return (x == other.x && y == other.y); }
};

// Joint angles in degrees
struct JointAngles {
    float theta1;  // Base joint angle
    float theta2;  // Elbow joint angle

    JointAngles() : theta1(0.0f), theta2(0.0f) {}
    JointAngles(float t1, float t2) : theta1(t1), theta2(t2) {}
};

// Motion target: X, Y (or t1, t2 for JOINT, or ms for DELAY_MS)
// Tool state is NOT embedded here — it is managed exclusively by
// TOOL_UP/TOOL_DOWN commands and the TOOL_ACTUATING state.
struct TargetState {
    MoveMode mode;
    float x; // Cartesian X | theta1 (JOINT) | milliseconds (DELAY_MS)
    float y; // Cartesian Y | theta2 (JOINT) | unused for DELAY/TOOL modes

    TargetState() : mode(MoveMode::CARTESIAN), x(0), y(0) {}
    TargetState(float x, float y, MoveMode m = MoveMode::CARTESIAN)
        : mode(m), x(x), y(y) {}

    Point2D toPoint2D() const { return Point2D(x, y); }
    JointAngles toJointAngles() const { return JointAngles(x, y); }
};

// Robot state information
struct RobotState {
    Point2D currentPosition;  // Current Cartesian position
    float toolZ;
    bool toolActive;            // Tool state
    JointAngles currentAngles;  // Current joint angles
    bool isMoving;              // Movement status (from motor feedback)
    bool isHomed;               // Homing status
    PlannerState plannerState;  // Current planner state machine state

    RobotState()
        : currentPosition(ARM_LENGTH_1 + ARM_LENGTH_2, 0),
          toolZ(0.0f),
          toolActive(false),
          currentAngles(0, 0),
          isMoving(false),
          isHomed(false),
          plannerState(PlannerState::IDLE) {}
};

// Command from web interface or G-code parser
struct Command {
    enum Type {
        MOVE_TO,        // G0/G1 style move to absolute position
        MOVE_RELATIVE,  // Move relative to current position
        HOME,           // G28 home
        SET_SPEED,      // F parameter
        TOOL_CONTROL,   // Legacy boolean tool on/off + Z
        TOOL_UP,        // G-code style: raise tool (safe Z, tool OFF)
        TOOL_DOWN,      // G-code style: lower tool (Z down, tool ON)
        STOP,           // Emergency stop (M0)
        DELAY,          // G4 dwell
        CONFIG_CHANGE,  // Arm config switch (LEFT/RIGHT_ELBOW)
        SET_HOME,       // Disable torque for manual positioning
        SAVE_HOME,      // Save current encoder position as home (0,0)
        TORQUE_OFF,     // Disable torque
        TORQUE_ON       // Enable torque
    };

    Type type;
    float x, y;        // Target XY position
    float z;           // Z-axis value
    float speed;       // Speed parameter
    bool toolState;    // Tool on/off (legacy TOOL_CONTROL)
    uint32_t delayMs;  // Delay duration in ms (for DELAY)
    int newConfig;     // Config value (for CONFIG_CHANGE: 0=LEFT, 1=RIGHT)

    Command()
        : type(MOVE_TO), x(0), y(0), z(0), speed(0), toolState(false), delayMs(0), newConfig(0) {}

    // General constructor
    Command(Type t, float x, float y, float z = 0.0f, float spd = 0.0f, bool tool = false)
        : type(t), x(x), y(y), z(z), speed(spd), toolState(tool), delayMs(0), newConfig(0) {}

    // Tool-only constructor
    Command(Type t, bool state, float zVal = 0.0f)
        : type(t), x(0), y(0), z(zVal), speed(0), toolState(state), delayMs(0), newConfig(0) {}
};

#endif  // TYPES_H
