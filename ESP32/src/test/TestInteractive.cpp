#include "TestInteractive.h"
#include "../Config.h"
#include <Arduino.h>

void TestInteractive::run(IMotor* motor1, IMotor* motor2) {
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║      INTERACTIVE INTEGRATION TEST                       ║");
    Serial.println("║      With Real Stepper Motors                           ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    
    // Initialize kinematics and planner
    Kinematics kin(ARM_LENGTH_1, ARM_LENGTH_2);
    Planner planner(DEFAULT_SPEED, ACCELERATION);
    
    // Initialize motors
    if (motor1) motor1->init();
    if (motor2) motor2->init();
    if (motor1) motor1->enable();
    if (motor2) motor2->enable();
    
    Serial.println("Motors initialized and enabled");
    Serial.printf("Arm lengths: L1=%.1f mm, L2=%.1f mm\n", ARM_LENGTH_1, ARM_LENGTH_2);
    Serial.printf("Max reach: %.1f mm\n", kin.getMaxReach());
    Serial.println();
    
    Point2D currentPos(150.0f, 150.0f);  // Start position
    
    // Calculate initial angles
    JointAngles initialAngles;
    if (kin.inverse(currentPos, initialAngles)) {
        if (motor1) motor1->moveToAngle(initialAngles.theta1);
        if (motor2) motor2->moveToAngle(initialAngles.theta2);
        Serial.println("Initial position set");
        printPoint(currentPos, "Current position");
        Serial.println();
        printAngles(initialAngles, "Current angles");
        Serial.println();
    }
    
    printHelp();
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("Ready for commands. Enter coordinates or 'help' for commands.");
    Serial.println("═══════════════════════════════════════════════════════════\n");
    
    String inputBuffer = "";
    
    while (true) {
        // Update motors
        if (motor1) motor1->update();
        if (motor2) motor2->update();
        
        // Check for serial input
        while (Serial.available() > 0) {
            char c = Serial.read();
            
            if (c == '\n' || c == '\r') {
                if (inputBuffer.length() > 0) {
                    processCommand(inputBuffer, kin, planner, motor1, motor2, currentPos);
                    inputBuffer = "";
                }
            } else {
                inputBuffer += c;
            }
        }
        
        delay(10);  // Small delay to prevent CPU spinning
    }
}

void TestInteractive::printHelp() {
    Serial.println("\nCommands:");
    Serial.println("  x,y          - Move to position (e.g., '200,150')");
    Serial.println("  move x,y     - Same as above");
    Serial.println("  home         - Move to home position (0,0)");
    Serial.println("  pos          - Show current position and angles");
    Serial.println("  test         - Run test sequence");
    Serial.println("  help         - Show this help");
    Serial.println();
}

void TestInteractive::processCommand(const String& command, 
                                    Kinematics& kin, 
                                    Planner& planner,
                                    IMotor* motor1, 
                                    IMotor* motor2,
                                    Point2D& currentPos) {
    String cmd = command;
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "help" || cmd == "h") {
        printHelp();
        return;
    }
    
    if (cmd == "pos" || cmd == "position") {
        JointAngles angles;
        if (kin.inverse(currentPos, angles)) {
            printPoint(currentPos, "Current position");
            Serial.println();
            printAngles(angles, "Current angles");
            Serial.println();
            
            if (motor1 && motor2) {
                Serial.printf("Motor 1 angle: %.2f°\n", motor1->getCurrentAngle());
                Serial.printf("Motor 2 angle: %.2f°\n", motor2->getCurrentAngle());
                Serial.printf("Motor 1 moving: %s\n", motor1->isMoving() ? "YES" : "NO");
                Serial.printf("Motor 2 moving: %s\n", motor2->isMoving() ? "YES" : "NO");
            }
        }
        return;
    }
    
    if (cmd == "home") {
        executeMove(currentPos, Point2D(0, 0), kin, planner, motor1, motor2, true);
        currentPos = Point2D(0, 0);
        return;
    }
    
    if (cmd.startsWith("test")) {
        Serial.println("\nRunning test sequence...");
        Point2D testPoints[] = {
            Point2D(200, 150),
            Point2D(250, 100),
            Point2D(200, 200),
            Point2D(150, 150),
        };
        
        for (int i = 0; i < 4; i++) {
            Serial.printf("\n--- Test move %d/%d ---\n", i + 1, 4);
            executeMove(currentPos, testPoints[i], kin, planner, motor1, motor2, true);
            currentPos = testPoints[i];
            
            // Wait for movement to complete
            if (motor1 && motor2) {
                while (motor1->isMoving() || motor2->isMoving()) {
                    motor1->update();
                    motor2->update();
                    delay(10);
                }
                delay(1000);  // Pause between moves
            }
        }
        Serial.println("\n✅ Test sequence completed!");
        return;
    }
    
    // Parse x,y coordinates
    int commaIndex = cmd.indexOf(',');
    if (commaIndex > 0) {
        // Remove "move " prefix if present
        if (cmd.startsWith("move ")) {
            cmd = cmd.substring(5);
            commaIndex = cmd.indexOf(',');
        }
        
        float x = cmd.substring(0, commaIndex).toFloat();
        float y = cmd.substring(commaIndex + 1).toFloat();
        
        Point2D target(x, y);
        
        Serial.println("\n═══════════════════════════════════════════════════════════");
        Serial.println("MOVING TO TARGET");
        Serial.println("═══════════════════════════════════════════════════════════");
        printPoint(currentPos, "From");
        Serial.println();
        printPoint(target, "To");
        Serial.println();
        
        executeMove(currentPos, target, kin, planner, motor1, motor2, true);
        currentPos = target;
        
        Serial.println("\n✅ Movement command completed!");
        Serial.println("═══════════════════════════════════════════════════════════\n");
        return;
    }
    
    Serial.println("❌ Unknown command. Type 'help' for available commands.");
}

