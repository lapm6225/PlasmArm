#include "TestInteractive.h"
#include "../Config.h"
#include <Arduino.h>

void TestInteractive::run(DynamixelController* dxlCtrl) {
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║      INTERACTIVE INTEGRATION TEST                        ║");
    Serial.println("║      With Real Motors                                    ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    
    // Initialize kinematics and planner
    Kinematics kin(ARM_LENGTH_1, ARM_LENGTH_2);
    Planner planner(DEFAULT_SPEED, ACCELERATION);
    
    // Initialize motors
    if (dxlCtrl) {
        dxlCtrl->init();
        dxlCtrl->setTorque(true);
    }
    
    Serial.println("Motors initialized and enabled");
    Serial.printf("Arm lengths: L1=%.1f mm, L2=%.1f mm\n", ARM_LENGTH_1, ARM_LENGTH_2);
    Serial.printf("Max reach: %.1f mm\n", kin.getMaxReach());
    Serial.println();
    
    // Home position is at +X axis → start with RIGHT_ELBOW config
    Point2D currentPos(ARM_LENGTH_1 + ARM_LENGTH_2, 0);
    ArmConfig currentConfig = ArmConfig::RIGHT_ELBOW;
    
    // Set initial motor position
    JointAngles initialAngles;
    if (kin.inverse(currentPos, initialAngles, currentConfig)) {
        if (dxlCtrl) dxlCtrl->syncWriteAngles(initialAngles.theta1, initialAngles.theta2);
        Serial.println("Initial position set");
        printPoint(currentPos, "Current position");
        Serial.println();
        printAngles(initialAngles, "Current angles");
        Serial.println();
        Serial.printf("Current config: %s\n",
                      currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT_ELBOW" : "LEFT_ELBOW");
    }

    // Ensure the switch pin is configured for reliable reading (pull-up so button pulls it LOW when pressed)
    pinMode(TOOL_SWITCH_PIN, INPUT_PULLUP);
    
    printHelp();
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("Ready for commands. Enter coordinates or 'help' for commands.");
    Serial.println("═══════════════════════════════════════════════════════════\n");
    
    String inputBuffer = "";
    
    while (true) {
        // Update motors
        if (dxlCtrl) dxlCtrl->update();

        Serial.println(digitalRead(TOOL_SWITCH_PIN));
        // Check for serial input
        while (Serial.available() > 0) {
            char c = Serial.read();
            
            if (c == '\n' || c == '\r') {
                if (inputBuffer.length() > 0) {
                    processCommand(inputBuffer, kin, planner, dxlCtrl, currentPos, currentConfig);
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
    Serial.println("  angle t1,t2  - Move to joint angles (t1,t2)");
    Serial.println("  home         - Move to home position (angles 0,0)");
    Serial.println("  set-home     - Deactivate motor to manually home");
    Serial.println("  save-home    - Save home angles");
    Serial.println("  pos          - Show current position, angles and config");
    Serial.println("  test         - Run test sequence");
    Serial.println("  help         - Show this help");
    Serial.println();
}

void TestInteractive::processCommand(const String& command, 
                                    Kinematics& kin, 
                                    Planner& planner,
                                    DynamixelController* dxlCtrl,
                                    Point2D& currentPos,
                                    ArmConfig& currentConfig) {
    String cmd = command;
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "help" || cmd == "h") {
        printHelp();
        return;
    }
    
    if (cmd == "pos" || cmd == "position") {
        JointAngles angles;
        
        if (dxlCtrl) {
            // Read actual angles from motors
            angles.theta1 = dxlCtrl->getAngle(DynamixelController::ID_M1);
            angles.theta2 = dxlCtrl->getAngle(DynamixelController::ID_M2);
            // Update Cartesian position based on real angles
            kin.forward(angles, currentPos);
        } else {
            // Simulate reading angles from currentPos
            kin.inverse(currentPos, angles, currentConfig);
        }
        
        printPoint(currentPos, "Current position");
        Serial.println();
        printAngles(angles, "Current angles");
        Serial.println();
        Serial.printf("Current config: %s\n",
                      currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT_ELBOW" : "LEFT_ELBOW");
        
        if (dxlCtrl) {
            Serial.printf("Motors moving: %s\n", dxlCtrl->isMoving() ? "YES" : "NO");
        }
        return;
    }

    if (cmd == "set-home") {
        Serial.println("Mode set-home activated");
        Serial.println("You can now manually move the robot to the angle (0,0) position");
        if (dxlCtrl) dxlCtrl->setHomeMode();
        return;
    }

    if (cmd == "save-home") {
        if (dxlCtrl) {
            dxlCtrl->saveHome();
            // At this point, the arm is considered to be at angle (0,0)
            JointAngles angles(0, 0);
            kin.forward(angles, currentPos);
        }
        Serial.println("Angles (0,0) have been saved");
        return;
    }

    // Use a single SG90 instance to avoid exhausting ESP32's LEDC PWM channels.
    // Creating a new Servo object repeatedly would eventually hit the 16-channel limit.
    static SG90 zServo(TOOL_SERVO_PIN, TOOL_SWITCH_PIN);

    if(cmd == "z-down"){
        Serial.println("Activating Z down sequence...");
        zServo.sg_down(LOW); 
        Serial.println("Z down sequence completed.");
        return;
    }

    if(cmd == "z-up"){
        Serial.println("Activating Z up sequence...");
        zServo.sg_up(LOW); 
        Serial.println("Z up sequence completed.");
        return;
    }
    
    if (cmd == "home") {
        Serial.println("\n═══════════════════════════════════════════════════════════");
        Serial.println("HOMING: MOVING TO ANGLES: 0.00°, 0.00°");
        Serial.println("═══════════════════════════════════════════════════════════\n");
        
        if (dxlCtrl) dxlCtrl->syncWriteAngles(0.0f, 0.0f);
        
        // Update motors to allow movement execution
        if (dxlCtrl) {
            for (int i = 0; i < 10; i++) {
                dxlCtrl->update();
                delay(1);
            }
        }
        
        // Home is at +X axis → reset to RIGHT_ELBOW config
        currentConfig = ArmConfig::RIGHT_ELBOW;
        JointAngles angles(0, 0);
        kin.forward(angles, currentPos);
        
        Serial.println("\n✅ Home command completed!");
        Serial.printf("Config reset to: RIGHT_ELBOW\n");
        Serial.println("═══════════════════════════════════════════════════════════\n");
        return;
    }
    
    if (cmd.startsWith("test")) {
        Serial.println("\nRunning test sequence...");
        Point2D testPoints[] = {
            Point2D(200, 100),
            Point2D(200, 200),
            Point2D(100, 200),
            Point2D(100, 100),
            Point2D(200, 100),
        };
        
        for (int i = 0; i < 5; i++) {
            Serial.printf("\n--- Test move %d/%d ---\n", i + 1, 5);
            executeMove(currentPos, testPoints[i], kin, planner, dxlCtrl, true, currentConfig);
            currentPos = testPoints[i];
            
            // Wait for movement to complete
            if (dxlCtrl) {
                while (dxlCtrl->isMoving()) {
                    dxlCtrl->update();
                    delay(10);
                }
                delay(1000);  // Pause between moves
            }
        }
        Serial.println("\n✅ Test sequence completed!");
        return;
    }
    
    if (cmd.startsWith("angle ")) {
        cmd = cmd.substring(6);
        int commaIndex = cmd.indexOf(',');
        if (commaIndex > 0) {
            float t1 = cmd.substring(0, commaIndex).toFloat();
            float t2 = cmd.substring(commaIndex + 1).toFloat();
            
            Serial.printf("\n═══════════════════════════════════════════════════════════\n");
            Serial.printf("MOVING TO ANGLES: %.2f°, %.2f°\n", t1, t2);
            Serial.printf("═══════════════════════════════════════════════════════════\n\n");
            
            if (dxlCtrl) dxlCtrl->syncWriteAngles(t1, t2);
            
            // Update motors to allow movement
            if (dxlCtrl) {
                for (int i = 0; i < 10; i++) {
                    dxlCtrl->update();
                    delay(1);
                }
            }
            
            // Update currentPos and infer config from the new angles
            JointAngles angles(t1, t2);
            kin.forward(angles, currentPos);
            // theta2 sign tells us which config we're in
            currentConfig = (t2 <= 0.0f) ? ArmConfig::RIGHT_ELBOW : ArmConfig::LEFT_ELBOW;
            
            Serial.println("\n✅ Angle command completed!");
            Serial.printf("Config inferred: %s\n",
                          currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT_ELBOW" : "LEFT_ELBOW");
            Serial.println("═══════════════════════════════════════════════════════════\n");
        } else {
            Serial.println("❌ Invalid format. Use 'angle t1,t2'");
        }
        return;
    }
    
    // Parse x,y coordinates
    int commaIndex = cmd.indexOf(',');
    int spaceIndex = cmd.indexOf(' ');
    if (commaIndex > 0 || spaceIndex > 0) {
        // Remove "move " prefix if present
        if (cmd.startsWith("move ")) {
            cmd = cmd.substring(5);
            commaIndex = cmd.indexOf(',');
        }
        int separatorIndex;
        if (commaIndex > 0) {
            separatorIndex = commaIndex;
        } else if (spaceIndex > 0) {
            separatorIndex = spaceIndex;
        }
        float x = cmd.substring(0, separatorIndex).toFloat();
        float y = cmd.substring(separatorIndex + 1).toFloat();
        
        Point2D target(x, y);
        
        Serial.println("\n═══════════════════════════════════════════════════════════");
        Serial.println("MOVING TO TARGET");
        Serial.println("═══════════════════════════════════════════════════════════");
        printPoint(currentPos, "From");
        Serial.println();
        printPoint(target, "To");
        Serial.println();
        Serial.printf("Config: %s\n",
                      currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT_ELBOW" : "LEFT_ELBOW");
        
        executeMove(currentPos, target, kin, planner, dxlCtrl, true, currentConfig);
        currentPos = target; // TODO: validate actual position after move
        
        Serial.println("\n✅ Movement command completed!");
        Serial.printf("Config after move: %s\n",
                      currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT_ELBOW" : "LEFT_ELBOW");
        Serial.println("═══════════════════════════════════════════════════════════\n");
        return;
    }
    
    Serial.println("❌ Unknown command. Type 'help' for available commands.");
}

void TestInteractive::executeMove(const Point2D& start,
                                  const Point2D& target,
                                  Kinematics& kin,
                                  Planner& planner,
                                  DynamixelController* dxlCtrl,
                                  bool showDetails,
                                  ArmConfig& currentConfig) {
    // Check if target is reachable in ANY config
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
        Serial.println("\nPoint# | X (mm)  | Y (mm)  | θ1 (°)  | θ2 (°)  | Config       | Status");
        Serial.println("─────────────────────────────────────────────────────────────────────────");
    }
    
    int index = 0;
    int passed = 0;
    int failed = 0;
    int configSwitches = 0;
    
    while (!queue.empty()) {
        Point2D point = queue.front();
        queue.pop();
        
        // --- Lazy config switching: try currentConfig first ---
        JointAngles angles;
        ArmConfig usedConfig;
        bool ikSuccess = kin.inverse(point, angles, currentConfig, usedConfig);
        
        if (ikSuccess) {
            // Detect and report config switch
            if (usedConfig != currentConfig) {
                configSwitches++;
                if (showDetails) {
                    Serial.printf("       ⚡ Config switch: %s → %s at point %d (%.1f, %.1f)\n",
                                  currentConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT" : "LEFT",
                                  usedConfig    == ArmConfig::RIGHT_ELBOW ? "RIGHT" : "LEFT",
                                  index, point.x, point.y);
                }
                currentConfig = usedConfig;  // Lazy switch: adopt the new config
            }

            // Verify round-trip accuracy
            Point2D verify;
            kin.forward(angles, verify);
            float error = Planner::distance(point, verify);
            bool accurate = error < 0.1f;
            
            if (accurate) passed++;
            else failed++;
            
            // Print details (every 5th point or first/last)
            if (showDetails && (index % 5 == 0 || index == 0 || index == numPoints - 1)) {
                Serial.printf("%5d | %7.2f | %7.2f | %7.2f | %7.2f | %-12s | %s\n",
                            index, point.x, point.y,
                            angles.theta1, angles.theta2,
                            usedConfig == ArmConfig::RIGHT_ELBOW ? "RIGHT_ELBOW" : "LEFT_ELBOW",
                            accurate ? "✅" : "❌");
            }

            // Command motors to move
            if (dxlCtrl) dxlCtrl->syncWriteAngles(angles.theta1, angles.theta2);
            
            // Update motors and wait a bit for movement
            if (dxlCtrl) {
                for (int i = 0; i < 10; i++) {
                    dxlCtrl->update();
                    delay(1);
                }
            }
            
        } else {
            failed++;
            if (showDetails) {
                Serial.printf("%5d | %7.2f | %7.2f |   FAIL   |   FAIL   | ---          | ❌\n",
                             index, point.x, point.y);
            }
        }
        
        index++;
    }
    
    if (showDetails) {
        Serial.println("─────────────────────────────────────────────────────────────────────────");
        Serial.printf("Summary: %d passed ✅, %d failed ❌, %d config switch(es)\n",
                      passed, failed, configSwitches);
    }
    
    // Wait for final movement to complete
    if (dxlCtrl) {
        Serial.println("\n⏳ Waiting for motors to reach target...");
        unsigned long startTime = millis();
        unsigned long timeout = 30000;  // 30 second timeout
        
        while (dxlCtrl->isMoving() && 
               (millis() - startTime < timeout)) {
            dxlCtrl->update();
            delay(10);
        }
        
        if (dxlCtrl->isMoving()) {
            Serial.println("⚠️  Timeout reached, motors may still be moving");
        } else {
            Serial.println("✅ Motors reached target position");
        }
        
        // Show final angles
        Serial.println();
        printAngles(JointAngles(dxlCtrl->getAngle(DynamixelController::ID_M1),
                                dxlCtrl->getAngle(DynamixelController::ID_M2)), "Final angles");
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
