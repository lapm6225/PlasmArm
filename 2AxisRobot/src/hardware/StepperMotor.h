#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "IMotor.h"
#include <Arduino.h>

/**
 * @file StepperMotor.h
 * @brief Stepper motor implementation using STEP/DIR interface
 * 
 * This class implements the IMotor interface for stepper motors
 * controlled via STEP and DIR pins (common with drivers like A4988, DRV8825).
 */

class StepperMotor : public IMotor {
private:
    uint8_t stepPin;
    uint8_t dirPin;
    uint8_t enablePin;
    
    float currentAngle;      // Current angle in degrees
    float targetAngle;       // Target angle in degrees
    float speed;             // Speed in steps per second
    bool enabled;            // Motor enable state
    bool isMovingFlag;       // Movement status
    
    // Step generation variables
    long currentStep;        // Current step position
    long targetStep;         // Target step position
    unsigned long lastStepTime;  // Last step timestamp (microseconds)
    unsigned long stepInterval;  // Time between steps (microseconds)
    
    // Calculate steps from angle
    long angleToSteps(float angle);
    float stepsToAngle(long steps);
    
public:
    /**
     * @brief Constructor
     * @param stepPin GPIO pin for STEP signal
     * @param dirPin GPIO pin for DIR signal
     * @param enablePin GPIO pin for ENABLE signal
     */
    StepperMotor(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin);
    
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

#endif // STEPPER_MOTOR_H
