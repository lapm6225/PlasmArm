#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <Arduino.h>

/**
 * @file TestRunner.h
 * @brief Simple test framework for unit testing
 * 
 * Provides macros and utilities for running unit tests
 * and reporting results via Serial.
 */

// Test result structure
struct TestResult {
    const char* testName;
    bool passed;
    const char* message;
    float expected;
    float actual;
    float tolerance;
};

// Test statistics
struct TestStats {
    int total;
    int passed;
    int failed;
};

class TestRunner {
private:
    TestStats stats;
    bool verbose;
    
public:
    TestRunner(bool verbose = true) : verbose(verbose) {
        stats.total = 0;
        stats.passed = 0;
        stats.failed = 0;
    }
    
    // Run a test function
    void runTest(const char* name, bool (*testFunc)());
    
    // Assert functions
    bool assertTrue(bool condition, const char* message = "");
    bool assertFalse(bool condition, const char* message = "");
    bool assertEqual(float expected, float actual, float tolerance = 0.01f, const char* message = "");
    bool assertEqual(int expected, int actual, const char* message = "");
    bool assertNear(float expected, float actual, float tolerance, const char* message = "");
    
    // Print test results
    void printResults();
    void printHeader(const char* suiteName);
    void printTest(const char* testName, bool passed, const char* message = "");
    
    // Get statistics
    TestStats getStats() { return stats; }
    void reset() {
        stats.total = 0;
        stats.passed = 0;
        stats.failed = 0;
    }
};

// Macros for easier test writing
#define TEST_ASSERT_TRUE(condition) runner.assertTrue(condition, #condition)
#define TEST_ASSERT_FALSE(condition) runner.assertFalse(condition, #condition)
#define TEST_ASSERT_EQUAL(expected, actual) runner.assertEqual(expected, actual, 0.01f, #actual)
#define TEST_ASSERT_NEAR(expected, actual, tolerance) runner.assertNear(expected, actual, tolerance, #actual)

#endif // TEST_RUNNER_H
