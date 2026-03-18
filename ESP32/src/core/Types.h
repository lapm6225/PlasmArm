#ifndef TYPES_H
#define TYPES_H

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

// 2D Cartesian point
struct Point2D {
  float x;
  float y;

  Point2D() : x(0.0f), y(0.0f) {}
  Point2D(float x, float y) : x(x), y(y) {}

  bool operator==(const Point2D &other) const {
    return (x == other.x && y == other.y);
  }
};

// Joint angles in degrees
struct JointAngles {
  float theta1; // Base joint angle
  float theta2; // Elbow joint angle

  JointAngles() : theta1(0.0f), theta2(0.0f) {}
  JointAngles(float t1, float t2) : theta1(t1), theta2(t2) {}
};

// 4D motion target: X, Y, Z, Tool
struct TargetState {
  float x;
  float y;
  float z;
  bool toolActive;

  TargetState() : x(0), y(0), z(0), toolActive(false) {}
  TargetState(float x, float y, float z = 0.0f, bool tool = false)
      : x(x), y(y), z(z), toolActive(tool) {}

  Point2D toPoint2D() const { return Point2D(x, y); }
};

// Robot state information
struct RobotState {
  Point2D currentPosition; // Current Cartesian position
  float toolZ;
  bool toolActive;           // Tool state
  JointAngles currentAngles; // Current joint angles
  bool isMoving;             // Movement status
  bool isHomed;              // Homing status

  RobotState()
      : currentPosition(0, 300), toolZ(0.0f), toolActive(false),
        currentAngles(90, 0), isMoving(false), isHomed(false) {}
};

// Command from web interface or G-code parser
struct Command {
  enum Type {
    MOVE_TO,       // Move to absolute position
    MOVE_RELATIVE, // Move relative to current position
    HOME,          // Home the robot
    SET_SPEED,     // Set movement speed
    TOOL_CONTROL,  // Control tool (on/off + Z)
    STOP           // Emergency stop
  };

  Type type;
  float x, y;     // Target XY position
  float z;        // Z-axis value
  float speed;    // Speed parameter
  bool toolState; // Tool on/off

  Command() : type(MOVE_TO), x(0), y(0), z(0), speed(0), toolState(false) {}

  // General constructor
  Command(Type t, float x, float y, float z = 0.0f, float spd = 0.0f, bool tool = false)
      : type(t), x(x), y(y), z(z), speed(spd), toolState(tool) {}

  // Tool-only constructor
  Command(Type t, bool state, float zVal = 0.0f)
      : type(t), x(0), y(0), z(zVal), speed(0), toolState(state) {}
};

#endif // TYPES_H
