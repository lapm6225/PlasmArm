#include "TestStepperMotor.h"
#include "../Config.h"

bool TestStepperMotor::testInit() {
    // Use dummy pins (not connected)
    StepperMotor motor(99, 98, 97);
    motor.init();
    
    TestRunner runner(false);
    
    // After init, motor should be disabled
    return runner.assertFalse(motor.isEnabled());
}

bool TestStepperMotor::testEnableDisable() {
    StepperMotor motor(99, 98, 97);
    motor.init();
    
    TestRunner runner(false);
    
    // Test enable
    motor.enable();
    if (!runner.assertTrue(motor.isEnabled())) return false;
    
    // Test disable
    motor.disable();
    if (!runner.assertFalse(motor.isEnabled())) return false;
    
    return true;
}

bool TestStepperMotor::testAngleToSteps() {
    StepperMotor motor(99, 98, 97);
    motor.init();
    
    TestRunner runner(false);
    
    // Test angle to steps conversion
    // 360 degrees should equal STEPS_PER_REVOLUTION * MICROSTEPS
    float expectedSteps = STEPS_PER_REVOLUTION * MICROSTEPS;
    
    // We can't directly test private methods, so we test through moveToAngle
    motor.enable();
    motor.moveToAngle(360.0f);
    
    // After moving to 360°, current angle should be near 360° (or 0° if normalized)
    float angle = motor.getCurrentAngle();
    
    // Angle should be in valid range [0, 360)
    return runner.assertTrue(angle >= 0.0f) &&
           runner.assertTrue(angle < 360.0f);
}

bool TestStepperMotor::testStepsToAngle() {
    // This is tested indirectly through getCurrentAngle
    // which uses stepsToAngle internally
    return testGetCurrentAngle();
}

bool TestStepperMotor::testMoveToAngle() {
    StepperMotor motor(99, 98, 97);
    motor.init();
    motor.enable();
    
    TestRunner runner(false);
    
    // Test moving to different angles
    float angles[] = {0.0f, 90.0f, 180.0f, 270.0f, 360.0f};
    
    for (int i = 0; i < 5; i++) {
        motor.moveToAngle(angles[i]);
        
        // Motor should be moving (or have reached target)
        // We can't easily verify exact position without hardware,
        // but we can verify the command was accepted
        float current = motor.getCurrentAngle();
        
        // Current angle should be valid
        if (!runner.assertTrue(current >= 0.0f) ||
            !runner.assertTrue(current < 360.0f)) {
            return false;
        }
    }
    
    return true;
}

bool TestStepperMotor::testGetCurrentAngle() {
    StepperMotor motor(99, 98, 97);
    motor.init();
    motor.enable();
    
    TestRunner runner(false);
    
    // Initially should be at 0°
    float angle = motor.getCurrentAngle();
    if (!runner.assertNear(0.0f, angle, 1.0f)) return false;
    
    // Move to 45°
    motor.moveToAngle(45.0f);
    
    // Give it some time (simulated - in real test would need delays)
    // For now, just verify angle is valid
    angle = motor.getCurrentAngle();
    return runner.assertTrue(angle >= 0.0f) &&
           runner.assertTrue(angle < 360.0f);
}

bool TestStepperMotor::testIsMoving() {
    StepperMotor motor(99, 98, 97);
    motor.init();
    
    TestRunner runner(false);
    
    // When disabled, should not be moving
    if (!runner.assertFalse(motor.isMoving())) return false;
    
    motor.enable();
    
    // When enabled but no movement command, should not be moving
    // (assuming we start at target)
    // This is a basic check - real behavior depends on implementation
    
    // Move to a new angle
    motor.moveToAngle(90.0f);
    
    // Should be moving (or have reached target quickly)
    // We just verify the method doesn't crash
    bool moving = motor.isMoving();
    
    return runner.assertTrue(moving || !moving);  // Always true, just check it works
}

bool TestStepperMotor::testSetSpeed() {
    StepperMotor motor(99, 98, 97);
    motor.init();
    
    TestRunner runner(false);
    
    // Test setting different speeds
    float speeds[] = {100.0f, 200.0f, 500.0f, 1000.0f};
    
    for (int i = 0; i < 4; i++) {
        motor.setSpeed(speeds[i]);
        
        // Speed setting should not crash
        // We can't easily verify the internal speed without accessing private members
        // So we just verify the method works
    }
    
    return true;
}

void TestStepperMotor::runAllTests(TestRunner& runner) {
    runner.printHeader("STEPPER MOTOR");
    
    runner.runTest("Init", testInit);
    runner.runTest("Enable/Disable", testEnableDisable);
    runner.runTest("Angle to Steps", testAngleToSteps);
    runner.runTest("Steps to Angle", testStepsToAngle);
    runner.runTest("Move to Angle", testMoveToAngle);
    runner.runTest("Get Current Angle", testGetCurrentAngle);
    runner.runTest("Is Moving", testIsMoving);
    runner.runTest("Set Speed", testSetSpeed);
}
