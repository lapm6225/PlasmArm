#include "TestEffector.h"
#include <Arduino.h>
#include "../hardware/sg90.h"

// Global effector instance for testing
static SG90* effector = nullptr;

void runEffectorTest() {
    Serial.println("\n\n=====================================");
    Serial.println("  Initialisation du Test d'Effecteur ");
    Serial.println("=====================================");

    // Configuraion par defaut: Servo et Switch configures par les macros
    effector = new SG90(TOOL_SERVO_PIN, TOOL_SWITCH_PIN);

    Serial.print("Effecteur instancie. (Servo: pin ");
    Serial.print(TOOL_SERVO_PIN);
    Serial.print(", Switch: pin ");
    Serial.print(TOOL_SWITCH_PIN);
    Serial.println(")");
    Serial.println("Commandes disponibles via le Serial Monitor:");
    Serial.println("  'd' : Lancer effector->down() (descendre jusqu'au contact avec la switch)");
    Serial.println("  'u' : Lancer effector->up() (remonter de 20 degres)");
    Serial.println("  'a<num>' : Aller a un angle specifique (ex: a90)");
    Serial.println("  's' : Lire l'etat de la switch");

    while (true) {
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();

            if (cmd == "d") {
                Serial.println("Demarrage descente (stepDown)...");
                while (!effector->stepDown(TOOL_STEP_DEG)) {
                    Serial.print("Angle: ");
                    Serial.println(effector->getAngle());
                    delay(10);
                }
                Serial.print("Descente terminee. Angle actuel: ");
                Serial.println(effector->getAngle());
            } else if (cmd == "u") {
                Serial.println("Demarrage montee (stepUp) vers 175...");
                while (!effector->stepUp(TOOL_STEP_DEG, 175)) {
                    Serial.print("Angle: ");
                    Serial.println(effector->getAngle());
                    delay(10);
                }
                Serial.print("Montee terminee. Angle actuel: ");
                Serial.println(effector->getAngle());
            } else if (cmd.startsWith("a")) {
                int targetAngle = cmd.substring(1).toInt();
                Serial.print("Deplacement vers l'angle: ");
                Serial.println(targetAngle);
                effector->write(targetAngle);
            } else if (cmd == "s") {
                int state = digitalRead(TOOL_SWITCH_PIN);
                Serial.print("Etat de la switch: ");
                Serial.print(state);
                Serial.println(state == LOW ? " (Appuye / LOW)" : " (Non appuye / HIGH)");
            } else if (cmd.length() > 0) {
                Serial.println("Commande non reconnue.");
            }
        }
        delay(10);
    }
}
