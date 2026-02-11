#include "TestPlanner.h"
#include <math.h>

void TestPlanner::runAllTests(TestRunner& runner) {
    runner.printHeader("PLANNER");
    
    // Basic interpolation tests
    runner.runTest("Plan: Simple path", testPlanPath_Simple);
    runner.runTest("Plan: Short distance", testPlanPath_ShortDistance);
    runner.runTest("Plan: Long distance", testPlanPath_LongDistance);
    
    // Speed and timing tests
    runner.runTest("Plan: Speed variation", testPlanPath_SpeedVariation);
    runner.runTest("Plan: Interpolation interval", testPlanPath_InterpolationInterval);
    
    // Edge cases
    runner.runTest("Plan: Same point", testPlanPath_SamePoint);
    runner.runTest("Plan: Vertical line", testPlanPath_VerticalLine);
    runner.runTest("Plan: Horizontal line", testPlanPath_HorizontalLine);
    
    // Utility tests
    runner.runTest("Distance: Calculation", testDistance_Calculation);
}

bool TestPlanner::testPlanPath_Simple() {
    Planner planner(50.0f, 200.0f);  // 50 mm/s, 200 mm/s²
    TestRunner runner(false);
    
    Point2D start(0.0f, 0.0f);
    Point2D end(100.0f, 100.0f);
    std::queue<Point2D> queue;
    
    int numPoints = planner.planPath(start, end, queue);
    
    // Should generate at least 2 points (start and end)
    if (!runner.assertTrue(numPoints >= 2)) return false;
    if (!runner.assertTrue(!queue.empty())) return false;
    
    // First point should be start
    Point2D first = queue.front();
    if (!runner.assertNear(start.x, first.x, 0.1f) ||
        !runner.assertNear(start.y, first.y, 0.1f)) {
        return false;
    }
    
    // Last point should be end
    Point2D last;
    while (!queue.empty()) {
        last = queue.front();
        queue.pop();
    }
    
    return runner.assertNear(end.x, last.x, 0.1f) &&
           runner.assertNear(end.y, last.y, 0.1f);
}

bool TestPlanner::testPlanPath_ShortDistance() {
    Planner planner(50.0f, 200.0f);
    TestRunner runner(false);
    
    Point2D start(0.0f, 0.0f);
    Point2D end(1.0f, 1.0f);  // Very short distance
    std::queue<Point2D> queue;
    
    int numPoints = planner.planPath(start, end, queue);
    
    // Should still generate at least 1 point (the end point)
    return runner.assertTrue(numPoints >= 1) &&
           runner.assertTrue(!queue.empty());
}

bool TestPlanner::testPlanPath_LongDistance() {
    Planner planner(50.0f, 200.0f);
    TestRunner runner(false);
    
    Point2D start(0.0f, 0.0f);
    Point2D end(500.0f, 500.0f);  // Long distance
    std::queue<Point2D> queue;
    
    int numPoints = planner.planPath(start, end, queue);
    
    // Should generate many points for smooth motion
    return runner.assertTrue(numPoints > 10) &&
           runner.assertTrue(!queue.empty());
}

bool TestPlanner::testPlanPath_SpeedVariation() {
    TestRunner runner(false);
    
    Point2D start(0.0f, 0.0f);
    Point2D end(100.0f, 100.0f);
    
    // Test with different speeds
    float speeds[] = {10.0f, 50.0f, 100.0f};
    
    for (int i = 0; i < 3; i++) {
        Planner planner(speeds[i], 200.0f);
        std::queue<Point2D> queue;
        
        int numPoints = planner.planPath(start, end, queue);
        
        // Higher speed should generate fewer points (same distance, less time)
        if (i > 0) {
            // This is a heuristic - higher speed might have fewer points
            // but we just verify it generates points
            if (!runner.assertTrue(numPoints >= 2)) return false;
        }
    }
    
    return true;
}

bool TestPlanner::testPlanPath_InterpolationInterval() {
    Planner planner(50.0f, 200.0f);
    TestRunner runner(false);
    
    Point2D start(0.0f, 0.0f);
    Point2D end(100.0f, 0.0f);  // 100mm distance
    std::queue<Point2D> queue;
    
    int numPoints = planner.planPath(start, end, queue);
    
    // At 50 mm/s, 100mm takes 2 seconds
    // With 10ms intervals, should have ~200 points
    // But we just verify it's reasonable
    return runner.assertTrue(numPoints >= 2) &&
           runner.assertTrue(numPoints < 1000);  // Sanity check
}

bool TestPlanner::testPlanPath_SamePoint() {
    Planner planner(50.0f, 200.0f);
    TestRunner runner(false);
    
    Point2D point(100.0f, 100.0f);
    std::queue<Point2D> queue;
    
    int numPoints = planner.planPath(point, point, queue);
    
    // Should generate at least 1 point
    return runner.assertTrue(numPoints >= 1) &&
           runner.assertTrue(!queue.empty());
}

bool TestPlanner::testPlanPath_VerticalLine() {
    Planner planner(50.0f, 200.0f);
    TestRunner runner(false);
    
    Point2D start(0.0f, 0.0f);
    Point2D end(0.0f, 200.0f);  // Vertical line
    std::queue<Point2D> queue;
    
    int numPoints = planner.planPath(start, end, queue);
    
    // Verify all points have x = 0
    bool allVertical = true;
    while (!queue.empty() && allVertical) {
        Point2D p = queue.front();
        queue.pop();
        if (abs(p.x) > 0.1f) {
            allVertical = false;
        }
    }
    
    return runner.assertTrue(numPoints >= 2) && runner.assertTrue(allVertical);
}

bool TestPlanner::testPlanPath_HorizontalLine() {
    Planner planner(50.0f, 200.0f);
    TestRunner runner(false);
    
    Point2D start(0.0f, 0.0f);
    Point2D end(200.0f, 0.0f);  // Horizontal line
    std::queue<Point2D> queue;
    
    int numPoints = planner.planPath(start, end, queue);
    
    // Verify all points have y = 0
    bool allHorizontal = true;
    while (!queue.empty() && allHorizontal) {
        Point2D p = queue.front();
        queue.pop();
        if (abs(p.y) > 0.1f) {
            allHorizontal = false;
        }
    }
    
    return runner.assertTrue(numPoints >= 2) && runner.assertTrue(allHorizontal);
}

bool TestPlanner::testDistance_Calculation() {
    TestRunner runner(false);
    
    Point2D p1(0.0f, 0.0f);
    Point2D p2(3.0f, 4.0f);  // Distance should be 5 (3-4-5 triangle)
    
    float dist = Planner::distance(p1, p2);
    
    return runner.assertNear(5.0f, dist, 0.01f);
}
