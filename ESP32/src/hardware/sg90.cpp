#include "sg90.h"

static constexpr int SERVO_FREQUENCY_HZ = 50;
static constexpr int SERVO_RESOLUTION_BITS = 12;
static constexpr int SERVO_PERIOD_US = 1000000 / SERVO_FREQUENCY_HZ;

SG90::SG90(int servoPin, int switchPin, int ledcChannel)
    : _pin(servoPin), _switchPin(switchPin), _ledcChannel(ledcChannel), _angle(120.0f) {
    pinMode(_pin, OUTPUT);
    pinMode(_switchPin, INPUT_PULLUP);
    ledcSetup(_ledcChannel, SERVO_FREQUENCY_HZ, SERVO_RESOLUTION_BITS);
    ledcAttachPin(_pin, _ledcChannel);
}

uint32_t SG90::angleToDuty(float angle) const {
    const float minPulseUs = 500.0f;
    const float maxPulseUs = 2500.0f;

    if (angle < 0.0f)
        angle = 0.0f;
    if (angle > 180.0f)
        angle = 180.0f;

    float pulseUs = angle * (maxPulseUs - minPulseUs) / 180.0f + minPulseUs;
    uint32_t maxDuty = (1u << SERVO_RESOLUTION_BITS) - 1;
    return (uint32_t)((pulseUs * (float)maxDuty) / (float)SERVO_PERIOD_US);
}

void SG90::write(float angle) {
    if (angle < 0.0f)
        angle = 0.0f;
    if (angle > 180.0f)
        angle = 180.0f;

    uint32_t duty = angleToDuty(angle);
    ledcWrite(_ledcChannel, duty);
    _angle = angle;
}

float SG90::getAngle(){
    return _angle;
}

bool SG90::stepDown(float stepDeg) {
    if (digitalRead(_switchPin) == LOW) {
        // Switch was pressed during the physical movement of the last step.
        // Back off slightly to relieve pressure on the servo and prevent buzzing.
        write(_angle + 5.0f);
        return true;
    }
    if (_angle <= 0.0f) {
        return true;
    }
    
    float next = _angle - stepDeg;
    if (next < 0.0f)
        next = 0.0f;
    write(next);
    
    // Check if the switch triggered instantly
    if (digitalRead(_switchPin) == LOW) {
        write(_angle + 5.0f);
        return true;
    }
    
    return (_angle <= 0.0f);
}

bool SG90::stepUp(float stepDeg, float targetAngle) {
    if (targetAngle < 0.0f)
        targetAngle = 0.0f;
    if (targetAngle > 180.0f)
        targetAngle = 180.0f;

    if (_angle >= targetAngle) {
        return true;
    }
    float next = _angle + stepDeg;
    if (next > targetAngle) {
        next = targetAngle;
    }
    write(next);
    return (_angle >= targetAngle);
}

bool SG90::down(int stepDelayMs) {
    while (digitalRead(_switchPin) != LOW) {
        if (_angle <= 0.0f)
            break;
        write(_angle - 1.0f);
        delay(stepDelayMs);
    }
    return true;
}

bool SG90::up(float degrees, int stepDelayMs) {
    float target = _angle + degrees;
    if (target < 0.0f)
        target = 0.0f;
    if (target > 180.0f)
        target = 180.0f;

    while (_angle < target) {
        write(_angle + 1.0f);
        delay(stepDelayMs);
    }
    return true;
}
