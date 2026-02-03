#include "RunTests.h"
#include "../hardware/StepperMotor.h"

void runAllUnitTests() {
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║         ESP32 SCARA ROBOT - UNIT TESTS                   ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    
    TestRunner runner(true);
    
    // Run all test suites
    TestTypes::runAllTests(runner);
    TestKinematics::runAllTests(runner);
    TestPlanner::runAllTests(runner);
    TestStepperMotor::runAllTests(runner);
    
    // Print final results
    runner.printResults();
    
    Serial.println("\nTests completed. Check results above.");
    Serial.println("Press RESET to run tests again.\n");
}

void runVisualTestsOnly() {
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║      ESP32 SCARA ROBOT - VISUAL TESTS                    ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    
    TestRunner runner(false);  // Don't print individual test results
    
    // Run only visual tests
    TestVisual::runAllTests(runner);
    
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("Visual tests completed. Review output above.");
    Serial.println("Press RESET to run tests again.\n");
}

void runInteractiveTest() {
    // Create motor instances
    StepperMotor* motor1 = new StepperMotor(MOTOR1_STEP_PIN, MOTOR1_DIR_PIN, MOTOR1_ENABLE_PIN);
    StepperMotor* motor2 = new StepperMotor(MOTOR2_STEP_PIN, MOTOR2_DIR_PIN, MOTOR2_ENABLE_PIN);
    
    // Run interactive test
    TestInteractive::run(motor1, motor2);
    
    // Cleanup (never reached in interactive mode, but good practice)
    delete motor1;
    delete motor2;
}
