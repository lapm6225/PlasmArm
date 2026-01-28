#include "StepperMotor.h"
#include "../Config.h"

StepperMotor::StepperMotor(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin)
    : stepPin(stepPin), dirPin(dirPin), enablePin(enablePin),
      currentAngle(0.0f), targetAngle(0.0f), speed(100.0f),
      enabled(false), isMovingFlag(false),
      currentStep(0), targetStep(0),
      lastStepTime(0), stepInterval(0) {
}

void StepperMotor::init() {
    pinMode(stepPin, OUTPUT);
    pinMode(dirPin, OUTPUT);
    pinMode(enablePin, OUTPUT);
    
    digitalWrite(stepPin, LOW);
    digitalWrite(dirPin, LOW);
    disable();  // Start disabled
    
    // Calculate initial step interval based on speed
    if (speed > 0) {
        stepInterval = (unsigned long)(1000000.0f / speed);  // microseconds per step
    }
}

void StepperMotor::setSpeed(float speed) {
    this->speed = speed;
    if (speed > 0) {
        stepInterval = (unsigned long)(1000000.0f / speed);
    } else {
        stepInterval = 0;
    }
}

void StepperMotor::moveToAngle(float angle) {
    // Normalize angle to 0-360 range
    while (angle < 0) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    
    targetAngle = angle;
    targetStep = angleToSteps(angle);
    
    // Set direction
    if (targetStep > currentStep) {
        digitalWrite(dirPin, HIGH);
    } else {
        digitalWrite(dirPin, LOW);
    }
    
    isMovingFlag = (targetStep != currentStep);
}

float StepperMotor::getCurrentAngle() {
    return currentAngle;
}

void StepperMotor::enable() {
    digitalWrite(enablePin, LOW);  // Active low on most drivers
    enabled = true;
}

void StepperMotor::disable() {
    digitalWrite(enablePin, HIGH);
    enabled = false;
    isMovingFlag = false;
}

bool StepperMotor::isEnabled() {
    return enabled;
}

bool StepperMotor::isMoving() {
    return isMovingFlag && enabled;
}

void StepperMotor::stop() {
    targetStep = currentStep;
    targetAngle = currentAngle;
    isMovingFlag = false;
}

void StepperMotor::update() {
    if (!enabled || !isMovingFlag) {
        return;
    }
    
    unsigned long currentTime = micros();
    
    // Check if it's time for the next step
    if (currentTime - lastStepTime >= stepInterval) {
        if (currentStep != targetStep) {
            // Generate step pulse
            digitalWrite(stepPin, HIGH);
            delayMicroseconds(2);  // Minimum pulse width (adjust based on driver)
            digitalWrite(stepPin, LOW);
            
            // Update position
            if (targetStep > currentStep) {
                currentStep++;
            } else {
                currentStep--;
            }
            
            // Update current angle
            currentAngle = stepsToAngle(currentStep);
            
            // Update direction if needed
            if (targetStep > currentStep) {
                digitalWrite(dirPin, HIGH);
            } else {
                digitalWrite(dirPin, LOW);
            }
            
            lastStepTime = currentTime;
        } else {
            // Reached target
            isMovingFlag = false;
            currentAngle = targetAngle;
        }
    }
}

long StepperMotor::angleToSteps(float angle) {
    return (long)(angle * STEPS_PER_DEGREE);
}

float StepperMotor::stepsToAngle(long steps) {
    return (float)steps / STEPS_PER_DEGREE;
}
