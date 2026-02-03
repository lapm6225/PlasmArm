#ifndef TEST_PLANNER_H
#define TEST_PLANNER_H

#include "TestRunner.h"
#include "../core/Planner.h"
#include "../core/Types.h"
#include <queue>

/**
 * @file TestPlanner.h
 * @brief Unit tests for Planner module
 */

class TestPlanner {
public:
    static void runAllTests(TestRunner& runner);
    
private:
    // Basic interpolation tests
    static bool testPlanPath_Simple();
    static bool testPlanPath_ShortDistance();
    static bool testPlanPath_LongDistance();
    
    // Speed and timing tests
    static bool testPlanPath_SpeedVariation();
    static bool testPlanPath_InterpolationInterval();
    
    // Edge cases
    static bool testPlanPath_SamePoint();
    static bool testPlanPath_VerticalLine();
    static bool testPlanPath_HorizontalLine();
    
    // Distance calculation
    static bool testDistance_Calculation();
};

#endif // TEST_PLANNER_H
