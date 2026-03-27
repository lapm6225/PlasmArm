#ifndef RUN_TESTS_H
#define RUN_TESTS_H

#include "TestRunner.h"
#include "TestKinematics.h"
#include "TestPlanner.h"
#include "TestStepperMotor.h"
#include "TestVisual.h"
#include "TestInteractive.h"
#include "TestInteractive2.h"
#include "TestDynamixelComm.h"
#include "TestEffector.h"

/**
 * @file RunTests.h
 * @brief Main test runner that executes all test suites
 */

void runAllUnitTests();
void runVisualTestsOnly();
void runInteractiveTest();
void runInteractiveTest2();
void runDynamixelCommTest();
void runEffectorTest();

#endif // RUN_TESTS_H
