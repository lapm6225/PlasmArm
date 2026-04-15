#include "hardware/DynamixelController.h"
#include <Arduino.h>
#include "../Config.h"

const float DynamixelController::POSITION_TOLERANCE = 1.0f;
const float DynamixelController::DEG_TO_PULSE = 4096.0f / 360.0f;

DynamixelController::DynamixelController(HardwareSerial& serial, int dirPin) 
    : dxl(serial, dirPin), hwSerial(serial) {
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
    uint32_t step_time_ms = INTERPOLATION_INTERVAL_MS;
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
    sw_data_array[0].goal_position = (int32_t)(target1_internal * DEG_TO_PULSE);
    sw_data_array[1].goal_position = (int32_t)(target2_internal * DEG_TO_PULSE);

    sw_infos.is_info_changed = true;
    dxl.syncWrite(&sw_infos);

    moving1 = true;
    moving2 = true;
    stallCount1 = 0;
    stallCount2 = 0;
}

float DynamixelController::getAngle(uint8_t id) {
    // Single read — RX flush inside readOneMotor ensures clean data
    float raw = readOneMotor(id);
    if (raw < 0.0f) {
        Serial.printf("Warning: Failed to read motor %d\n", id);
        return 0.0f;
    }
    
    float pos = normalize(raw);
    if (id == ID_M1) {
        return internalToMech_ID1(pos);
    } else if (id == ID_M2) {
        return internalToMech_ID2(pos);
    }
    return 0.0f;
}

void DynamixelController::setTorque(bool enable) {
    if (enable) {
        dxl.torqueOn(ID_M1);
        delay(5);
        dxl.torqueOn(ID_M2);
        delay(5);
    } else {
        dxl.torqueOff(ID_M1);
        delay(5);
        dxl.torqueOff(ID_M2);
        delay(5);
    }
}

void DynamixelController::setHomeMode() {
    Serial.println("=== MODE SET HOME ===");
    setTorque(false);
    homeMode = true;
    moving1 = false;
    moving2 = false;
}

