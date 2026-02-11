#ifndef TEST_VISUAL_H
#define TEST_VISUAL_H

#include "TestRunner.h"
#include "../core/Kinematics.h"
#include "../core/Planner.h"
#include "../core/Types.h"
#include <queue>

/**
 * @file TestVisual.h
 * @brief Visual tests that print detailed information for debugging
 * 
 * These tests are designed to be easy to visualize and understand.
 * They print interpolation points, angles, and verify round-trip accuracy.
 */

class TestVisual {
public:
    static void runAllTests(TestRunner& runner);
    
    // Visual tests that print detailed output
    static void testInterpolationVisual();
    static void testAngleToPositionVisual();
    static void testPositionToAngleVisual();
    static void testFullPathVisual();
    
private:
    static void printPoint(const Point2D& p, const char* label = "");
    static void printAngles(const JointAngles& angles, const char* label = "");
    static void printSeparator();
};

#endif // TEST_VISUAL_H
