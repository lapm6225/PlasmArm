#ifndef TEST_STEPPER_MOTOR_H
#define TEST_STEPPER_MOTOR_H

#include "TestRunner.h"
#include "../hardware/StepperMotor.h"

/**
 * @file TestStepperMotor.h
 * @brief Unit tests for StepperMotor module
 * 
 * Note: These tests simulate motor behavior without actual hardware
 */

class TestStepperMotor {
public:
    static void runAllTests(TestRunner& runner);
    
private:
    // Initialization tests
    static bool testInit();
    static bool testEnableDisable();
    
    // Angle conversion tests
    static bool testAngleToSteps();
    static bool testStepsToAngle();
    
    // Movement tests (simulated)
    static bool testMoveToAngle();
    static bool testGetCurrentAngle();
    static bool testIsMoving();
    
    // Speed tests
    static bool testSetSpeed();
};

#endif // TEST_STEPPER_MOTOR_H
