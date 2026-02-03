#include "TestRunner.h"

void TestRunner::runTest(const char* name, bool (*testFunc)()) {
    stats.total++;
    Serial.print("  [TEST] ");
    Serial.print(name);
    Serial.print("... ");
    
    bool result = testFunc();
    
    if (result) {
        stats.passed++;
        Serial.println("PASS");
    } else {
        stats.failed++;
        Serial.println("FAIL");
    }
    
    delay(10);  // Small delay for Serial output
}

bool TestRunner::assertTrue(bool condition, const char* message) {
    if (!condition && verbose) {
        Serial.print("    ASSERT FAILED: ");
        Serial.println(message);
    }
    return condition;
}

bool TestRunner::assertFalse(bool condition, const char* message) {
    if (condition && verbose) {
        Serial.print("    ASSERT FAILED: ");
        Serial.println(message);
    }
    return !condition;
}

bool TestRunner::assertEqual(float expected, float actual, float tolerance, const char* message) {
    float diff = abs(expected - actual);
    bool result = diff <= tolerance;
    
    if (!result && verbose) {
        Serial.printf("    ASSERT FAILED: Expected %.4f, got %.4f (diff: %.4f)\n", 
                     expected, actual, diff);
        if (strlen(message) > 0) {
            Serial.print("    Message: ");
            Serial.println(message);
        }
    }
    
    return result;
}

bool TestRunner::assertEqual(int expected, int actual, const char* message) {
    bool result = (expected == actual);
    
    if (!result && verbose) {
        Serial.printf("    ASSERT FAILED: Expected %d, got %d\n", expected, actual);
        if (strlen(message) > 0) {
            Serial.print("    Message: ");
            Serial.println(message);
        }
    }
    
    return result;
}

bool TestRunner::assertNear(float expected, float actual, float tolerance, const char* message) {
    return assertEqual(expected, actual, tolerance, message);
}

void TestRunner::printResults() {
    Serial.println("\n========================================");
    Serial.println("TEST RESULTS");
    Serial.println("========================================");
    Serial.printf("Total:  %d\n", stats.total);
    Serial.printf("Passed: %d\n", stats.passed);
    Serial.printf("Failed: %d\n", stats.failed);
    
    if (stats.failed == 0) {
        Serial.println("\n✅ ALL TESTS PASSED!");
    } else {
        Serial.println("\n❌ SOME TESTS FAILED");
    }
    Serial.println("========================================\n");
}

void TestRunner::printHeader(const char* suiteName) {
    Serial.println("\n========================================");
    Serial.print("TEST SUITE: ");
    Serial.println(suiteName);
    Serial.println("========================================");
}

void TestRunner::printTest(const char* testName, bool passed, const char* message) {
    Serial.print("  [");
    Serial.print(passed ? "PASS" : "FAIL");
    Serial.print("] ");
    Serial.print(testName);
    if (strlen(message) > 0) {
        Serial.print(" - ");
        Serial.print(message);
    }
    Serial.println();
}
