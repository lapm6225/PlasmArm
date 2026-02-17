#include "TestInteractive.h"
#include "../Config.h"
#include <Arduino.h>

void TestInteractive::run(IMotor* motor1, IMotor* motor2) {
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║      INTERACTIVE INTEGRATION TEST                        ║");
    Serial.println("║      4D Motion: X, Y, Z, Tool                            ║");
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
    
    // Initialize tool pin
    pinMode(TOOL_PIN, OUTPUT);
    digitalWrite(TOOL_PIN, LOW);
    
    Serial.println("Motors initialized and enabled");
    Serial.println("Tool pin initialized (OFF)");
    Serial.printf("Arm lengths: L1=%.1f mm, L2=%.1f mm\n", ARM_LENGTH_1, ARM_LENGTH_2);
    Serial.printf("Max reach: %.1f mm\n", kin.getMaxReach());
    Serial.println();
    
    TargetState currentState(150.0f, 150.0f, 0.0f, false);  // Start position
    
    // Calculate initial angles
    JointAngles initialAngles;
    Point2D startXY = currentState.toPoint2D();
    if (kin.inverse(startXY, initialAngles)) {
        if (motor1) motor1->moveToAngle(initialAngles.theta1);
        if (motor2) motor2->moveToAngle(initialAngles.theta2);
        Serial.println("Initial position set");
        printState(currentState, "Current state");
        Serial.println();
        printAngles(initialAngles, "Current angles");
        Serial.println();
    }
    
    printHelp();
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("Ready for commands. Enter 'help' for available commands.");
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
                    processCommand(inputBuffer, kin, planner, motor1, motor2, currentState);
                    inputBuffer = "";
                }
            } else {
                inputBuffer += c;
            }
        }
        
        delay(1/MOTION_CONTROL_FREQUENCY*1000);  // Motion control loop delay to prevent CPU spinning
    }
}

void TestInteractive::printHelp() {
    Serial.println("\nCommands:");
    Serial.println("  x,y            - Move to position (e.g., '200,150')");
    Serial.println("  x,y,z          - Move with Z height (e.g., '200,150,10')");
    Serial.println("  x,y,z,1        - Move with tool ON  (e.g., '200,150,0,1')");
    Serial.println("  x,y,z,0        - Move with tool OFF (e.g., '200,150,10,0')");
    Serial.println("  tool on        - Turn tool ON (without moving)");
    Serial.println("  tool off       - Turn tool OFF (without moving)");
    Serial.println("  home           - Move to home position (0,0,0, tool OFF)");
    Serial.println("  pos            - Show current state and angles");
    Serial.println("  test           - Run test sequence with tool toggling");
    Serial.println("  help           - Show this help");
    Serial.println();
}

