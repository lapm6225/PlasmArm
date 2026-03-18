#include "sg90.h"

SG90::SG90(int servoPin, int switchPin, int minAngle, int maxAngle)
    : _pin(servoPin),
      _switchPin(switchPin),
      _angle(minAngle),
      _minAngle(minAngle),
      _maxAngle(maxAngle) {
    pinMode(_pin, OUTPUT);
    _servo.attach(_pin);
    write(_angle);
}

int SG90::clampAngle(int angle) const {
    if (angle < _minAngle) return _minAngle;
    if (angle > _maxAngle) return _maxAngle;
    return angle;
}

void SG90::write(int angle) {
    int clamped = clampAngle(angle);
    _servo.write(clamped);
    _angle = clamped;
}

void SG90::sg_down(int activeState, int stepDelayMs) {
    pinMode(_switchPin, INPUT);

    while (digitalRead(_switchPin) != activeState && _angle < _maxAngle) {
        write(_angle + 1);
        delay(stepDelayMs);
    }
}

void SG90::sg_up(int degrees, int stepDelayMs) {
    int target = clampAngle(_angle - degrees);
    while (_angle > target) {
        write(_angle - 1);
        delay(stepDelayMs);
    }
}
