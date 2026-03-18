#include "TestKinematics.h"
#include <math.h>

void TestKinematics::runAllTests(TestRunner& runner) {
    runner.printHeader("KINEMATICS");
    
    // Forward kinematics tests
    runner.runTest("Forward: Zero angles", testForwardKinematics_ZeroAngles);
    runner.runTest("Forward: 90 degrees", testForwardKinematics_90Degrees);
    runner.runTest("Forward: 180 degrees", testForwardKinematics_180Degrees);
    
    // Inverse kinematics tests
    runner.runTest("Inverse: Straight out", testInverseKinematics_StraightOut);
    runner.runTest("Inverse: Right angle", testInverseKinematics_RightAngle);
    runner.runTest("Inverse: Circular path", testInverseKinematics_CircularPath);
    
    // Reachability tests
    runner.runTest("Reachability: Within range", testIsReachable_WithinRange);
    runner.runTest("Reachability: Out of range", testIsReachable_OutOfRange);
    runner.runTest("Reachability: Edge cases", testIsReachable_EdgeCases);
    
    // Round-trip tests
    runner.runTest("Round-trip: Simple", testRoundTrip_Simple);
    runner.runTest("Round-trip: Multiple angles", testRoundTrip_MultipleAngles);
    
    // Arm configuration tests
    runner.runTest("ArmConfig: Right elbow +X", testInverse_RightElbow_PositiveX);
    runner.runTest("ArmConfig: Left elbow -X", testInverse_LeftElbow_NegativeX);
    runner.runTest("ArmConfig: AUTO picks correctly", testInverse_Auto_PicksCorrectly);
    runner.runTest("ArmConfig: Fallback on limit", testInverse_FallbackOnLimitViolation);
    runner.runTest("ArmConfig: Round-trip both", testRoundTrip_BothConfigs);
    runner.runTest("ArmConfig: On Y axis", testInverse_OnYAxis);
    runner.runTest("ArmConfig: Reports used config", testInverse_ReportsUsedConfig);
    runner.runTest("ArmConfig: Lazy switching", testLazyConfigSwitching);
}

// Forward Kinematics Tests
bool TestKinematics::testForwardKinematics_ZeroAngles() {
    Kinematics kin(150.0f, 150.0f);
    JointAngles angles(0.0f, 0.0f);
    Point2D result;
    
    kin.forward(angles, result);
    
    // At 0°, both arms straight: x = L1 + L2, y = 0
    float expectedX = 150.0f + 150.0f;  // 300mm
    float expectedY = 0.0f;
    
    TestRunner runner(false);
    return runner.assertNear(expectedX, result.x, 0.1f) &&
           runner.assertNear(expectedY, result.y, 0.1f);
}

bool TestKinematics::testForwardKinematics_90Degrees() {
    Kinematics kin(150.0f, 150.0f);
    JointAngles angles(90.0f, 0.0f);
    Point2D result;
    
    kin.forward(angles, result);
    
    // At 90°, first arm up: x = 0, y = L1 + L2
    float expectedX = 0.0f;
    float expectedY = 150.0f + 150.0f;  // 300mm
    
    TestRunner runner(false);
    return runner.assertNear(expectedX, result.x, 0.1f) &&
           runner.assertNear(expectedY, result.y, 0.1f);
}

bool TestKinematics::testForwardKinematics_180Degrees() {
    Kinematics kin(150.0f, 150.0f);
    JointAngles angles(180.0f, 0.0f);
    Point2D result;
    
    kin.forward(angles, result);
    
    // At 180°, first arm left: x = -(L1 + L2), y = 0
    float expectedX = -(150.0f + 150.0f);  // -300mm
    float expectedY = 0.0f;
    
    TestRunner runner(false);
    return runner.assertNear(expectedX, result.x, 0.1f) &&
           runner.assertNear(expectedY, result.y, 0.1f);
}

// Inverse Kinematics Tests
bool TestKinematics::testInverseKinematics_StraightOut() {
    Kinematics kin(150.0f, 150.0f);
    Point2D target(300.0f, 0.0f);  // Straight out at 0°
    JointAngles result;
    
    bool success = kin.inverse(target, result);
    
    if (!success) return false;
    
    // Should be approximately 0°, 0°
    TestRunner runner(false);
    return runner.assertNear(0.0f, result.theta1, 1.0f) &&
           runner.assertNear(0.0f, result.theta2, 1.0f);
}

bool TestKinematics::testInverseKinematics_RightAngle() {
    Kinematics kin(150.0f, 150.0f);
    Point2D target(0.0f, 300.0f);  // Straight up
    JointAngles result;
    
    bool success = kin.inverse(target, result);
    
    if (!success) return false;
    
    // Should be approximately 90°, 0°
    TestRunner runner(false);
    return runner.assertNear(90.0f, result.theta1, 1.0f) &&
           runner.assertNear(0.0f, result.theta2, 1.0f);
}

