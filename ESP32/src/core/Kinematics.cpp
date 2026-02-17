#include "Kinematics.h"
#include <Arduino.h>
#include <math.h>


Kinematics::Kinematics(float l1, float l2) : L1(l1), L2(l2) {}

void Kinematics::setArmLengths(float l1, float l2) {
  L1 = l1;
  L2 = l2;
}

bool Kinematics::inverse(const Point2D &target, JointAngles &angles) {
  float x = target.x;
  float y = target.y;

  // ---- Step 1: Workspace boundary check ----
  float r = sqrt(x * x + y * y);

  if (r > (L1 + L2)) {
    Serial.printf("IK REJECT: r=%.1f > max reach %.1f\n", r, L1 + L2);
    return false;
  }
  if (r < WORKSPACE_R_MIN) {
    Serial.printf("IK REJECT: r=%.1f < min reach %.1f (singularity zone)\n", r,
                  WORKSPACE_R_MIN);
    return false;
  }

  // ---- Step 2: Calculate theta2 (elbow angle) ----
  // Law of cosines: r² = L1² + L2² + 2·L1·L2·cos(θ2)
  float cosTheta2 = (r * r - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);

  // Clamp to [-1, 1] for numerical safety
  cosTheta2 = constrain(cosTheta2, -1.0f, 1.0f);

  // Elbow-up configuration (positive theta2)
  float theta2Rad = acos(cosTheta2);
  angles.theta2 = theta2Rad * 180.0f / M_PI; // Always 0° to ~161°

  // ---- Step 3: Calculate theta1 (base/shoulder angle) ----
  // alpha = angle from +X axis to the target point
  float alpha = atan2(y, x) * 180.0f / M_PI; // -180° to +180°

  // beta = angle offset due to elbow bend
  float sinBeta = (L2 * sin(theta2Rad)) / r;
  sinBeta = constrain(sinBeta, -1.0f, 1.0f);
  float beta = asin(sinBeta) * 180.0f / M_PI;

  // theta1 = alpha - beta (elbow-up solution)
  angles.theta1 = alpha - beta;

  // ---- Step 4: Validate angle limits ----
  if (angles.theta1 < THETA1_MIN || angles.theta1 > THETA1_MAX) {
    Serial.printf("IK REJECT: theta1=%.1f° outside limits [%.0f°, %.0f°] "
                  "for target (%.1f, %.1f)\n",
                  angles.theta1, THETA1_MIN, THETA1_MAX, x, y);
    return false;
  }
  if (angles.theta2 < THETA2_MIN || angles.theta2 > THETA2_MAX) {
    Serial.printf("IK REJECT: theta2=%.1f° outside limits [%.0f°, %.0f°] "
                  "for target (%.1f, %.1f)\n",
                  angles.theta2, THETA2_MIN, THETA2_MAX, x, y);
    return false;
  }

#if DEBUG_KINEMATICS
  Serial.printf("IK: (%.2f, %.2f) r=%.1f -> theta1=%.2f° theta2=%.2f°\n", x, y,
                r, angles.theta1, angles.theta2);
#endif

  return true;
}

void Kinematics::forward(const JointAngles &angles, Point2D &position) {
  float theta1Rad = angles.theta1 * M_PI / 180.0f;
  float theta2Rad = angles.theta2 * M_PI / 180.0f;

  // End effector position:
  // x = L1·cos(θ1) + L2·cos(θ1 + θ2)
  // y = L1·sin(θ1) + L2·sin(θ1 + θ2)
  position.x = L1 * cos(theta1Rad) + L2 * cos(theta1Rad + theta2Rad);
  position.y = L1 * sin(theta1Rad) + L2 * sin(theta1Rad + theta2Rad);

#if DEBUG_KINEMATICS
  Serial.printf("FK: theta1=%.2f° theta2=%.2f° -> (%.2f, %.2f)\n",
                angles.theta1, angles.theta2, position.x, position.y);
#endif
}

bool Kinematics::isReachable(const Point2D &point) {
  float r = sqrt(point.x * point.x + point.y * point.y);

  // Check circular boundaries
  if (r > (L1 + L2) || r < WORKSPACE_R_MIN) {
    return false;
  }

  // Check half-plane constraint (y > 0, with small tolerance for boundary)
  // Allow y slightly below 0 to handle points at (300, 0) and (-300, 0)
  if (point.y < -1.0f) {
    return false;
  }

  // Full validation: try computing IK and check angle limits
  JointAngles testAngles;
  // Use a temporary without printing errors
  float x = point.x;
  float y = point.y;
  float cosTheta2 = (r * r - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
  cosTheta2 = constrain(cosTheta2, -1.0f, 1.0f);
  float theta2Rad = acos(cosTheta2);
  float theta2 = theta2Rad * 180.0f / M_PI;

  float alpha = atan2(y, x) * 180.0f / M_PI;
  float sinBeta = (L2 * sin(theta2Rad)) / r;
  sinBeta = constrain(sinBeta, -1.0f, 1.0f);
  float beta = asin(sinBeta) * 180.0f / M_PI;
  float theta1 = alpha - beta;

  // Check angle limits
  if (theta1 < THETA1_MIN || theta1 > THETA1_MAX)
    return false;
  if (theta2 < THETA2_MIN || theta2 > THETA2_MAX)
    return false;

  return true;
}

float Kinematics::getMaxReach() { return L1 + L2; }

float Kinematics::getMinReach() { return WORKSPACE_R_MIN; }
