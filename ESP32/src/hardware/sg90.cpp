#include "sg90.h"

static constexpr int SERVO_FREQUENCY_HZ = 50;
static constexpr int SERVO_RESOLUTION_BITS = 12;
static constexpr int SERVO_PERIOD_US = 1000000 / SERVO_FREQUENCY_HZ;

SG90::SG90(int servoPin, int switchPin, int ledcChannel)
    : _pin(servoPin), _switchPin(switchPin), _ledcChannel(ledcChannel), _angle(90) {
    pinMode(_pin, OUTPUT);
    pinMode(_switchPin, INPUT_PULLUP);
    ledcSetup(_ledcChannel, SERVO_FREQUENCY_HZ, SERVO_RESOLUTION_BITS);
    ledcAttachPin(_pin, _ledcChannel);
}

uint32_t SG90::angleToDuty(int angle) const {
    const uint32_t minPulseUs = 500;
    const uint32_t maxPulseUs = 2500;

    if (angle < 0)
        angle = 0;
    if (angle > 180)
        angle = 180;

    uint32_t pulseUs = map(angle, 0, 180, minPulseUs, maxPulseUs);
    uint32_t maxDuty = (1u << SERVO_RESOLUTION_BITS) - 1;
    return (pulseUs * maxDuty) / SERVO_PERIOD_US;
}

void SG90::write(int angle) {
    if (angle < 0)
        angle = 0;
    if (angle > 180)
        angle = 180;

    uint32_t duty = angleToDuty(angle);
    ledcWrite(_ledcChannel, duty);
    _angle = angle;
}

int SG90::getAngle(){
    return _angle;
}

bool SG90::stepDown(float stepDeg) {
    if (digitalRead(_switchPin) == LOW) {
        // Switch was pressed during the physical movement of the last step.
        // Back off slightly to relieve pressure on the servo and prevent buzzing.
        write(_angle + 2);
        return true;
    }
    if (_angle <= 0) {
        return true;
    }
    
    int stepInt = static_cast<int>(round(stepDeg));
    int next = _angle - stepInt;
    if (next < 0)
        next = 0;
    write(next);
    
    // Check if the switch triggered instantly
    if (digitalRead(_switchPin) == LOW) {
        write(_angle + 2);
        return true;
    }
    
    return (_angle == 0);
}

bool SG90::stepUp(float stepDeg, float targetAngle) {
    if (targetAngle < 0)
        targetAngle = 0;
    if (targetAngle > 180)
        targetAngle = 180;

    if (_angle >= static_cast<int>(round(targetAngle))) {
        return true;
    }
    int stepInt = static_cast<int>(round(stepDeg));
    int next = _angle + stepInt;
    if (next > static_cast<int>(round(targetAngle))) {
        next = static_cast<int>(round(targetAngle));
    }
    write(next);
    return (_angle >= static_cast<int>(round(targetAngle)));
}

bool SG90::down(int stepDelayMs) {
    while (digitalRead(_switchPin) != LOW) {
        if (_angle <= 0)
            break;
        write(_angle - 1);
        delay(stepDelayMs);
    }
    return true;
}

bool SG90::up(int degrees, int stepDelayMs) {
    int target = _angle + degrees;
    if (target < 0)
        target = 0;
    if (target > 180)
        target = 180;

    while (_angle < target) {
        write(_angle + 1);
        delay(stepDelayMs);
    }
    return true;
}