bool TestKinematics::testInverseKinematics_CircularPath() {
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Test multiple points on a circle
    for (int angle = 0; angle < 360; angle += 45) {
        float rad = angle * M_PI / 180.0f;
        float radius = 200.0f;
        Point2D target(radius * cos(rad), radius * sin(rad));
        JointAngles result;
        
        if (!kin.inverse(target, result)) {
            return false;
        }
        
        // Verify round-trip: forward should give us back the target
        Point2D verify;
        kin.forward(result, verify);
        
        if (!runner.assertNear(target.x, verify.x, 1.0f) ||
            !runner.assertNear(target.y, verify.y, 1.0f)) {
            return false;
        }
    }
    
    return true;
}

// Reachability Tests
bool TestKinematics::testIsReachable_WithinRange() {
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Points within reach
    Point2D p1(200.0f, 100.0f);
    Point2D p2(0.0f, 250.0f);
    Point2D p3(150.0f, 150.0f);
    
    return runner.assertTrue(kin.isReachable(p1)) &&
           runner.assertTrue(kin.isReachable(p2)) &&
           runner.assertTrue(kin.isReachable(p3));
}

bool TestKinematics::testIsReachable_OutOfRange() {
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Points out of reach
    Point2D p1(400.0f, 0.0f);      // Too far
    Point2D p2(0.0f, 400.0f);      // Too far
    Point2D p3(10.0f, 10.0f);      // Too close (inside minimum reach)
    
    return runner.assertFalse(kin.isReachable(p1)) &&
           runner.assertFalse(kin.isReachable(p2)) &&
           runner.assertFalse(kin.isReachable(p3));
}

bool TestKinematics::testIsReachable_EdgeCases() {
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Edge cases
    float maxReach = kin.getMaxReach();  // 300mm
    float minReach = kin.getMinReach();  // 0mm (L1 == L2)
    
    Point2D p1(maxReach, 0.0f);         // At max reach
    Point2D p2(maxReach * 0.99f, 0.0f); // Just inside
    Point2D p3(maxReach * 1.01f, 0.0f); // Just outside
    
    return runner.assertTrue(kin.isReachable(p1)) &&
           runner.assertTrue(kin.isReachable(p2)) &&
           runner.assertFalse(kin.isReachable(p3));
}

// Round-trip Tests
bool TestKinematics::testRoundTrip_Simple() {
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Start with angles, go forward, then inverse, should get similar angles
    JointAngles original(45.0f, 30.0f);
    Point2D position;
    
    kin.forward(original, position);
    
    JointAngles recovered;
    if (!kin.inverse(position, recovered)) {
        return false;
    }
    
    // Angles might differ by 360° or be in different configuration
    // So we verify the position is the same
    Point2D verify;
    kin.forward(recovered, verify);
    
    return runner.assertNear(position.x, verify.x, 0.1f) &&
           runner.assertNear(position.y, verify.y, 0.1f);
}

