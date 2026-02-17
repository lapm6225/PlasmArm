#ifndef TYPES_H
#define TYPES_H

/**
 * @file Types.h
 * @brief Common data structures for the SCARA robot controller
 * 
 * Central type definitions for the entire project.
 * TargetState is the 4D motion primitive (X, Y, Z, Tool).
 * Command is the high-level instruction from host/WebSocket.
 */

// 2D Cartesian point
struct Point2D {
    float x;
    float y;
    
    Point2D() : x(0.0f), y(0.0f) {}
    Point2D(float x, float y) : x(x), y(y) {}
    
    bool operator==(const Point2D& other) const {
        return (x == other.x && y == other.y);
    }
};

/**
 * @brief 4D motion target — the fundamental unit of synchronized motion.
 * 
 * Every single interpolated point in the motion queue carries the full
 * state of the robot: position (X, Y), tool height (Z), and tool 
 * activation state. This ensures the tool is never desynchronized from 
 * the motion path.
 * 
 * Used by: Planner → motionQueue → MotionControl task
 */
struct TargetState {
    float x;           // Cartesian X position (mm)
    float y;           // Cartesian Y position (mm)
    float z;           // Tool height / Z axis (mm)
    bool  toolActive;  // Tool state: true = ON (cutting/extruding), false = OFF (travel)

    TargetState() : x(0.0f), y(0.0f), z(0.0f), toolActive(false) {}
    TargetState(float x, float y, float z = 0.0f, bool tool = false) 
        : x(x), y(y), z(z), toolActive(tool) {}
    
    // Convenience: extract the XY position as a Point2D
    Point2D toPoint2D() const { return Point2D(x, y); }

    bool operator==(const TargetState& other) const {
        return (x == other.x && y == other.y && z == other.z && toolActive == other.toolActive);
    }
};

// Joint angles in degrees
struct JointAngles {
    float theta1;  // Base joint angle
    float theta2;  // Elbow joint angle
    
    JointAngles() : theta1(0.0f), theta2(0.0f) {}
    JointAngles(float t1, float t2) : theta1(t1), theta2(t2) {}
};

// Robot state information
struct RobotState {
<<<<<<< Updated upstream:src/core/Types.h
    Point2D currentPosition;    // Current Cartesian position
=======
    Point2D currentPosition;    // Current Cartesian position (XY)
    float toolZ;                // Current Z height
    bool toolActive;            // Tool state (ON/OFF)
>>>>>>> Stashed changes:ESP32/src/core/Types.h
    JointAngles currentAngles;  // Current joint angles
    bool isMoving;              // Movement status
    bool isHomed;               // Homing status
    
    RobotState() : currentPosition(0, 0), currentAngles(0, 0), 
                   isMoving(false), isHomed(false) {}
};

/**
 * @brief High-level command from host (WebSocket, HTTP, Serial).
 * 
 * Commands are placed in the commandQueue by the WebServer or test code.
 * The Trajectory Planner task reads commands and converts them to a stream
 * of TargetState points in the motionQueue.
 * 
 * A MOVE_TO command carries the destination (x, y, z) AND the tool state
 * for the entire move segment. For example:
 *   - MOVE_TO(100, 200, 0, tool=true)  → cut/extrude while traveling
 *   - MOVE_TO(50, 50, 10, tool=false)  → rapid travel (tool raised)
 */
struct Command {
    enum Type {
<<<<<<< Updated upstream:src/core/Types.h
        MOVE_TO,      // Move to absolute position
        MOVE_RELATIVE,// Move relative to current position
        HOME,         // Home the robot
        SET_SPEED,    // Set movement speed
        STOP          // Emergency stop
    };
    
    Type type;
    Point2D target;  // Target position (for MOVE_TO, MOVE_RELATIVE)
    float speed;     // Speed parameter (for SET_SPEED, MOVE_TO)
    
    Command() : type(MOVE_TO), target(0, 0), speed(0.0f) {}
    Command(Type t, Point2D pos, float spd = 0.0f) 
        : type(t), target(pos), speed(spd) {}
=======
        MOVE_TO,       // Move to absolute position (with tool state)
        MOVE_RELATIVE, // Move relative to current position
        HOME,          // Home the robot
        SET_SPEED,     // Set movement speed
        TOOL_CONTROL,  // Change tool state without moving (on/off + optional Z)
        STOP           // Emergency stop
    };
    
    Type type;
    float x;         // Target X (for MOVE_TO, MOVE_RELATIVE)
    float y;         // Target Y (for MOVE_TO, MOVE_RELATIVE)
    float z;         // Target Z / tool height
    float speed;     // Speed parameter (mm/s)
    bool toolState;  // Tool ON/OFF

    // Default constructor
    Command() : type(MOVE_TO), x(0.0f), y(0.0f), z(0.0f), speed(0.0f), toolState(false) {}
    
    // Movement command (with optional Z and tool)
    Command(Type t, float x, float y, float spd = 0.0f, float z = 0.0f, bool tool = false) 
        : type(t), x(x), y(y), z(z), speed(spd), toolState(tool) {}

    // Tool-only command (TOOL_CONTROL)
    Command(Type t, bool state, float zVal = 0.0f) 
        : type(t), x(0.0f), y(0.0f), z(zVal), speed(0.0f), toolState(state) {}

    // Convenience: get target as Point2D
    Point2D targetPoint() const { return Point2D(x, y); }

    // Convenience: get target as TargetState
    TargetState targetState() const { return TargetState(x, y, z, toolState); }
>>>>>>> Stashed changes:ESP32/src/core/Types.h
};

#endif // TYPES_H
