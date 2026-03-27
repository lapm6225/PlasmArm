#include "TestInteractive2.h"
#include "../Config.h"
#include <Arduino.h>

void TestInteractive2::run(DynamixelController* dxlCtrl) {
    Serial.println("\n\n");
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║      INTERACTIVE INTEGRATION TEST 2 (G-CODE STYLE)       ║");
    Serial.println("║      Queue-based State Machine                           ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();
    
    Kinematics kin(ARM_LENGTH_1, ARM_LENGTH_2);
    Planner planner(DEFAULT_SPEED, ACCELERATION);
    
    if (dxlCtrl) {
        dxlCtrl->init();
        dxlCtrl->setTorque(true);
    }
    
    Serial.println("Motors initialized and enabled");
    Serial.printf("Arm lengths: L1=%.1f mm, L2=%.1f mm\n", ARM_LENGTH_1, ARM_LENGTH_2);
    Serial.printf("Max reach: %.1f mm\n", kin.getMaxReach());
    Serial.println();
    
    Point2D currentPos(ARM_LENGTH_1 + ARM_LENGTH_2, 0);
    ArmConfig currentConfig = ArmConfig::RIGHT_ELBOW;
    
    JointAngles initialAngles;
    if (kin.inverse(currentPos, initialAngles, currentConfig)) {
        if (dxlCtrl) dxlCtrl->syncWriteAngles(initialAngles.theta1, initialAngles.theta2);
        Serial.println("Initial position set");
        printPoint(currentPos, "Current position");
        Serial.println();
        printAngles(initialAngles, "Current angles");
        Serial.println();
    }

    pinMode(TOOL_SWITCH_PIN, INPUT_PULLUP);
    static SG90 zServo(TOOL_SERVO_PIN, TOOL_SWITCH_PIN);
    
    printHelp();
    Serial.println("\n═══════════════════════════════════════════════════════════");
    Serial.println("Ready for JSON commands. Example: {\"type\":\"TOOL\",\"state\":\"DOWN\"}");
    Serial.println("═══════════════════════════════════════════════════════════\n");
    
    String inputBuffer = "";
    
    std::queue<RobotCommand> executionQueue;
    ExecState currentState = ExecState::IDLE;
    uint32_t waitStartTime = 0;
    uint32_t waitDuration = 0;
    
    while (true) {
        // Maintien des moteurs (si nécessaire)
        if (dxlCtrl) dxlCtrl->update();

        // 1. GESTION DES COMMANDES SÉRIE (PARSING JSON)
        while (Serial.available() > 0) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (inputBuffer.length() > 0) {
                    processCommand(inputBuffer, kin, planner, dxlCtrl, currentPos, currentConfig, executionQueue);
                    inputBuffer = "";
                }
            } else {
                inputBuffer += c;
            }
        }
        
        // 2. STATE MACHINE PRINCIPALE
        
        // --- ETAT: ATTENTE (DELAY) ---
        if (currentState == ExecState::WAITING_TIME) {
            if (millis() - waitStartTime >= waitDuration) {
                currentState = ExecState::IDLE;
                Serial.println("✅ Delay terminé.");
            }
        }
        
        // --- ETAT: IDLE (PRET POUR LA PROCHAINE COMMANDE) ---
        if (currentState == ExecState::IDLE && !executionQueue.empty()) {
            RobotCommand cmd = executionQueue.front();
            executionQueue.pop();
            
            switch (cmd.type) {
                case CommandType::MOVE_TO: {
                    Serial.printf("\n➡️ EXÉCUTION MOVE_TO: (%.2f, %.2f) px/s=%.2f\n", cmd.target.x, cmd.target.y, cmd.speed);
                    // Pour l'instant on garde la vitesse courante du planner, ou on pourrait la modifier:
                    // planner = Planner(cmd.speed, ACCELERATION);
                    executeMove(currentPos, cmd.target, kin, planner, dxlCtrl, true, currentConfig);
                    currentPos = cmd.target;
                    Serial.println("✅ Move_To terminé.");
                    break;
                }
                
                case CommandType::TOOL: {
                    if (cmd.toolState == ToolState::DOWN) {
                        Serial.println("\n⬇️ EXÉCUTION TOOL: DOWN (jusqu'au feedback)");
                        zServo.down();
                    } else {
                        Serial.println("\n⬆️ EXÉCUTION TOOL: UP");
                        zServo.up(60);
                    }
                    Serial.println("✅ Outil configuré.");
                    break;
                }
                
                case CommandType::DELAY: {
                    Serial.printf("\n⏳ EXÉCUTION DELAY: %d ms\n", cmd.delayMs);
                    waitStartTime = millis();
                    waitDuration = cmd.delayMs;
                    currentState = ExecState::WAITING_TIME;
                    break;
                }
                
                case CommandType::CONFIG_CHANGE: {
                    Serial.printf("\n🔄 EXÉCUTION CONFIG_CHANGE: %s\n", cmd.newConfig == 0 ? "LEFT" : "RIGHT");
                    currentConfig = (cmd.newConfig == 0) ? ArmConfig::LEFT_ELBOW : ArmConfig::RIGHT_ELBOW;
                    // L'ESP32 note le changement, le prochain calcul de kinematics (MOVE_TO) utilisera cette nouvelle config.
                    break;
                }
            }
        }
        
        delay(5);  // Petite pause pour relâcher le CPU
    }
}