bool TestKinematics::testRoundTrip_MultipleAngles() {
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Test multiple angle combinations
    float angles1[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
    float angles2[] = {0.0f, 30.0f, 60.0f, 90.0f, 120.0f};
    
    for (int i = 0; i < 5; i++) {
        JointAngles original(angles1[i], angles2[i]);
        Point2D position;
        
        kin.forward(original, position);
        
        if (!kin.isReachable(position)) {
            continue;  // Skip unreachable positions
        }
        
        JointAngles recovered;
        if (!kin.inverse(position, recovered)) {
            return false;
        }
        
        // Verify position matches
        Point2D verify;
        kin.forward(recovered, verify);
        
        if (!runner.assertNear(position.x, verify.x, 1.0f) ||
            !runner.assertNear(position.y, verify.y, 1.0f)) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Arm Configuration Tests
// ============================================================================

bool TestKinematics::testInverse_RightElbow_PositiveX() {
    // For a positive-x target, RIGHT_ELBOW should give theta2 ≤ 0
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    Point2D target(200.0f, 100.0f);
    JointAngles angles;
    
    bool success = kin.inverse(target, angles, ArmConfig::RIGHT_ELBOW);
    if (!success) return false;
    
    // theta2 should be negative (or zero for fully extended)
    if (!runner.assertTrue(angles.theta2 <= 0.01f)) return false;
    
    // Round-trip check
    Point2D verify;
    kin.forward(angles, verify);
    return runner.assertNear(target.x, verify.x, 0.5f) &&
           runner.assertNear(target.y, verify.y, 0.5f);
}

bool TestKinematics::testInverse_LeftElbow_NegativeX() {
    // For a negative-x target, LEFT_ELBOW should give theta2 ≥ 0
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    Point2D target(-200.0f, 100.0f);
    JointAngles angles;
    
    bool success = kin.inverse(target, angles, ArmConfig::LEFT_ELBOW);
    if (!success) return false;
    
    // theta2 should be positive (or zero for fully extended)
    if (!runner.assertTrue(angles.theta2 >= -0.01f)) return false;
    
    // Round-trip check
    Point2D verify;
    kin.forward(angles, verify);
    return runner.assertNear(target.x, verify.x, 0.5f) &&
           runner.assertNear(target.y, verify.y, 0.5f);
}

bool TestKinematics::testInverse_Auto_PicksCorrectly() {
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Positive-x target: AUTO should pick RIGHT_ELBOW → theta2 ≤ 0
    {
        Point2D target(200.0f, 100.0f);
        JointAngles angles;
        if (!kin.inverse(target, angles, ArmConfig::AUTO)) return false;
        if (!runner.assertTrue(angles.theta2 <= 0.01f)) return false;
    }
    
    // Negative-x target: AUTO should pick LEFT_ELBOW → theta2 ≥ 0
    {
        Point2D target(-200.0f, 100.0f);
        JointAngles angles;
        if (!kin.inverse(target, angles, ArmConfig::AUTO)) return false;
        if (!runner.assertTrue(angles.theta2 >= -0.01f)) return false;
    }
    
    return true;
}

bool TestKinematics::testInverse_FallbackOnLimitViolation() {
    // Test that if the preferred config violates joint limits,
    // the solver falls back to the other config and still returns a valid solution.
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Force LEFT_ELBOW on a positive-x target — it may violate theta1 limits,
    // but the solver should fallback to RIGHT_ELBOW and still succeed.
    Point2D target(200.0f, 100.0f);
    JointAngles angles;
    
    // This should still succeed (via fallback)
    bool success = kin.inverse(target, angles, ArmConfig::LEFT_ELBOW);
    if (!success) {
        // If it genuinely can't reach this point in either config, that's also valid
        // but for (200,100) with L1=L2=150, it should be reachable
        return false;
    }
    
    // Round-trip check
    Point2D verify;
    kin.forward(angles, verify);
    return runner.assertNear(target.x, verify.x, 0.5f) &&
           runner.assertNear(target.y, verify.y, 0.5f);
}

bool TestKinematics::testRoundTrip_BothConfigs() {
    // Verify FK→IK→FK round-trips for both configs across workspace
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    struct TestCase {
        float x, y;
        ArmConfig config;
    };
    
    TestCase cases[] = {
        { 200.0f, 100.0f, ArmConfig::RIGHT_ELBOW},
        { 200.0f, 100.0f, ArmConfig::LEFT_ELBOW},
        {-200.0f, 100.0f, ArmConfig::RIGHT_ELBOW},
        {-200.0f, 100.0f, ArmConfig::LEFT_ELBOW},
        { 100.0f, 200.0f, ArmConfig::RIGHT_ELBOW},
        { 100.0f, 200.0f, ArmConfig::LEFT_ELBOW},
        {-100.0f, 200.0f, ArmConfig::RIGHT_ELBOW},
        {-100.0f, 200.0f, ArmConfig::LEFT_ELBOW},
    };
    
    for (int i = 0; i < 8; i++) {
        Point2D target(cases[i].x, cases[i].y);
        JointAngles angles;
        
        bool success = kin.inverse(target, angles, cases[i].config);
        if (!success) continue;  // Some config/target combos may not be valid
        
        Point2D verify;
        kin.forward(angles, verify);
        
        if (!runner.assertNear(target.x, verify.x, 1.0f) ||
            !runner.assertNear(target.y, verify.y, 1.0f)) {
            return false;
        }
    }
    
    return true;
}

bool TestKinematics::testInverse_OnYAxis() {
    // On the Y axis (x=0), both configurations should produce the same endpoint
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    Point2D target(0.0f, 250.0f);
    
    JointAngles anglesRight, anglesLeft;
    bool successRight = kin.inverse(target, anglesRight, ArmConfig::RIGHT_ELBOW);
    bool successLeft  = kin.inverse(target, anglesLeft,  ArmConfig::LEFT_ELBOW);
    
    if (!successRight || !successLeft) return false;
    
    // Both should reach the same cartesian point
    Point2D verifyRight, verifyLeft;
    kin.forward(anglesRight, verifyRight);
    kin.forward(anglesLeft,  verifyLeft);
    
    // Both endpoints should match the target
    if (!runner.assertNear(target.x, verifyRight.x, 0.5f) ||
        !runner.assertNear(target.y, verifyRight.y, 0.5f)) return false;
    if (!runner.assertNear(target.x, verifyLeft.x, 0.5f) ||
        !runner.assertNear(target.y, verifyLeft.y, 0.5f)) return false;
    
    // theta2 signs should be opposite (or both zero for fully extended)
    // RIGHT_ELBOW: theta2 ≤ 0, LEFT_ELBOW: theta2 ≥ 0
    if (!runner.assertTrue(anglesRight.theta2 <= 0.01f)) return false;
    if (!runner.assertTrue(anglesLeft.theta2  >= -0.01f)) return false;
    
    return true;
}

bool TestKinematics::testInverse_ReportsUsedConfig() {
    // Verify that the 4-arg inverse() overload correctly reports which config was used
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Case 1: preferred config is RIGHT_ELBOW, point is reachable → usedConfig = RIGHT_ELBOW
    {
        Point2D target(200.0f, 100.0f);
        JointAngles angles;
        ArmConfig usedConfig;
        bool ok = kin.inverse(target, angles, ArmConfig::RIGHT_ELBOW, usedConfig);
        if (!ok) return false;
        if (!runner.assertTrue(usedConfig == ArmConfig::RIGHT_ELBOW)) return false;
        if (!runner.assertTrue(angles.theta2 <= 0.01f)) return false;
    }
    
    // Case 2: preferred config is LEFT_ELBOW, point is reachable → usedConfig = LEFT_ELBOW
    {
        Point2D target(-200.0f, 100.0f);
        JointAngles angles;
        ArmConfig usedConfig;
        bool ok = kin.inverse(target, angles, ArmConfig::LEFT_ELBOW, usedConfig);
        if (!ok) return false;
        if (!runner.assertTrue(usedConfig == ArmConfig::LEFT_ELBOW)) return false;
        if (!runner.assertTrue(angles.theta2 >= -0.01f)) return false;
    }
    
    // Case 3: preferred config fails, fallback succeeds → usedConfig = fallback config
    {
        // Force LEFT_ELBOW on a +x target. LEFT_ELBOW may fail joint limits for this point
        // and the solver falls back to RIGHT_ELBOW, which must be reported in usedConfig.
        Point2D target(200.0f, 100.0f);
        JointAngles angles;
        ArmConfig usedConfig;
        bool ok = kin.inverse(target, angles, ArmConfig::LEFT_ELBOW, usedConfig);
        if (!ok) return false;
        // The solver found SOME valid solution — verify usedConfig matches actual theta2 sign
        if (usedConfig == ArmConfig::RIGHT_ELBOW) {
            if (!runner.assertTrue(angles.theta2 <= 0.01f)) return false;
        } else {
            if (!runner.assertTrue(angles.theta2 >= -0.01f)) return false;
        }
    }
    
    return true;
}

bool TestKinematics::testLazyConfigSwitching() {
    // Simulate the lazy switching strategy over a sequence of points.
    // Config should only change when the current config can't reach the next point.
    Kinematics kin(150.0f, 150.0f);
    TestRunner runner(false);
    
    // Sequence: starts on +x side, crosses to -x side, comes back
    // RIGHT_ELBOW should work for all +x points without switching.
    // Only one switch expected when we move into -x territory.
    struct Point { float x, y; };
    Point sequence[] = {
        { 200.0f, 100.0f},  // +x → RIGHT_ELBOW
        { 150.0f, 150.0f},  // +x → RIGHT_ELBOW
        { 100.0f, 200.0f},  // +x → RIGHT_ELBOW
        {-100.0f, 200.0f},  // -x → may need LEFT_ELBOW
        {-200.0f, 100.0f},  // -x → LEFT_ELBOW
        {-150.0f, 150.0f},  // -x → LEFT_ELBOW
        { 100.0f, 200.0f},  // +x → may need RIGHT_ELBOW
        { 200.0f, 100.0f},  // +x → RIGHT_ELBOW
    };
    const int N = 8;

    ArmConfig currentConfig = ArmConfig::RIGHT_ELBOW;
    int switches = 0;
    
    for (int i = 0; i < N; i++) {
        Point2D target(sequence[i].x, sequence[i].y);
        JointAngles angles;
        ArmConfig usedConfig;
        
        bool ok = kin.inverse(target, angles, currentConfig, usedConfig);
        if (!ok) return false;  // All points must be reachable
        
        // Round-trip check
        Point2D verify;
        kin.forward(angles, verify);
        if (!runner.assertNear(target.x, verify.x, 1.0f) ||
            !runner.assertNear(target.y, verify.y, 1.0f)) return false;
        
        // Count lazy switches
        if (usedConfig != currentConfig) {
            switches++;
            currentConfig = usedConfig;
        }
    }
    
    // Expect exactly 2 switches: one going into -x, one coming back to +x
    // (The exact number depends on joint limits, but it must be ≤ 2 and > 0)
    return runner.assertTrue(switches <= 2) && runner.assertTrue(switches > 0);
}
