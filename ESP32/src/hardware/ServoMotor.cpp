#include "ServoMotor.h"
#include "../Config.h"

ServoMotor::ServoMotor(uint8_t pwmPin)
    : pwmPin(pwmPin), currentAngle(0.0f), targetAngle(0.0f),
      speed(90.0f), enabled(false), isMovingFlag(false),
      lastUpdateTime(0), lastAngle(0.0f) {
}

void ServoMotor::init() {
    servo.setPeriodHertz(50);  // Standard 50Hz servo signal
    servo.attach(pwmPin, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    disable();
}

void ServoMotor::setSpeed(float speed) {
    this->speed = speed;  // degrees per second
}

void ServoMotor::moveToAngle(float angle) {
    // Normalize angle to 0-360 range
    while (angle < 0) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    
    // Clamp to servo range (typically 0-180 degrees)
    if (angle > 180.0f) angle = 180.0f;
    if (angle < 0.0f) angle = 0.0f;
    
    targetAngle = angle;
    isMovingFlag = (abs(targetAngle - currentAngle) > 0.5f);  // 0.5 degree threshold
    lastUpdateTime = millis();
    lastAngle = currentAngle;
}

float ServoMotor::getCurrentAngle() {
    return currentAngle;
}

void ServoMotor::enable() {
    enabled = true;
    if (!isMovingFlag) {
        servo.write(currentAngle);
    }
}

void ServoMotor::disable() {
    // Note: Some servos don't have a disable, so we just stop updating
    enabled = false;
    isMovingFlag = false;
}

bool ServoMotor::isEnabled() {
    return enabled;
}

bool ServoMotor::isMoving() {
    return isMovingFlag && enabled;
}

void ServoMotor::stop() {
    targetAngle = currentAngle;
    isMovingFlag = false;
}

void ServoMotor::update() {
    if (!enabled || !isMovingFlag) {
        return;
    }
    
    unsigned long currentTime = millis();
    float deltaTime = (currentTime - lastUpdateTime) / 1000.0f;  // Convert to seconds
    
    if (deltaTime > 0) {
        // Calculate maximum angle change based on speed
        float maxAngleChange = speed * deltaTime;
        float angleDiff = targetAngle - currentAngle;
        
        // Normalize angle difference to shortest path
        if (angleDiff > 180.0f) {
            angleDiff -= 360.0f;
        } else if (angleDiff < -180.0f) {
            angleDiff += 360.0f;
        }
        
        // Move towards target
        if (abs(angleDiff) <= maxAngleChange) {
            // Reached target
            currentAngle = targetAngle;
            isMovingFlag = false;
        } else {
            // Move by maxAngleChange towards target
            if (angleDiff > 0) {
                currentAngle += maxAngleChange;
            } else {
                currentAngle -= maxAngleChange;
            }
        }
        
        // Normalize current angle
        while (currentAngle < 0) currentAngle += 360.0f;
        while (currentAngle >= 360.0f) currentAngle -= 360.0f;
        
        // Clamp to servo range
        if (currentAngle > 180.0f) currentAngle = 180.0f;
        if (currentAngle < 0.0f) currentAngle = 0.0f;
        
        // Update servo position
        servo.write((int)currentAngle);
        
        lastUpdateTime = currentTime;
        lastAngle = currentAngle;
    }
}
