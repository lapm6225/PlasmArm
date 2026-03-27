#include "RunTests.h"
#include "../hardware/DynamixelController.h"

void runAllUnitTests() {
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║         ESP32 SCARA ROBOT - UNIT TESTS                   ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    
    TestRunner runner(true);
    
    // Run all test suites
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
    DynamixelController* dxlCtrl = new DynamixelController(Serial2);
    
    // Run interactive test
    TestInteractive::run(dxlCtrl);
    
    // Cleanup (never reached in interactive mode, but good practice)
    delete dxlCtrl;
}

void runInteractiveTest2() {
    // Create motor instances
    DynamixelController* dxlCtrl = new DynamixelController(Serial2);
    
    // Run interactive test
    TestInteractive2::run(dxlCtrl);
    
    // Cleanup (never reached in interactive mode, but good practice)
    delete dxlCtrl;
}

void runDynamixelCommTest() {
    // Create motor instances
    DynamixelController* dxlCtrl = new DynamixelController(Serial2);
    
    // Run simple comm test
    TestDynamixelComm::run(dxlCtrl);
    
    // Cleanup
    delete dxlCtrl;
}
//{"type":"MOVE_TO", "x": 150, "y": 200, "speed": 100}
//{"type":"TOOL", "state":"UP"}