void TestInteractive2::printHelp() {
    Serial.println("\nCommandes JSON acceptées :");
    Serial.println("  {\"type\":\"MOVE_TO\", \"x\":100, \"y\":200, \"speed\":50}");
    Serial.println("  {\"type\":\"TOOL\", \"state\":\"DOWN\"}  (ou \"UP\")");
    Serial.println("  {\"type\":\"DELAY\", \"ms\": 250}");
    Serial.println("  {\"type\":\"CONFIG_CHANGE\", \"config\": 0}  (0=LEFT, 1=RIGHT)");
    Serial.println("Commandes texte de maintenance:");
    Serial.println("  pos, home, set-home, save-home");
    Serial.println();
}

void TestInteractive2::processCommand(const String& input, 
                                     Kinematics& kin, 
                                     Planner& planner,
                                     DynamixelController* dxlCtrl,
                                     Point2D& currentPos,
                                     ArmConfig& currentConfig,
                                     std::queue<RobotCommand>& executionQueue) {
    String cmd = input;
    cmd.trim();
    
    Serial.print("📥 Commande reçue (brute temp): '");
    Serial.print(cmd);
    Serial.println("'");
    
    // Détection de JSON vs commandes de debug classiques
    if (cmd.startsWith("{")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, cmd);
        
        if (error) {
            Serial.print("❌ JSON parse erreur : ");
            Serial.println(error.c_str());
            return;
        }
        
        String type = doc["type"].as<String>();
        RobotCommand newCmd;
        
        if (type == "MOVE_TO") {
            newCmd.type = CommandType::MOVE_TO;
            newCmd.target.x = doc["x"].as<float>();
            newCmd.target.y = doc["y"].as<float>();
            newCmd.speed = doc["speed"].as<float>(); // Default s'il n'y a pas
            if (newCmd.speed == 0) newCmd.speed = DEFAULT_SPEED;
            executionQueue.push(newCmd);
            Serial.println("📥 Commande MOVE_TO ajoutée à la queue.");
            
        } else if (type == "TOOL") {
            newCmd.type = CommandType::TOOL;
            String stateStr = doc["state"].as<String>();
            newCmd.toolState = (stateStr == "DOWN") ? ToolState::DOWN : ToolState::UP;
            executionQueue.push(newCmd);
            Serial.println("📥 Commande TOOL ajoutée à la queue.");
            
        } else if (type == "DELAY") {
            newCmd.type = CommandType::DELAY;
            newCmd.delayMs = doc["ms"].as<uint32_t>();
            executionQueue.push(newCmd);
            Serial.println("📥 Commande DELAY ajoutée à la queue.");
            
        } else if (type == "CONFIG_CHANGE") {
            newCmd.type = CommandType::CONFIG_CHANGE;
            newCmd.newConfig = doc["config"].as<int>();
            executionQueue.push(newCmd);
            Serial.println("📥 Commande CONFIG_CHANGE ajoutée à la queue.");
            
        } else {
            Serial.println("❌ Type de commande JSON inconnu.");
        }
        return;
    }
    
    // Fallback aux commandes de DEBUG pures et dures
    cmd.toLowerCase();
    
    if (cmd == "help" || cmd == "h") {
        printHelp();
        return;
    }
    
    if (cmd == "pos" || cmd == "position") {
        JointAngles angles;
        if (dxlCtrl) {
            angles.theta1 = dxlCtrl->getAngle(DynamixelController::ID_M1);
            delay(15); 
            angles.theta2 = dxlCtrl->getAngle(DynamixelController::ID_M2);
            kin.forward(angles, currentPos);
        } else {
            kin.inverse(currentPos, angles, currentConfig);
        }
        printPoint(currentPos, "Current position");
        Serial.println();
        printAngles(angles, "Current angles");
        Serial.println();
        Serial.printf("Queue Size: %d\n", executionQueue.size());
        return;
    }

    if (cmd == "set-home") {
        Serial.println("Mode set-home activé");
        if (dxlCtrl) dxlCtrl->setHomeMode();
        return;
    }

    if (cmd == "save-home") {
        if (dxlCtrl) dxlCtrl->saveHome();
        Serial.println("Home saved (0,0).");
        return;
    }
    
    if (cmd == "home") {
        Serial.println("HOMING...");
        if (dxlCtrl) dxlCtrl->syncWriteAngles(0.0f, 0.0f);
        return;
    }
    
    Serial.println("❌ Commande non reconnue. Tapez 'help' (ou envoyez du JSON valide).");
}

