#include "TestVisual.h"
#include "../Config.h"
#include <math.h>

void TestVisual::runAllTests(TestRunner& runner) {
    runner.printHeader("VISUAL TESTS");
    
    Serial.println("\nThese tests print detailed information for visualization.");
    Serial.println("They help verify that interpolation and kinematics work correctly.\n");
    
    // Run visual tests (they don't return pass/fail, just print info)
    testInterpolationVisual();
    delay(1000);
    
    testAngleToPositionVisual();
    delay(1000);
    
    testPositionToAngleVisual();
    delay(1000);
    
    testFullPathVisual();
    
    Serial.println("\n✅ Visual tests completed. Review output above.\n");
}

void TestVisual::printPoint(const Point2D& p, const char* label) {
    if (strlen(label) > 0) {
        Serial.printf("%s: ", label);
    }
    Serial.printf("(%.2f, %.2f) mm", p.x, p.y);
}

void TestVisual::printAngles(const JointAngles& angles, const char* label) {
    if (strlen(label) > 0) {
        Serial.printf("%s: ", label);
    }
    Serial.printf("θ1=%.2f°, θ2=%.2f°", angles.theta1, angles.theta2);
}

void TestVisual::printSeparator() {
    Serial.println("─────────────────────────────────────────────────────────────");
}

void TestVisual::testInterpolationVisual() {
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("TEST 1: INTERPOLATION VISUALIZATION");
    Serial.println("═══════════════════════════════════════════════════════════");
    
    Planner planner(50.0f, 200.0f);  // 50 mm/s, 200 mm/s²
    
    Point2D start(100.0f, 100.0f);
    Point2D end(200.0f, 150.0f);
    
    Serial.println("\nPlanning path:");
    printPoint(start, "Start");
    Serial.println();
    printPoint(end, "End");
    Serial.println();
    Serial.printf("Speed: %.1f mm/s\n", 50.0f);
    Serial.printf("Distance: %.2f mm\n", Planner::distance(start, end));
    
    std::queue<Point2D> queue;
    int numPoints = planner.planPath(start, end, queue);
    
    Serial.printf("\nGenerated %d interpolation points:\n", numPoints);
    printSeparator();
    
    int index = 0;
    while (!queue.empty()) {
        Point2D p = queue.front();
        queue.pop();
        
        Serial.printf("Point %3d: ", index++);
        printPoint(p);
        Serial.println();
        
        // Print every 5th point or last point in detail
        if (index % 5 == 0 || index == numPoints) {
            float distFromStart = Planner::distance(start, p);
            float progress = (distFromStart / Planner::distance(start, end)) * 100.0f;
            Serial.printf("         Distance from start: %.2f mm (%.1f%%)\n", 
                         distFromStart, progress);
        }
    }
    
    printSeparator();
    Serial.println("✅ Interpolation test completed\n");
}

void TestVisual::testAngleToPositionVisual() {
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("TEST 2: ANGLE → POSITION (Forward Kinematics)");
    Serial.println("═══════════════════════════════════════════════════════════");
    
    Kinematics kin(ARM_LENGTH_1, ARM_LENGTH_2);
    
    Serial.println("\nTesting forward kinematics (angles → position):");
    Serial.printf("Arm lengths: L1=%.1f mm, L2=%.1f mm\n", ARM_LENGTH_1, ARM_LENGTH_2);
    printSeparator();
    
    // Test various angle combinations
    struct TestCase {
        float theta1;
        float theta2;
        const char* description;
    };
    
    TestCase testCases[] = {
        {0.0f, 0.0f, "Both arms straight (0°, 0°)"},
        {90.0f, 0.0f, "First arm up, second straight (90°, 0°)"},
        {45.0f, 45.0f, "Both arms at 45°"},
        {180.0f, 0.0f, "First arm left, second straight (180°, 0°)"},
        {0.0f, 90.0f, "First straight, second up (0°, 90°)"},
        {90.0f, -90.0f, "First up, second down (90°, -90°)"},
    };
    
    for (int i = 0; i < 6; i++) {
        JointAngles angles(testCases[i].theta1, testCases[i].theta2);
        Point2D position;
        
        kin.forward(angles, position);
        
        Serial.printf("\nTest %d: %s\n", i + 1, testCases[i].description);
        printAngles(angles);
        Serial.println();
        printPoint(position, "Resulting position");
        Serial.println();
        
        // Calculate distance from origin
        float distance = sqrt(position.x * position.x + position.y * position.y);
        Serial.printf("         Distance from origin: %.2f mm\n", distance);
        
        // Verify it's within workspace
        bool reachable = kin.isReachable(position);
        Serial.printf("         Within workspace: %s\n", reachable ? "YES ✅" : "NO ❌");
    }
    
    printSeparator();
    Serial.println("✅ Forward kinematics test completed\n");
}

