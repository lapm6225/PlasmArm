#ifndef TEST_INTERACTIVE_H
#define TEST_INTERACTIVE_H

#include "../core/Kinematics.h"
#include "../core/Planner.h"
#include "../core/Types.h"
#include "../hardware/IMotor.h"
#include <queue>

/**
 * @file TestInteractive.h
 * @brief Interactive integration test with real stepper motors
 * 
 * Allows entering commands via Serial console:
 *   x,y          - Move to position
 *   x,y,z        - Move to position with Z height
 *   x,y,z,tool   - Move with tool state (0/1)
 *   tool on/off   - Toggle tool state
 *   pos           - Show current position
 *   test          - Run test sequence (with tool toggling)
 */

class TestInteractive {
public:
    static void run(IMotor* motor1, IMotor* motor2);
    
private:
    static void printHelp();
    static void processCommand(const String& command, 
                              Kinematics& kin, 
                              Planner& planner,
                              IMotor* motor1, 
                              IMotor* motor2,
                              TargetState& currentState);
    static void executeMove(const TargetState& start,
                           const TargetState& target,
                           Kinematics& kin,
                           Planner& planner,
                           IMotor* motor1,
                           IMotor* motor2,
                           bool showDetails);
    static void printState(const TargetState& s, const char* label = "");
    static void printAngles(const JointAngles& angles, const char* label = "");
};

#endif // TEST_INTERACTIVE_H