void TestInteractive::processCommand(const String& command, 
                                    Kinematics& kin, 
                                    Planner& planner,
                                    IMotor* motor1, 
                                    IMotor* motor2,
                                    TargetState& currentState) {
    String cmd = command;
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "help" || cmd == "h") {
        printHelp();
        return;
    }
    
    if (cmd == "pos" || cmd == "position") {
        printState(currentState, "Current state");
        Serial.println();
        
        JointAngles angles;
        Point2D xy = currentState.toPoint2D();
        if (kin.inverse(xy, angles)) {
            printAngles(angles, "Current angles");
            Serial.println();
            
            if (motor1 && motor2) {
                Serial.printf("Motor 1 angle: %.2f°\n", motor1->getCurrentAngle());
                Serial.printf("Motor 2 angle: %.2f°\n", motor2->getCurrentAngle());
                Serial.printf("Motor 1 moving: %s\n", motor1->isMoving() ? "YES" : "NO");
                Serial.printf("Motor 2 moving: %s\n", motor2->isMoving() ? "YES" : "NO");
            }
        }
        Serial.printf("Tool pin state: %s\n", digitalRead(TOOL_PIN) ? "ON" : "OFF");
        return;
    }
    
    if (cmd == "tool on") {
        currentState.toolActive = true;
        digitalWrite(TOOL_PIN, HIGH);
        Serial.println("✅ Tool ON");
        return;
    }
    
    if (cmd == "tool off") {
        currentState.toolActive = false;
        digitalWrite(TOOL_PIN, LOW);
        Serial.println("✅ Tool OFF");
        return;
    }
    
    if (cmd == "home") {
        TargetState homeState(0, 0, 0, false);
        executeMove(currentState, homeState, kin, planner, motor1, motor2, true);
        currentState = homeState;
        digitalWrite(TOOL_PIN, LOW);
        return;
    }
    
    if (cmd.startsWith("test")) {
        Serial.println("\nRunning test sequence (with tool toggling)...");
        
        // Square path: 2 sides with tool ON, 2 sides with tool OFF
        struct TestMove { float x, y, z; bool tool; const char* desc; };
        TestMove testMoves[] = {
            {200, 150, 0, true,  "Side 1 (Tool ON - cutting)"},
            {250, 100, 0, true,  "Side 2 (Tool ON - cutting)"},
            {200, 200, 5, false, "Side 3 (Tool OFF - rapid travel, Z raised)"},
            {150, 150, 0, false, "Side 4 (Tool OFF - rapid travel)"},
        };
        
        for (int i = 0; i < 4; i++) {
            Serial.printf("\n--- Test move %d/4: %s ---\n", i + 1, testMoves[i].desc);
            TargetState target(testMoves[i].x, testMoves[i].y, testMoves[i].z, testMoves[i].tool);
            executeMove(currentState, target, kin, planner, motor1, motor2, true);
            currentState = target;
            
            // Actuate tool
            digitalWrite(TOOL_PIN, currentState.toolActive ? HIGH : LOW);
            
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
        
        // Turn tool off at end
        digitalWrite(TOOL_PIN, LOW);
        currentState.toolActive = false;
        Serial.println("\n✅ Test sequence completed! Tool OFF.");
        return;
    }
    
    // Parse coordinates: x,y or x,y,z or x,y,z,tool
    // Remove "move " prefix if present
    String parseStr = cmd;
    if (parseStr.startsWith("move ")) {
        parseStr = parseStr.substring(5);
    }
    
    int firstComma = parseStr.indexOf(',');
    if (firstComma > 0) {
        float x = parseStr.substring(0, firstComma).toFloat();
        
        String rest = parseStr.substring(firstComma + 1);
        int secondComma = rest.indexOf(',');
        
        float y, z;
        bool tool;
        
        if (secondComma > 0) {
            y = rest.substring(0, secondComma).toFloat();
            String rest2 = rest.substring(secondComma + 1);
            int thirdComma = rest2.indexOf(',');
            
            if (thirdComma > 0) {
                z = rest2.substring(0, thirdComma).toFloat();
                tool = rest2.substring(thirdComma + 1).toInt() != 0;
            } else {
                z = rest2.toFloat();
                tool = currentState.toolActive;  // Keep current tool state
            }
        } else {
            y = rest.toFloat();
            z = currentState.z;       // Keep current Z
            tool = currentState.toolActive;  // Keep current tool state
        }
        
        TargetState target(x, y, z, tool);
        
        Serial.println("\n═══════════════════════════════════════════════════════════");
        Serial.println("MOVING TO TARGET");
        Serial.println("═══════════════════════════════════════════════════════════");
        printState(currentState, "From");
        Serial.println();
        printState(target, "To");
        Serial.println();
        
        executeMove(currentState, target, kin, planner, motor1, motor2, true);
        currentState = target;
        
        // Actuate tool
        digitalWrite(TOOL_PIN, currentState.toolActive ? HIGH : LOW);
        
        Serial.println("\n✅ Movement command completed!");
        Serial.println("═══════════════════════════════════════════════════════════\n");
        return;
    }
    
    Serial.println("❌ Unknown command. Type 'help' for available commands.");
}

