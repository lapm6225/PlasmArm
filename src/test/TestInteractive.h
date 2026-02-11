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
 * Allows entering x,y coordinates via Serial console and seeing:
 * - All interpolation points
 * - Calculated angles for each point
 * - Real motor movement
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
                              Point2D& currentPos);
    static void executeMove(const Point2D& start,
                           const Point2D& target,
                           Kinematics& kin,
                           Planner& planner,
                           IMotor* motor1,
                           IMotor* motor2,
                           bool showDetails);
    static void printPoint(const Point2D& p, const char* label = "");
    static void printAngles(const JointAngles& angles, const char* label = "");
};

#endif // TEST_INTERACTIVE_H
