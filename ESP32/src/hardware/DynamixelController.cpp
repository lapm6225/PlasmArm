#include "hardware/DynamixelController.h"
#include <Arduino.h>

const float DynamixelController::POSITION_TOLERANCE = 1.0f;
const float DynamixelController::DEG_TO_PULSE = 4096.0f / 360.0f;

DynamixelController::DynamixelController(HardwareSerial& serial, int dirPin) 
    : dxl(serial, dirPin) {
}

bool DynamixelController::init() {
    // The hardware serial begin should usually be handled by main before init()
    // but the Dynamixel2Arduino begin gives control to the lib.
    dxl.begin(1000000);
    dxl.setPortProtocolVersion(2.0);

    // Check if motors are there
    bool ping1 = dxl.ping(ID_M1);
    bool ping2 = dxl.ping(ID_M2);
    if (!ping1 || !ping2) {
        Serial.println("Error: Could not ping one or both Dynamixel motors!");
        // return false; // We can continue but it's risky
    }

    // === CONFIGURATION MOTEURS ===
    dxl.torqueOff(ID_M1);
    dxl.setOperatingMode(ID_M1, OP_POSITION);
    // Activation du Time-Based Profile (Valeur 4 dans le registre Drive Mode)
    dxl.writeControlTableItem(ControlTableItem::DRIVE_MODE, ID_M1, 4);
    dxl.torqueOn(ID_M1);

    dxl.torqueOff(ID_M2);
    dxl.setOperatingMode(ID_M2, OP_POSITION);
    // Activation du Time-Based Profile (Valeur 4 dans le registre Drive Mode)
    dxl.writeControlTableItem(ControlTableItem::DRIVE_MODE, ID_M2, 4);
    dxl.torqueOn(ID_M2);

    // === PARAMETRAGE DU TEMPS D'INTERPOLATION ===
    // IMPORTANT : Ce temps doit correspondre à l'intervalle d'envoi du ESP32 !
    uint32_t step_time_ms = 10;
    dxl.writeControlTableItem(ControlTableItem::PROFILE_VELOCITY, ID_M1, step_time_ms);
    dxl.writeControlTableItem(ControlTableItem::PROFILE_VELOCITY, ID_M2, step_time_ms);
    dxl.writeControlTableItem(ControlTableItem::PROFILE_ACCELERATION, ID_M1, 0);
    dxl.writeControlTableItem(ControlTableItem::PROFILE_ACCELERATION, ID_M2, 0);

    // === INITIALISATION SYNCWRITE ===
    sw_infos.packet.p_buf = nullptr;
    sw_infos.packet.is_completed = false;
    sw_infos.addr = SW_START_ADDR;
    sw_infos.addr_length = SW_DATA_LEN;
    sw_infos.p_xels = info_xels;
    sw_infos.xel_count = 0;

    info_xels[0].id = ID_M1;
    info_xels[0].p_data = (uint8_t*)&sw_data_array[0].goal_position;
    sw_infos.xel_count++;

    info_xels[1].id = ID_M2;
    info_xels[1].p_data = (uint8_t*)&sw_data_array[1].goal_position;
    sw_infos.xel_count++;

    sw_infos.is_info_changed = true;
    
    Serial.println("Pret. System SCARA Dynamixel initialise avec Time-Based Profile.");
    return ping1 && ping2;
}

void DynamixelController::syncWriteAngles(float theta1_mech, float theta2_mech) {
    if (homeMode) return; // Prevent movement if in home setup mode

    float angle1_internal = mechToInternal_ID1(theta1_mech);
    float angle2_internal = mechToInternal_ID2(theta2_mech);

    target1_internal = normalize(angle1_internal);
    target2_internal = normalize(angle2_internal);

    // Convert degrees to pulses
    sw_data_array[0].goal_position = (int32_t)(angle1_internal * DEG_TO_PULSE);
    sw_data_array[1].goal_position = (int32_t)(angle2_internal * DEG_TO_PULSE);

    sw_infos.is_info_changed = true;
    dxl.syncWrite(&sw_infos);

    moving1 = true;
    moving2 = true;
}

float DynamixelController::getAngle(uint8_t id) {
    delay(2); // Small delay to prevent RS485 bus collisions after intense loops
    float pos = dxl.getPresentPosition(id, UNIT_DEGREE);
    pos = normalize(pos);
    if (id == ID_M1) {
        return internalToMech_ID1(pos);
    } else if (id == ID_M2) {
        return internalToMech_ID2(pos);
    }
    return pos;
}

void DynamixelController::setTorque(bool enable) {
    if (enable) {
        dxl.torqueOn(ID_M1);
        dxl.torqueOn(ID_M2);
    } else {
        dxl.torqueOff(ID_M1);
        dxl.torqueOff(ID_M2);
    }
}

void DynamixelController::setHomeMode() {
    Serial.println("=== MODE SET HOME ===");
    setTorque(false);
    homeMode = true;
}

void DynamixelController::saveHome() {
    Serial.println("=== SAUVEGARDE HOME ===");
    offset_ID3 = dxl.getPresentPosition(ID_M1, UNIT_DEGREE);
    offset_ID20 = dxl.getPresentPosition(ID_M2, UNIT_DEGREE);
    setTorque(true);
    homeMode = false;
}

bool DynamixelController::isMoving() {
    // Return true if either motor hasn't reached target threshold
    return (moving1 || moving2);
}

void DynamixelController::update() {
    // Compare present position to target to release the moving flag
    if (!homeMode && moving1) {
        float pos1 = normalize(dxl.getPresentPosition(ID_M1, UNIT_DEGREE));
        if (fabs(pos1 - target1_internal) < POSITION_TOLERANCE) moving1 = false;
    }
    if (!homeMode && moving2) {
        float pos2 = normalize(dxl.getPresentPosition(ID_M2, UNIT_DEGREE));
        if (fabs(pos2 - target2_internal) < POSITION_TOLERANCE) moving2 = false;
    }
}

// ==========================================
// CONVERSIONS MATHEMATIQUES
// ==========================================

float DynamixelController::normalize(float a) {
    while (a < 0) a += 360;
    while (a >= 360) a -= 360;
    return a;
}

float DynamixelController::mechToInternal_ID1(float a_mech) {
    a_mech = constrain(a_mech, 0.0f, 180.0f);
    return a_mech + offset_ID3;
}

float DynamixelController::mechToInternal_ID2(float cmd_deg) {
    cmd_deg = constrain(cmd_deg, -150.0f, 150.0f);
    return normalize(offset_ID20 + cmd_deg);
}

float DynamixelController::internalToMech_ID1(float a_int) {
    float mech = a_int - offset_ID3;
    return constrain(mech, 0.0f, 180.0f);
}

float DynamixelController::internalToMech_ID2(float a_int) {
    float mech = a_int - offset_ID20;
    if (mech < -150.0f) mech += 360.0f;
    if (mech > 150.0f) mech -= 360.0f;
    return mech;
}
