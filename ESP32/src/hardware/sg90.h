#ifndef SG90_H
#define SG90_H

#include <Arduino.h>

// Use the project Config.h (in ESP32/src/) so TOOL_SERVO_PIN / TOOL_SWITCH_PIN are defined.
#include "../Config.h"

class SG90 {
public:
    /**
     * @param servoPin Servo signal pin (defaults to TOOL_SERVO_PIN).
     * @param switchPin Limit switch / pressure sensor pin (defaults to TOOL_SWITCH_PIN).
     * @param ledcChannel LEDC channel to use for PWM (0-15). Defaults to 0.
     */
    SG90(int servoPin = TOOL_SERVO_PIN, int switchPin = TOOL_SWITCH_PIN, int ledcChannel = 0);

    /**
     * Write an absolute angle to the servo (0 - 180).
     */
    void write(float angle);

    /**
     * Move the servo downwards by one step increment.
     * @param stepDeg Angle increment per call.
     * @return true if motion complete (switch pressed or angle at 0), false otherwise.
     */
    bool stepDown(float stepDeg);

    /**
     * Move the servo upwards by one step increment toward target angle.
     * @param stepDeg Angle increment per call.
     * @param targetAngle Target angle to reach (0-180).
     * @return true if target reached, false otherwise.
     */
    bool stepUp(float stepDeg, float targetAngle = 180.0f);

    /**
     * Get the current angle of the servo.
     */
    float getAngle();

    bool down(int stepDelayMs = 20);

    bool up(float degrees = 60.0f, int stepDelayMs = 20);

private:
    int _pin;
    int _switchPin;
    int _ledcChannel;
    float _angle;

    uint32_t angleToDuty(float angle) const;
};

#endif
