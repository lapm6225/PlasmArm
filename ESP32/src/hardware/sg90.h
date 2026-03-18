#ifndef SG90_H
#define SG90_H

#include <Arduino.h>

// Use the project Config.h (in ESP32/src/) so TOOL_SERVO_PIN / TOOL_SWITCH_PIN are defined.
// Avoid accidentally including ESP32/include/config.h which does not define those macros.
#include "../Config.h"

class SG90 {
public:
    /**
     * @param servoPin Servo signal pin (defaults to TOOL_SERVO_PIN).
     * @param switchPin Limit switch / pressure sensor pin (defaults to TOOL_SWITCH_PIN).
     * @param ledcChannel LEDC channel to use for PWM (0-15). Defaults to 0.
     */
    SG90(int servoPin = TOOL_SERVO_PIN,
         int switchPin = TOOL_SWITCH_PIN,
         int ledcChannel = 0);

    /**
     * Write an absolute angle to the servo.
     */
    void write(int angle);

    /**
     * Move the servo towards the "down" direction until the configured limit switch becomes active.
     * This function does not enforce an angle limit; it is expected that the caller uses a limit switch
     * to stop movement.
     */
    void sg_down(int activeState = HIGH, int stepDelayMs = 20);

    /**
     * Move the servo "up" by a few degrees (default 10°) from the current position.
     */
    void sg_up(int degrees = 10, int stepDelayMs = 20);

private:
    int _pin;
    int _switchPin;
    int _ledcChannel;
    int _angle;

    uint32_t angleToDuty(int angle) const;
};

#endif
