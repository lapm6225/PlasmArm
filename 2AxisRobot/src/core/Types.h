#ifndef TYPES_H
#define TYPES_H

/**
 * @file Types.h
 * @brief Common data structures for the SCARA robot controller
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

// Joint angles in degrees
struct JointAngles {
    float theta1;  // Base joint angle
    float theta2;  // Elbow joint angle
    
    JointAngles() : theta1(0.0f), theta2(0.0f) {}
    JointAngles(float t1, float t2) : theta1(t1), theta2(t2) {}
};

// Robot state information
struct RobotState {
    Point2D currentPosition;    // Current Cartesian position
    JointAngles currentAngles;  // Current joint angles
    bool isMoving;               // Movement status
    bool isHomed;                // Homing status
    
    RobotState() : currentPosition(0, 0), currentAngles(0, 0), 
                   isMoving(false), isHomed(false) {}
};

// Command from web interface or G-code parser
struct Command {
    enum Type {
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
};

#endif // TYPES_H
