#ifndef DYNAMIXEL_CONTROLLER_H
#define DYNAMIXEL_CONTROLLER_H

#include <Arduino.h>
#include <Dynamixel2Arduino.h>

class DynamixelController {
public:
    // Constants
    static const uint8_t ID_M1 = 3;
    static const uint8_t ID_M2 = 20;

    /**
     * @brief Construct a new Dynamixel Controller
     *
     * @param serial The HardwareSerial port to use (e.g., Serial2)
     * @param dirPin The direction pin for half-duplex, or -1 if not needed
     */
    DynamixelController(HardwareSerial& serial, int dirPin = -1);

    /**
     * @brief Initialize bus, ping motors, and setup SyncWrite & Time-Based Profile
     */
    bool init();

    /**
     * @brief Send a synchronous write command for both joint angles
     * @param theta1 Mechanical angle for motor 1 (degrees)
     * @param theta2 Mechanical angle for motor 2 (degrees)
     */
    void syncWriteAngles(float theta1, float theta2);

    /**
     * @brief Get the current mechanical angle of a motor
     * @param id The motor ID (ID_M1 or ID_M2)
     * @return float Mechanical angle in degrees
     */
    float getAngle(uint8_t id);

    /**
     * @brief Enable/disable torque on both motors
     */
    void setTorque(bool enable);

    /**
     * @brief Enable set-home mode (disables torque to allow manual movement)
     */
    void setHomeMode();

    /**
     * @brief Save the current positions as the new home offsets (0,0)
     */
    void saveHome();

    /**
     * @brief Check if in home setup mode (torque off)
     */
    bool isHomeMode() { return homeMode; }

    /**
     * @brief Load home offsets from external source (e.g., NVS)
     */
    void setOffsets(float off1, float off2) {
        offset_ID3 = off1;
        offset_ID20 = off2;
        target1_internal = normalize(off1);
        target2_internal = normalize(off2);
    }

    /**
     * @brief Get current home offsets
     */
    void getOffsets(float& off1, float& off2) {
        off1 = offset_ID3;
        off2 = offset_ID20;
    }

    /**
     * @brief Check if any motor is currently moving to target
     * @return true if moving, false otherwise
     */
    bool isMoving();

    /**
     * @brief Manually update state for movement tracking
     */
    void update();

private:
    Dynamixel2Arduino dxl;
    HardwareSerial& hwSerial;  // Direct reference for RX buffer flushing

    /**
     * @brief Read a single motor's position with RX buffer flush.
     *        Flushes the hardware serial RX buffer before reading to prevent
     *        stale bytes from a previous motor's response being parsed.
     * @param id The motor ID
     * @return float Internal angle in degrees (0-360), or -1.0f on failure
     */
    float readOneMotor(uint8_t id);

    // Offsets
    float offset_ID3 = 41.0f;
    float offset_ID20 = 160.0f;

    // State
    bool homeMode = false;
    bool moving1 = false;
    bool moving2 = false;
    float target1_internal = 0.0f;
    float target2_internal = 0.0f;
    static const float POSITION_TOLERANCE;

    // SyncWrite configuration
    static const uint8_t DXL_ID_CNT = 2;
    static const uint16_t SW_START_ADDR = 116;  // Goal Position
    static const uint16_t SW_DATA_LEN = 4;
    static const float DEG_TO_PULSE;

    struct __attribute__((packed)) sw_data_t {
        int32_t goal_position;
    };

    sw_data_t sw_data_array[DXL_ID_CNT];
    DYNAMIXEL::InfoSyncWriteInst_t sw_infos;
    DYNAMIXEL::XELInfoSyncWrite_t info_xels[DXL_ID_CNT];

    // Conversion methods
    float normalize(float a);
    float wrapAngle180(float a);
    float mechToInternal_ID1(float a_mech);
    float mechToInternal_ID2(float a_mech);
    float internalToMech_ID1(float a_int);
    float internalToMech_ID2(float a_int);
};

#endif  // DYNAMIXEL_CONTROLLER_H
