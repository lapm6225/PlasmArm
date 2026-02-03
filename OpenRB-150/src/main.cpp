#include <Dynamixel2Arduino.h>

#define DXL_SERIAL Serial1
#define DEBUG_SERIAL Serial
const int DXL_DIR_PIN = -1;

const uint8_t DXL_ID_1 = 3;
const uint8_t DXL_ID_2 = 20;

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);
using namespace ControlTableItem;

const float POSITION_TOLERANCE = 1.0;

void setup() {
  delay(1500);

  DEBUG_SERIAL.begin(115200);
  while(!DEBUG_SERIAL);

  DEBUG_SERIAL.println("Initialisation Dynamixel...");

  dxl.begin(57600);
  dxl.setPortProtocolVersion(2.0);

  auto initMotor = [&](uint8_t id) {
    if (!dxl.ping(id)) {
      DEBUG_SERIAL.print("Erreur : impossible de ping l'ID ");
      DEBUG_SERIAL.println(id);
      return false;
    }
    dxl.torqueOff(id);
    dxl.setOperatingMode(id, OP_POSITION);
    dxl.torqueOn(id);
    dxl.writeControlTableItem(PROFILE_VELOCITY, id, 40);
    return true;
  };

  initMotor(DXL_ID_1);
  initMotor(DXL_ID_2);

  DEBUG_SERIAL.println("Pret. Entrez deux angles (ex: 120 45)");
}

void loop() {

  if (DEBUG_SERIAL.available()) {

    // Lecture robuste : accepte '\n' ou '\r'
    String line = DEBUG_SERIAL.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      line = DEBUG_SERIAL.readStringUntil('\r');
      line.trim();
    }

    if (line.length() == 0) return;

    // Affiche ce que tu as tapé
    DEBUG_SERIAL.print("> ");
    DEBUG_SERIAL.println(line);

    // Extraction des deux angles
    int spaceIndex = line.indexOf(' ');
    if (spaceIndex < 0) {
      DEBUG_SERIAL.println("Format invalide. Entrez deux angles (ex: 120 45)");
      return;
    }

    String a1 = line.substring(0, spaceIndex);
    String a2 = line.substring(spaceIndex + 1);

    float angle1 = a1.toFloat();
    float angle2 = a2.toFloat();

    // Vérification simple
    if ((angle1 == 0 && a1 != "0") || (angle2 == 0 && a2 != "0")) {
      DEBUG_SERIAL.println("Format invalide. Entrez deux angles (ex: 120 45)");
      return;
    }

    DEBUG_SERIAL.print("Angles recus : ");
    DEBUG_SERIAL.print(angle1);
    DEBUG_SERIAL.print("°, ");
    DEBUG_SERIAL.println(angle2);

    // Commande des deux moteurs
    dxl.setGoalPosition(DXL_ID_1, angle1, UNIT_DEGREE);
    dxl.setGoalPosition(DXL_ID_2, angle2, UNIT_DEGREE);

    // Attente de fin de course
    bool done1 = false, done2 = false;

    while (!(done1 && done2)) {
      float pos1 = dxl.getPresentPosition(DXL_ID_1, UNIT_DEGREE);
      float pos2 = dxl.getPresentPosition(DXL_ID_2, UNIT_DEGREE);

      done1 = fabs(pos1 - angle1) < POSITION_TOLERANCE;
      done2 = fabs(pos2 - angle2) < POSITION_TOLERANCE;

      delay(20);
    }

    DEBUG_SERIAL.println("DONE");
  }
}