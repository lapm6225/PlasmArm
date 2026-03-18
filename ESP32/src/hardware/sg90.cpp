#include "sg90.h"

// PWM configuration for hobby servos
static constexpr int SERVO_FREQUENCY_HZ = 50;
static constexpr int SERVO_RESOLUTION_BITS = 12; // 4096 steps
static constexpr int SERVO_PERIOD_US = 1000000 / SERVO_FREQUENCY_HZ; // 20ms

SG90::SG90(int servoPin, int switchPin, int ledcChannel)
    : _pin(servoPin),
      _switchPin(switchPin),
      _ledcChannel(ledcChannel),
      _angle(0) {
    pinMode(_pin, OUTPUT);
    pinMode(_switchPin, INPUT);
    ledcSetup(_ledcChannel, SERVO_FREQUENCY_HZ, SERVO_RESOLUTION_BITS);
    ledcAttachPin(_pin, _ledcChannel);
}

uint32_t SG90::angleToDuty(int angle) const {
    // Map angle (0-180) to pulse width (500-2500 us) similarly to most hobby servos.
    const uint32_t minPulseUs = 500;
    const uint32_t maxPulseUs = 2500;
    uint32_t pulseUs = map(angle, 0, 180, minPulseUs, maxPulseUs);

    // Convert to LEDC duty value using the PWM period.
    uint32_t maxDuty = (1u << SERVO_RESOLUTION_BITS) - 1;
    return (pulseUs * maxDuty) / SERVO_PERIOD_US;
}

void SG90::write(int angle) {
    uint32_t duty = angleToDuty(angle);
    ledcWrite(_ledcChannel, duty);
    _angle = angle;
}

void SG90::sg_down(int activeState, int stepDelayMs) {

    while (digitalRead(_switchPin) != activeState) {
        write(_angle + 1);
        delay(stepDelayMs);
    }
}

void SG90::sg_up(int degrees, int stepDelayMs) {
    int target = _angle - degrees;
    while (_angle > target) {
        write(_angle - 1);
        delay(stepDelayMs);
    }
}
