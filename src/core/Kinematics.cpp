#include "Kinematics.h"
#include <math.h>
#include <Arduino.h>

Kinematics::Kinematics(float l1, float l2) : L1(l1), L2(l2) {
}

void Kinematics::setArmLengths(float l1, float l2) {
    L1 = l1;
    L2 = l2;
}

bool Kinematics::inverse(const Point2D& target, JointAngles& angles) {
    float x = target.x;
    float y = target.y;
    
    // Calculate distance from base to target
    float r = sqrt(x * x + y * y);
    
    // Check if point is reachable
    if (r > (L1 + L2) || r < abs(L1 - L2)) {
        #if DEBUG_KINEMATICS
        Serial.printf("Kinematics: Point (%.2f, %.2f) out of reach (r=%.2f)\n", 
                      x, y, r);
        #endif
        return false;
    }
    
    // Calculate theta2 using law of cosines
    // r² = L1² + L2² - 2*L1*L2*cos(180° - theta2)
    // cos(180° - theta2) = -cos(theta2)
    // r² = L1² + L2² + 2*L1*L2*cos(theta2)
    // cos(theta2) = (r² - L1² - L2²) / (2*L1*L2)
    
    float cosTheta2 = (r * r - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
    
    // Clamp to valid range [-1, 1] to avoid numerical errors
    if (cosTheta2 > 1.0f) cosTheta2 = 1.0f;
    if (cosTheta2 < -1.0f) cosTheta2 = -1.0f;
    
    // Calculate theta2 (elbow angle)
    // We use the "elbow up" configuration (positive theta2)
    // For "elbow down", use negative theta2
    float theta2Rad = acos(cosTheta2);
    angles.theta2 = theta2Rad * 180.0f / M_PI;
    
    // Calculate theta1 (base angle)
    // First, calculate angle from base to target
    float alpha = atan2(y, x) * 180.0f / M_PI;
    
    // Then calculate angle from base to elbow
    // Using law of sines: sin(beta) / L2 = sin(theta2) / r
    float sinBeta = (L2 * sin(theta2Rad)) / r;
    if (sinBeta > 1.0f) sinBeta = 1.0f;
    if (sinBeta < -1.0f) sinBeta = -1.0f;
    
    float betaRad = asin(sinBeta);
    float beta = betaRad * 180.0f / M_PI;
    
    // Theta1 is alpha minus beta
    angles.theta1 = alpha - beta;
    
    // Normalize angles to 0-360 range
    while (angles.theta1 < 0) angles.theta1 += 360.0f;
    while (angles.theta1 >= 360.0f) angles.theta1 -= 360.0f;
    while (angles.theta2 < 0) angles.theta2 += 360.0f;
    while (angles.theta2 >= 360.0f) angles.theta2 -= 360.0f;
    
    #if DEBUG_KINEMATICS
    Serial.printf("Kinematics: (%.2f, %.2f) -> theta1=%.2f°, theta2=%.2f°\n",
                  x, y, angles.theta1, angles.theta2);
    #endif
    
    return true;
}

void Kinematics::forward(const JointAngles& angles, Point2D& position) {
    // Convert angles to radians
    float theta1Rad = angles.theta1 * M_PI / 180.0f;
    float theta2Rad = angles.theta2 * M_PI / 180.0f;
    
    // Calculate end effector position
    // x = L1*cos(theta1) + L2*cos(theta1 + theta2)
    // y = L1*sin(theta1) + L2*sin(theta1 + theta2)
    
    position.x = L1 * cos(theta1Rad) + L2 * cos(theta1Rad + theta2Rad);
    position.y = L1 * sin(theta1Rad) + L2 * sin(theta1Rad + theta2Rad);
    
    #if DEBUG_KINEMATICS
    Serial.printf("Kinematics: theta1=%.2f°, theta2=%.2f° -> (%.2f, %.2f)\n",
                  angles.theta1, angles.theta2, position.x, position.y);
    #endif
}

bool Kinematics::isReachable(const Point2D& point) {
    float r = sqrt(point.x * point.x + point.y * point.y);
    return (r <= (L1 + L2) && r >= abs(L1 - L2));
}

float Kinematics::getMaxReach() {
    return L1 + L2;
}

float Kinematics::getMinReach() {
    return abs(L1 - L2);
}
