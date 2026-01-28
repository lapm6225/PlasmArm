#ifndef RUN_TESTS_H
#define RUN_TESTS_H

#include "TestRunner.h"
#include "TestKinematics.h"
#include "TestPlanner.h"
#include "TestTypes.h"
#include "TestStepperMotor.h"
#include "TestVisual.h"

/**
 * @file RunTests.h
 * @brief Main test runner that executes all test suites
 */

void runAllUnitTests();
void runVisualTestsOnly();

#endif // RUN_TESTS_H
