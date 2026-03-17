#include "TestDynamixelComm.h"

void TestDynamixelComm::run(DynamixelController* dxlCtrl) {
    Serial.println("\n\n");
    Serial.println("============================================================");
    Serial.println("      DYNAMIXEL COMMUNICATION TEST                          ");
    Serial.println("============================================================");
    
    if (!dxlCtrl) {
        Serial.println("Error: DynamixelController instance is null!");
        return;
    }
    
    Serial.println("Initializing Dynamixel Controller...");
    bool connected = dxlCtrl->init();
    
    if (connected) {
        Serial.println("SUCCESS: Motors responded to ping (via init function).");
    } else {
        Serial.println("WARNING: One or both motors failed to respond to ping.");
        Serial.println("Check connections, power, and baud rate.");
    }
    
    // Enable torque so we can command them
    dxlCtrl->setTorque(true);
    Serial.println("Torque enabled on both motors.");
    
    printHelp();
    
    String inputBuffer = "";
    
    while(true) {
        // Must call update() to clear the 'moving' flags if using syncWriteAngles
        dxlCtrl->update();
        
        while (Serial.available() > 0) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (inputBuffer.length() > 0) {
                    processCommand(inputBuffer, dxlCtrl);
                    inputBuffer = "";
                }
            } else {
                inputBuffer += c;
            }
        }
        delay(10);
    }
}

void TestDynamixelComm::printHelp() {
    Serial.println("\nCommands:");
    Serial.println("  ping        - Check motor connection (re-init)");
    Serial.println("  angles      - Read current angles of M1 and M2");
    Serial.println("  move t1,t2  - Move to angles t1 and t2 (e.g. 'move 90,0')");
    Serial.println("  torque on   - Enable torque");
    Serial.println("  torque off  - Disable torque (free movement)");
    Serial.println("  set-home    - Disable torque to set home manually");
    Serial.println("  save-home   - Save current position as home offset");
    Serial.println("  help        - Print this menu");
    Serial.println();
}

void TestDynamixelComm::processCommand(const String& command, DynamixelController* dxlCtrl) {
    String cmd = command;
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "help" || cmd == "h") {
        printHelp();
        return;
    }
    
    if (cmd == "ping") {
        Serial.println("Pinging motors (re-init)...");
        bool ok = dxlCtrl->init();
        if (ok) Serial.println("Ping successful.");
        else Serial.println("Ping failed.");
        return;
    }
    
    if (cmd == "angles" || cmd == "pos") {
        // Reading current angles
        float a1 = dxlCtrl->getAngle(DynamixelController::ID_M1);
        float a2 = dxlCtrl->getAngle(DynamixelController::ID_M2);
        Serial.printf("Motor 1 (ID %d): %.2f deg\n", DynamixelController::ID_M1, a1);
        Serial.printf("Motor 2 (ID %d): %.2f deg\n", DynamixelController::ID_M2, a2);
        return;
    }
    
    if (cmd == "torque on") {
        dxlCtrl->setTorque(true);
        Serial.println("Torque ON");
        return;
    }
    
    if (cmd == "torque off") {
        dxlCtrl->setTorque(false);
        Serial.println("Torque OFF");
        return;
    }
    
    if (cmd == "set-home") {
        dxlCtrl->setHomeMode();
        Serial.println("Home mode (torque off). Move motors manually, then use 'save-home'.");
        return;
    }
    
    if (cmd == "save-home") {
        dxlCtrl->saveHome();
        Serial.println("Home positions saved and torque re-enabled.");
        return;
    }
    
    if (cmd.startsWith("move ")) {
        cmd = cmd.substring(5);
        int commaIndex = cmd.indexOf(',');
        if (commaIndex > 0) {
            float t1 = cmd.substring(0, commaIndex).toFloat();
            float t2 = cmd.substring(commaIndex + 1).toFloat();
            Serial.printf("Commanding motors to: M1=%.2f deg, M2=%.2f deg\n", t1, t2);
            dxlCtrl->syncWriteAngles(t1, t2);
            
            // Wait a brief moment to allow movement to start
            for(int i=0; i<10; i++) dxlCtrl->update();
            delay(10);
            
        } else {
            Serial.println("Format error. Use: 'move t1,t2'");
        }
        return;
    }
    
    Serial.println("Unknown command. Type 'help' for options.");
}