void TestInteractive::executeMove(const TargetState& start,
                                  const TargetState& target,
                                  Kinematics& kin,
                                  Planner& planner,
                                  IMotor* motor1,
                                  IMotor* motor2,
                                  bool showDetails) {
    // Check if XY target is reachable
    Point2D targetXY = target.toPoint2D();
    if (!kin.isReachable(targetXY)) {
        Serial.printf("❌ Target (%.2f, %.2f) is NOT reachable!\n", target.x, target.y);
        float distance = sqrt(target.x * target.x + target.y * target.y);
        Serial.printf("   Distance from origin: %.2f mm\n", distance);
        Serial.printf("   Workspace range: %.1f - %.1f mm\n", 
                      kin.getMinReach(), kin.getMaxReach());
        return;
    }
    
    // Generate interpolation points (4D)
    std::queue<TargetState> queue;
    int numPoints = planner.planPath(start, target, queue);
    
    Serial.printf("\n📊 Interpolation: %d points | Tool: %s | Z: %.2f\n", 
                  numPoints, target.toolActive ? "ON" : "OFF", target.z);
    
    if (showDetails) {
        Serial.println("\nPoint# | X (mm)  | Y (mm)  | Z (mm) | Tool | θ1 (°)  | θ2 (°)  | Status");
        Serial.println("──────────────────────────────────────────────────────────────────────────");
    }
    
    int index = 0;
    int passed = 0;
    int failed = 0;
    
    while (!queue.empty()) {
        TargetState point = queue.front();
        queue.pop();
        
        // Calculate inverse kinematics for XY
        Point2D xy = point.toPoint2D();
        JointAngles angles;
        bool ikSuccess = kin.inverse(xy, angles);
        
        if (ikSuccess) {
            // Verify round-trip
            Point2D verify;
            kin.forward(angles, verify);
            float error = Planner::distance(xy, verify);
            bool accurate = error < 0.1f;
            
            if (accurate) passed++;
            else failed++;
            
            // Print details (every 5th point or first/last)
            if (showDetails && (index % 5 == 0 || index == 0 || index == numPoints - 1)) {
                Serial.printf("%5d | %7.2f | %7.2f | %6.2f | %s  | %7.2f | %7.2f | %s\n",
                            index, point.x, point.y, point.z,
                            point.toolActive ? " ON" : "OFF",
                            angles.theta1, angles.theta2,
                            accurate ? "✅" : "❌");
            }
            
            // Command motors to move
            if (motor1) motor1->moveToAngle(angles.theta1);
            if (motor2) motor2->moveToAngle(angles.theta2);
            
            // Update motors and wait a bit for movement
            if (motor1 && motor2) {
                for (int i = 0; i < 10; i++) {
                    motor1->update();
                    motor2->update();
                    delay(1);
                }
            }
            
        } else {
            failed++;
            if (showDetails) {
                Serial.printf("%5d | %7.2f | %7.2f | %6.2f | %s  |   FAIL   |   FAIL   | ❌\n",
                             index, point.x, point.y, point.z,
                             point.toolActive ? " ON" : "OFF");
            }
        }
        
        index++;
    }
    
    if (showDetails) {
        Serial.println("──────────────────────────────────────────────────────────────────────────");
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

void TestInteractive::printState(const TargetState& s, const char* label) {
    if (strlen(label) > 0) {
        Serial.printf("%s: ", label);
    }
    Serial.printf("(%.2f, %.2f, Z=%.2f) Tool=%s", 
                  s.x, s.y, s.z, s.toolActive ? "ON" : "OFF");
}

void TestInteractive::printAngles(const JointAngles& angles, const char* label) {
    if (strlen(label) > 0) {
        Serial.printf("%s: ", label);
    }
    Serial.printf("θ1=%.2f°, θ2=%.2f°", angles.theta1, angles.theta2);
}
