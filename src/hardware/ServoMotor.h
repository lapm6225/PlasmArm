#ifndef SERVO_MOTOR_H
#define SERVO_MOTOR_H

#include "IMotor.h"
#include <Arduino.h>
#include <ESP32Servo.h>

/**
 * @file ServoMotor.h
 * @brief Smart servo motor implementation using PWM
 * 
 * This class implements the IMotor interface for servo motors
 * controlled via PWM signals (e.g., SG90, MG996R, or smart servos).
 */

class ServoMotor : public IMotor {
private:
    uint8_t pwmPin;
    Servo servo;
    
    float currentAngle;      // Current angle in degrees
    float targetAngle;       // Target angle in degrees
    float speed;             // Speed in degrees per second
    bool enabled;            // Motor enable state
    bool isMovingFlag;       // Movement status
    
    unsigned long lastUpdateTime;  // Last update timestamp (milliseconds)
    float lastAngle;              // Angle at last update
    
public:
    /**
     * @brief Constructor
     * @param pwmPin GPIO pin for PWM signal
     */
    ServoMotor(uint8_t pwmPin);
    
    // IMotor interface implementation
    void init() override;
    void setSpeed(float speed) override;
    void moveToAngle(float angle) override;
    float getCurrentAngle() override;
    void enable() override;
    void disable() override;
    bool isEnabled() override;
    bool isMoving() override;
    void stop() override;
    void update() override;
};

#endif // SERVO_MOTOR_H
