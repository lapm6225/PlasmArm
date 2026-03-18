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
    SG90(int servoPin = TOOL_SERVO_PIN,
         int switchPin = TOOL_SWITCH_PIN,
         int ledcChannel = 0);

    /**
     * Write an absolute angle to the servo (0 - 180).
     */
    void write(int angle);

    /**
     * Move the servo downwards until the limit switch becomes active (LOW).
     * @param stepDelayMs Delay between each degree step to control speed.
     */
    void down(int stepDelayMs = 20);

    /**
     * Move the servo upwards by a given number of degrees from the current position.
     * @param degrees Number of degrees to move back up (default 20).
     * @param stepDelayMs Delay between each degree step to control speed.
     */
    void up(int degrees = 60, int stepDelayMs = 20);

    /**
     * Get the current angle of the servo.
     */
    int getAngle() const { return _angle; }

private:
    int _pin;
    int _switchPin;
    int _ledcChannel;
    int _angle;

    uint32_t angleToDuty(int angle) const;
};

#endif
