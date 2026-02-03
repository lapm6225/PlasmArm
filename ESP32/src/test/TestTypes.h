#ifndef TEST_TYPES_H
#define TEST_TYPES_H

#include "TestRunner.h"
#include "../core/Types.h"

/**
 * @file TestTypes.h
 * @brief Unit tests for Types module (data structures)
 */

class TestTypes {
public:
    static void runAllTests(TestRunner& runner);
    
private:
    // Point2D tests
    static bool testPoint2D_DefaultConstructor();
    static bool testPoint2D_ParameterizedConstructor();
    static bool testPoint2D_Equality();
    
    // JointAngles tests
    static bool testJointAngles_DefaultConstructor();
    static bool testJointAngles_ParameterizedConstructor();
    
    // RobotState tests
    static bool testRobotState_DefaultConstructor();
    static bool testRobotState_Initialization();
    
    // Command tests
    static bool testCommand_DefaultConstructor();
    static bool testCommand_Types();
};

#endif // TEST_TYPES_H