void TestInteractive2::executeMove(const Point2D& start,
                                  const Point2D& target,
                                  Kinematics& kin,
                                  Planner& planner,
                                  DynamixelController* dxlCtrl,
                                  bool showDetails,
                                  ArmConfig& currentConfig) {
    if (!kin.isReachable(target)) {
        Serial.printf("❌ Cible (%.2f, %.2f) NON atteignable!\n", target.x, target.y);
        return;
    }
    
    std::queue<Point2D> pointQueue;
    int numPoints = planner.planPath(start, target, pointQueue);
    
    int index = 0;
    while (!pointQueue.empty()) {
        Point2D point = pointQueue.front();
        pointQueue.pop();
        
        JointAngles angles;
        ArmConfig usedConfig;
        bool ikSuccess = kin.inverse(point, angles, currentConfig, usedConfig);
        
        if (ikSuccess) {
            if (usedConfig != currentConfig) {
                currentConfig = usedConfig;  
            }

            if (dxlCtrl) {
                dxlCtrl->syncWriteAngles(angles.theta1, angles.theta2);
                for (int i = 0; i < 10; i++) {
                    dxlCtrl->update();
                    delay(1);
                }
            }
        }
        index++;
    }
    
    if (dxlCtrl) {
        unsigned long startTime = millis();
        unsigned long timeout = 5000;  
        
        while (dxlCtrl->isMoving() && (millis() - startTime < timeout)) {
            dxlCtrl->update();
            delay(10);
        }
    }
}

void TestInteractive2::printPoint(const Point2D& p, const char* label) {
    if (strlen(label) > 0) Serial.printf("%s: ", label);
    Serial.printf("(%.2f, %.2f) mm", p.x, p.y);
}

void TestInteractive2::printAngles(const JointAngles& angles, const char* label) {
    if (strlen(label) > 0) Serial.printf("%s: ", label);
    Serial.printf("θ1=%.2f°, θ2=%.2f°", angles.theta1, angles.theta2);
}
