#ifndef SG90_H
#define SG90_H

#include <Arduino.h>
#include <Servo.h>

// Use the project Config.h (in ESP32/src/) so TOOL_SERVO_PIN / TOOL_SWITCH_PIN are defined.
// Avoid accidentally including ESP32/include/config.h which does not define those macros.
#include "../Config.h"

class SG90 {
public:
    /**
     * @param servoPin Servo signal pin (defaults to TOOL_SERVO_PIN).
     * @param switchPin Limit switch / pressure sensor pin (defaults to TOOL_SWITCH_PIN).
     * @param minAngle Minimum angle (default 0).
     * @param maxAngle Maximum angle (default 180).
     */
    SG90(int servoPin = TOOL_SERVO_PIN,
         int switchPin = TOOL_SWITCH_PIN,
         int minAngle = 0,
         int maxAngle = 180);

    /**
     * Write an absolute angle to the servo.
     */
    void write(int angle);

    /**
     * Move the servo towards the "down" direction until the configured limit switch becomes active.
     * The function will stop at maxAngle if the switch never becomes active.
     */
    void sg_down(int activeState = HIGH, int stepDelayMs = 20);

    /**
     * Move the servo "up" by a few degrees (default 10°) from the current position.
     */
    void sg_up(int degrees = 10, int stepDelayMs = 20);

private:
    int _pin;
    int _switchPin;
    Servo _servo;
    int _angle;
    int _minAngle;
    int _maxAngle;
    int clampAngle(int angle) const;
};

#endif