void TestInteractive::executeMove(const Point2D& start,
                                  const Point2D& target,
                                  Kinematics& kin,
                                  Planner& planner,
                                  IMotor* motor1,
                                  IMotor* motor2,
                                  bool showDetails) {
    // Check if target is reachable
    if (!kin.isReachable(target)) {
        Serial.printf("❌ Target (%.2f, %.2f) is NOT reachable!\n", target.x, target.y);
        float distance = sqrt(target.x * target.x + target.y * target.y);
        Serial.printf("   Distance from origin: %.2f mm\n", distance);
        Serial.printf("   Workspace range: %.1f - %.1f mm\n", 
                      kin.getMinReach(), kin.getMaxReach());
        return;
    }
    
    // Generate interpolation points
    std::queue<Point2D> queue;
    int numPoints = planner.planPath(start, target, queue);
    
    Serial.printf("\n📊 Interpolation: %d points generated\n", numPoints);
    
    if (showDetails) {
        Serial.println("\nPoint# | X (mm)  | Y (mm)  | θ1 (°)  | θ2 (°)  | Status");
        Serial.println("─────────────────────────────────────────────────────────────");
    }
    
    int index = 0;
    int passed = 0;
    int failed = 0;
    
    while (!queue.empty()) {
        Point2D point = queue.front();
        queue.pop();
        
        // Calculate inverse kinematics
        JointAngles angles;
        bool ikSuccess = kin.inverse(point, angles);
        
        if (ikSuccess) {
            // Verify round-trip
            Point2D verify;
            kin.forward(angles, verify);
            float error = Planner::distance(point, verify);
            bool accurate = error < 0.1f;
            
            if (accurate) passed++;
            else failed++;
            
            // Print details (every 5th point or first/last)
            if (showDetails && (index % 5 == 0 || index == 0 || index == numPoints - 1)) {
                Serial.printf("%5d | %7.2f | %7.2f | %7.2f | %7.2f | %s\n",
                            index, point.x, point.y,
                            angles.theta1, angles.theta2,
                            accurate ? "✅" : "❌");
            }
            
            // Command motors to move
            if (motor1) motor1->moveToAngle(angles.theta1);
            if (motor2) motor2->moveToAngle(angles.theta2);
            
            // Update motors and wait a bit for movement
            if (motor1 && motor2) {
                // Update motors multiple times to allow movement
                for (int i = 0; i < 10; i++) {
                    motor1->update();
                    motor2->update();
                    delay(1);
                }
            }
            
        } else {
            failed++;
            if (showDetails) {
                Serial.printf("%5d | %7.2f | %7.2f |   FAIL   |   FAIL   | ❌\n",
                             index, point.x, point.y);
            }
        }
        
        index++;
    }
    
    if (showDetails) {
        Serial.println("─────────────────────────────────────────────────────────────");
        Serial.printf("Summary: %d passed ✅, %d failed ❌\n", passed, failed);
    }
    
    // Wait for final movement to complete
    if (motor1 && motor2) {
        Serial.println("\n⏳ Waiting for motors to reach target...");
        unsigned long startTime = millis();
        unsigned long timeout = 30000;  // 30 second timeout
        
        while ((motor1->isMoving() || motor2->isMoving()) && 
               (millis() - startTime < timeout)) {
            motor1->update();
            motor2->update();
            delay(10);
        }
        
        if (motor1->isMoving() || motor2->isMoving()) {
            Serial.println("⚠️  Timeout reached, motors may still be moving");
        } else {
            Serial.println("✅ Motors reached target position");
        }
        
        // Show final angles
        Serial.println();
        printAngles(JointAngles(motor1->getCurrentAngle(), motor2->getCurrentAngle()), "Final angles");
        Serial.println();
    }
}

void TestInteractive::printPoint(const Point2D& p, const char* label) {
    if (strlen(label) > 0) {
        Serial.printf("%s: ", label);
    }
    Serial.printf("(%.2f, %.2f) mm", p.x, p.y);
}

void TestInteractive::printAngles(const JointAngles& angles, const char* label) {
    if (strlen(label) > 0) {
        Serial.printf("%s: ", label);
    }
    Serial.printf("θ1=%.2f°, θ2=%.2f°", angles.theta1, angles.theta2);
}
