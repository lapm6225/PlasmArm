#include "sg90.h"

// PWM configuration for hobby servos
static constexpr int SERVO_FREQUENCY_HZ = 50;
static constexpr int SERVO_RESOLUTION_BITS = 12; // 4096 steps
static constexpr int SERVO_PERIOD_US = 1000000 / SERVO_FREQUENCY_HZ; // 20ms

SG90::SG90(int servoPin, int switchPin, int ledcChannel)
    : _pin(servoPin),
      _switchPin(switchPin),
      _ledcChannel(ledcChannel),
      _angle(90) { // On remet à 90 par défaut pour éviter un mouvement brusque
    pinMode(_pin, OUTPUT);
    // Switch pin is INPUT_PULLUP to ensure HIGH when open and LOW when pressed
    pinMode(_switchPin, INPUT);
    ledcSetup(_ledcChannel, SERVO_FREQUENCY_HZ, SERVO_RESOLUTION_BITS);
    ledcAttachPin(_pin, _ledcChannel);
    // ATTENTION: On ne fait plus de write(_angle) ici au démarrage pour éviter de forcer
    // le servo dans la butée de la switch avant de savoir où on est !
}

uint32_t SG90::angleToDuty(int angle) const {
    const uint32_t minPulseUs = 500;
    const uint32_t maxPulseUs = 2500;
    
    // Clamp angle between 0 and 180
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    uint32_t pulseUs = map(angle, 0, 180, minPulseUs, maxPulseUs);

    uint32_t maxDuty = (1u << SERVO_RESOLUTION_BITS) - 1;
    return (pulseUs * maxDuty) / SERVO_PERIOD_US;
}

void SG90::write(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    uint32_t duty = angleToDuty(angle);
    ledcWrite(_ledcChannel, duty);
    _angle = angle;
}

void SG90::down(int stepDelayMs) {
    // Moves the servo increasing the angle until switch reads LOW (pressed)
    while (digitalRead(_switchPin) != LOW) {
        if (_angle <= 0) break; // Physical limit protection
        write(_angle - 1); // +1 pour descendre
        delay(stepDelayMs);
    }
}

void SG90::up(int degrees, int stepDelayMs) {
    // Moves the servo decreasing the angle by 'degrees'
    int target = _angle + degrees;
    if (target < 0) target = 0;
    if (target > 180) target = 180;
    
    while (_angle < target) {
        write(_angle + 1);
        delay(stepDelayMs);
    }
}