void DynamixelController::saveHome() {
    Serial.println("=== SAUVEGARDE HOME ===");
    
    // RX flush inside readOneMotor prevents cross-motor contamination.
    const int NUM_SAMPLES = 5;
    float samples1[NUM_SAMPLES];
    float samples2[NUM_SAMPLES];
    int count1 = 0, count2 = 0;
    
    // ---- Read Motor 1 ----
    Serial.println("Reading Motor 1...");
    for (int i = 0; i < NUM_SAMPLES * 2 && count1 < NUM_SAMPLES; i++) {
        delay(30);
        float val = readOneMotor(ID_M1);
        if (val >= 0.0f) {
            samples1[count1++] = val;
            Serial.printf("  M1 read %d: %.2f\n", count1, val);
        }
    }
    
    // ---- Read Motor 2 ----
    Serial.println("Reading Motor 2...");
    for (int i = 0; i < NUM_SAMPLES * 2 && count2 < NUM_SAMPLES; i++) {
        delay(30);
        float val = readOneMotor(ID_M2);
        if (val >= 0.0f) {
            samples2[count2++] = val;
            Serial.printf("  M2 read %d: %.2f\n", count2, val);
        }
    }
    
    if (count1 < 3 || count2 < 3) {
        Serial.printf("ERROR: Not enough readings (M1=%d, M2=%d). Aborting save.\n", count1, count2);
        return;
    }
    
    // Sort both arrays to extract median
    auto sortArr = [](float* arr, int n) {
        for (int i = 1; i < n; i++) {
            float key = arr[i];
            int j = i - 1;
            while (j >= 0 && arr[j] > key) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
    };
    
    sortArr(samples1, count1);
    sortArr(samples2, count2);
    
    float new_offset1 = samples1[count1 / 2];
    float new_offset2 = samples2[count2 / 2];
    float spread1 = samples1[count1 - 1] - samples1[0];
    float spread2 = samples2[count2 - 1] - samples2[0];
    
    Serial.printf("  Motor 1: %d readings, median=%.2f, spread=%.2f\n", count1, new_offset1, spread1);
    Serial.printf("  Motor 2: %d readings, median=%.2f, spread=%.2f\n", count2, new_offset2, spread2);
    
    if (spread1 > 2.0f) {
        Serial.printf("Warning: Motor 1 spread=%.2f (unstable?)\n", spread1);
    }
    if (spread2 > 2.0f) {
        Serial.printf("Warning: Motor 2 spread=%.2f (unstable?)\n", spread2);
    }

    offset_ID3 = new_offset1;
    offset_ID20 = new_offset2;
    
    target1_internal = normalize(offset_ID3);
    target2_internal = normalize(offset_ID20);
    
    // Ensure the Target Goal is updated in the motors BEFORE enabling torque.
    // This stops the motor from jolting back to its old target!
    delay(10);
    dxl.setGoalPosition(ID_M1, target1_internal, UNIT_DEGREE);
    delay(10);
    dxl.setGoalPosition(ID_M2, target2_internal, UNIT_DEGREE);
    delay(10);

    // Update the sync write buffer so it matches the current stopped position
    sw_data_array[0].goal_position = (int32_t)(target1_internal * DEG_TO_PULSE);
    sw_data_array[1].goal_position = (int32_t)(target2_internal * DEG_TO_PULSE);
    sw_infos.is_info_changed = true;
    
    // Prevent update() from immediately probing the bus
    moving1 = false;
    moving2 = false;
    
    // Now turn torque ON to hold the motors right where they are!
    setTorque(true);
    homeMode = false;
    
    Serial.printf("Home positions successfully saved: ID3=%.2f, ID20=%.2f\n", offset_ID3, offset_ID20);
}

bool DynamixelController::isMoving() {
    // Return true if either motor hasn't reached target threshold
    return (moving1 || moving2);
}

void DynamixelController::stop() {
    if (homeMode) return;
    
    // Read current positions
    float pos1 = readOneMotor(ID_M1);
    float pos2 = readOneMotor(ID_M2);
    
    if (pos1 >= 0.0f && pos2 >= 0.0f) {
        // Set goal to current position (UNIT_DEGREE)
        dxl.setGoalPosition(ID_M1, pos1, UNIT_DEGREE);
        dxl.setGoalPosition(ID_M2, pos2, UNIT_DEGREE);
        
        // Update targets so isMoving() can settle
        target1_internal = normalize(pos1);
        target2_internal = normalize(pos2);
        
        // Update sync write buffer with the stopped position
        sw_data_array[0].goal_position = (int32_t)(target1_internal * DEG_TO_PULSE);
        sw_data_array[1].goal_position = (int32_t)(target2_internal * DEG_TO_PULSE);
        sw_infos.is_info_changed = true;
    }
    
    moving1 = false;
    moving2 = false;
    Serial.println("DXL: Motors STOPPED at current position.");
}

void DynamixelController::update() {
    // Throttle updates to prevent flooding the RS485 bus
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();

    // Compare present position to target to release the moving flag
    if (!homeMode && (moving1 || moving2)) {
        if (moving1) {
            float raw1 = readOneMotor(ID_M1);
            if (raw1 >= 0.0f) {
                float pos1 = normalize(raw1);
                float diff1 = fabs(wrapAngle180(pos1 - target1_internal));
                
                if (fabs(wrapAngle180(pos1 - lastPos1)) < 0.15f) {
                    stallCount1++;
                } else {
                    stallCount1 = 0;
                }
                lastPos1 = pos1;

                if (diff1 < POSITION_TOLERANCE || stallCount1 >= 4) {
                    moving1 = false;
                    if (stallCount1 >= 4 && diff1 >= POSITION_TOLERANCE) {
                        Serial.printf("DXL: M1 stalled at diff=%.2f\n", diff1);
                    }
                }
            }
        }
        if (moving2) {
            float raw2 = readOneMotor(ID_M2);
            if (raw2 >= 0.0f) {
                float pos2 = normalize(raw2);
                float diff2 = fabs(wrapAngle180(pos2 - target2_internal));
                
                if (fabs(wrapAngle180(pos2 - lastPos2)) < 0.15f) {
                    stallCount2++;
                } else {
                    stallCount2 = 0;
                }
                lastPos2 = pos2;

                if (diff2 < POSITION_TOLERANCE || stallCount2 >= 4) {
                    moving2 = false;
                    if (stallCount2 >= 4 && diff2 >= POSITION_TOLERANCE) {
                        Serial.printf("DXL: M2 stalled at diff=%.2f\n", diff2);
                    }
                }
            }
        }
    }
}

// ==========================================
// CONVERSIONS MATHEMATIQUES
// ==========================================

float DynamixelController::normalize(float a) {
    while (a < 0.0f) a += 360.0f;
    while (a >= 360.0f) a -= 360.0f;
    return a;
}

float DynamixelController::wrapAngle180(float a) {
    while (a <= -180.0f) a += 360.0f;
    while (a > 180.0f) a -= 360.0f;
    return a;
}

float DynamixelController::mechToInternal_ID1(float a_mech) {
    // ID1: Mechanical is 0..180. Add to the offset to get internal.
    // If offset is 159.19, a mech of 0 means internal is 159.19.
    a_mech = constrain(a_mech, 0.0f, 180.0f);
    return normalize(offset_ID3 + a_mech);
}

float DynamixelController::mechToInternal_ID2(float cmd_deg) {
    // ID2: Mechanical is -150..150. Subtract from offset because ID2 is physically reversed!
    // If offset is 154.79, a mech of 0 means internal is 154.79.
    // A mech of +10 means internal is 144.79.
    cmd_deg = constrain(cmd_deg, -150.0f, 150.0f);
    return normalize(offset_ID20 - cmd_deg);
}

float DynamixelController::internalToMech_ID1(float a_int) {
    float mech = a_int - offset_ID3;
    mech = wrapAngle180(mech);
    return constrain(mech, 0.0f, 180.0f);
}

float DynamixelController::internalToMech_ID2(float a_int) {
    float mech = offset_ID20 - a_int;
    mech = wrapAngle180(mech);
    return constrain(mech, -150.0f, 150.0f);
}

float DynamixelController::readOneMotor(uint8_t id) {
    // *** ROOT CAUSE FIX ***
    // Flush the hardware serial RX buffer BEFORE reading.
    // Without this, stale response bytes from a PREVIOUS motor's response
    // linger in the ESP32's 256-byte serial FIFO. The Dynamixel2Arduino library
    // then parses those old bytes as if they were the current motor's response,
    // causing Motor 1 to return Motor 2's position (and vice versa).
    while (hwSerial.available()) {
        hwSerial.read();
    }
    delay(1); // Let any in-flight bytes arrive and drain
    while (hwSerial.available()) {
        hwSerial.read();
    }
    
    // Now read — the RX buffer is guaranteed clean
    float pos = dxl.getPresentPosition(id, UNIT_RAW);
    
    // Check for library errors
    if (dxl.getLastLibErrCode() != 0) {
        return -1.0f;
    }
    
    int32_t raw = (int32_t)pos;
    
    // Sanity check: valid range for single-turn mode
    if (raw < 0 || raw > 4095) {
        return -1.0f;
    }
    
    return (float)raw / DEG_TO_PULSE;
}