void TestVisual::testPositionToAngleVisual() {
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("TEST 3: POSITION → ANGLES (Inverse Kinematics)");
    Serial.println("═══════════════════════════════════════════════════════════");
    
    Kinematics kin(ARM_LENGTH_1, ARM_LENGTH_2);
    
    Serial.println("\nTesting inverse kinematics (position → angles):");
    Serial.printf("Arm lengths: L1=%.1f mm, L2=%.1f mm\n", ARM_LENGTH_1, ARM_LENGTH_2);
    Serial.printf("Max reach: %.1f mm, Min reach: %.1f mm\n", 
                  kin.getMaxReach(), kin.getMinReach());
    printSeparator();
    
    // Test various positions
    struct TestCase {
        float x;
        float y;
        const char* description;
    };
    
    TestCase testCases[] = {
        {250.0f, 0.0f, "Straight right (on X axis)"},
        {0.0f, 250.0f, "Straight up (on Y axis)"},
        {150.0f, 150.0f, "Diagonal (45°)"},
        {200.0f, 100.0f, "Right and up"},
        {100.0f, 200.0f, "Left and up"},
        {180.0f, 180.0f, "Diagonal center"},
    };
    
    for (int i = 0; i < 6; i++) {
        Point2D target(testCases[i].x, testCases[i].y);
        JointAngles angles;
        
        Serial.printf("\nTest %d: %s\n", i + 1, testCases[i].description);
        printPoint(target, "Target position");
        Serial.println();
        
        // Check if reachable
        bool reachable = kin.isReachable(target);
        Serial.printf("         Reachable: %s\n", reachable ? "YES ✅" : "NO ❌");
        
        if (reachable) {
            bool success = kin.inverse(target, angles);
            
            if (success) {
                printAngles(angles, "Calculated angles");
                Serial.println();
                
                // Verify round-trip: angles → position
                Point2D verify;
                kin.forward(angles, verify);
                
                printPoint(verify, "Verification (angles → position)");
                Serial.println();
                
                // Calculate error
                float errorX = abs(target.x - verify.x);
                float errorY = abs(target.y - verify.y);
                float errorDistance = sqrt(errorX * errorX + errorY * errorY);
                
                Serial.printf("         Error: Δx=%.3f mm, Δy=%.3f mm, Distance=%.3f mm\n",
                             errorX, errorY, errorDistance);
                
                if (errorDistance < 0.1f) {
                    Serial.println("         ✅ Round-trip verification PASSED");
                } else {
                    Serial.println("         ❌ Round-trip verification FAILED");
                }
            } else {
                Serial.println("         ❌ Inverse kinematics calculation failed");
            }
        } else {
            float distance = sqrt(target.x * target.x + target.y * target.y);
            Serial.printf("         Distance from origin: %.2f mm\n", distance);
            Serial.printf("         (Outside workspace: %.1f - %.1f mm)\n",
                         kin.getMinReach(), kin.getMaxReach());
        }
    }
    
    printSeparator();
    Serial.println("✅ Inverse kinematics test completed\n");
}

void TestVisual::testFullPathVisual() {
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("TEST 4: FULL PATH VISUALIZATION");
    Serial.println("(Interpolation + Kinematics Verification)");
    Serial.println("═══════════════════════════════════════════════════════════");
    
    Kinematics kin(ARM_LENGTH_1, ARM_LENGTH_2);
    Planner planner(50.0f, 200.0f);
    
    Point2D start(150.0f, 100.0f);
    Point2D end(200.0f, 200.0f);
    
    Serial.println("\nFull path test:");
    printPoint(start, "Start");
    Serial.println();
    printPoint(end, "End");
    Serial.println();
    
    // Check if both points are reachable
    bool startReachable = kin.isReachable(start);
    bool endReachable = kin.isReachable(end);
    
    Serial.printf("Start reachable: %s\n", startReachable ? "YES ✅" : "NO ❌");
    Serial.printf("End reachable: %s\n", endReachable ? "YES ✅" : "NO ❌");
    
    if (!startReachable || !endReachable) {
        Serial.println("\n❌ Cannot proceed - points are not reachable");
        return;
    }
    
    // Calculate angles for start and end
    JointAngles startAngles, endAngles;
    kin.inverse(start, startAngles);
    kin.inverse(end, endAngles);
    
    Serial.println("\nStart and end angles:");
    printAngles(startAngles, "Start");
    Serial.println();
    printAngles(endAngles, "End");
    Serial.println();
    
    // Generate interpolation points
    std::queue<Point2D> queue;
    int numPoints = planner.planPath(start, end, queue);
    
    Serial.printf("\nInterpolated path (%d points):\n", numPoints);
    printSeparator();
    Serial.println("Point# | X (mm)  | Y (mm)  | θ1 (°)  | θ2 (°)  | Error (mm) | Status");
    printSeparator();
    
    int index = 0;
    int passed = 0;
    int failed = 0;
    float maxError = 0.0f;
    
    while (!queue.empty()) {
        Point2D target = queue.front();
        queue.pop();
        
        JointAngles calculatedAngles;
        bool ikSuccess = kin.inverse(target, calculatedAngles);
        
        if (ikSuccess) {
            // Verify round-trip
            Point2D verify;
            kin.forward(calculatedAngles, verify);
            
            float error = Planner::distance(target, verify);
            if (error > maxError) maxError = error;
            
            bool accurate = error < 0.1f;
            if (accurate) passed++;
            else failed++;
            
            // Print every 5th point or first/last
            if (index % 5 == 0 || index == 0 || index == numPoints - 1) {
                Serial.printf("%5d | %7.2f | %7.2f | %7.2f | %7.2f | %10.4f | %s\n",
                            index, target.x, target.y,
                            calculatedAngles.theta1, calculatedAngles.theta2,
                            error, accurate ? "✅" : "❌");
            }
        } else {
            failed++;
            Serial.printf("%5d | %7.2f | %7.2f |   FAIL   |   FAIL   |      -      | ❌ IK Failed\n",
                         index, target.x, target.y);
        }
        
        index++;
    }
    
    printSeparator();
    Serial.printf("\nSummary:\n");
    Serial.printf("  Total points: %d\n", numPoints);
    Serial.printf("  Passed: %d ✅\n", passed);
    Serial.printf("  Failed: %d ❌\n", failed);
    Serial.printf("  Max error: %.4f mm\n", maxError);
    
    if (failed == 0) {
        Serial.println("\n✅ All interpolation points verified successfully!");
    } else {
        Serial.printf("\n⚠️  %d points failed verification\n", failed);
    }
    
    printSeparator();
    Serial.println("✅ Full path test completed\n");
}
