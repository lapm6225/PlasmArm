#ifndef TEST_KINEMATICS_H
#define TEST_KINEMATICS_H

#include "TestRunner.h"
#include "../core/Kinematics.h"
#include "../core/Types.h"

/**
 * @file TestKinematics.h
 * @brief Unit tests for Kinematics module
 */

class TestKinematics {
public:
    static void runAllTests(TestRunner& runner);
    
private:
    // Forward kinematics tests
    static bool testForwardKinematics_ZeroAngles();
    static bool testForwardKinematics_90Degrees();
    static bool testForwardKinematics_180Degrees();
    
    // Inverse kinematics tests
    static bool testInverseKinematics_StraightOut();
    static bool testInverseKinematics_RightAngle();
    static bool testInverseKinematics_CircularPath();
    
    // Reachability tests
    static bool testIsReachable_WithinRange();
    static bool testIsReachable_OutOfRange();
    static bool testIsReachable_EdgeCases();
    
    // Round-trip tests (forward then inverse)
    static bool testRoundTrip_Simple();
    static bool testRoundTrip_MultipleAngles();
};

#endif // TEST_KINEMATICS_H
