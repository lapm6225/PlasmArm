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
