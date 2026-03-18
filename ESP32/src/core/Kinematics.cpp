#include "Kinematics.h"
#include <Arduino.h>
#include <math.h>


Kinematics::Kinematics(float l1, float l2) : L1(l1), L2(l2) {}

void Kinematics::setArmLengths(float l1, float l2) {
  L1 = l1;
  L2 = l2;
}

/**
 * @brief Solve IK for a specific theta2 sign
 * 
 * Helper that computes (theta1, theta2) for a given elbow direction.
 * Returns false if joint limits are violated.
 */
static bool solveForConfig(float x, float y, float r, float L1, float L2,
                           float cosTheta2, bool negativeTheta2,
                           JointAngles& angles) {
  float theta2Rad;
  if (negativeTheta2) {
    theta2Rad = -acos(cosTheta2);  // RIGHT_ELBOW: theta2 ≤ 0
  } else {
    theta2Rad = acos(cosTheta2);   // LEFT_ELBOW:  theta2 ≥ 0
  }
  angles.theta2 = theta2Rad * 180.0f / M_PI;

  // Calculate theta1: alpha is the angle to the target, beta is the elbow offset
  float alpha = atan2(y, x) * 180.0f / M_PI;

  float sinBeta = (L2 * sin(theta2Rad)) / r;
  sinBeta = constrain(sinBeta, -1.0f, 1.0f);
  float beta = asin(sinBeta) * 180.0f / M_PI;

  angles.theta1 = alpha - beta;

  // Validate angle limits
  if (angles.theta1 < THETA1_MIN || angles.theta1 > THETA1_MAX)
    return false;
  if (angles.theta2 < THETA2_MIN || angles.theta2 > THETA2_MAX)
    return false;

  return true;
}

bool Kinematics::inverse(const Point2D &target, JointAngles &angles,
                         ArmConfig config, ArmConfig &usedConfig) {
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

  // ---- Step 2: Calculate cosTheta2 (common to both solutions) ----
  float cosTheta2 = (r * r - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
  cosTheta2 = constrain(cosTheta2, -1.0f, 1.0f);

  // ---- Step 3: Resolve AUTO to a concrete config ----
  ArmConfig preferred = config;
  if (preferred == ArmConfig::AUTO) {
    preferred = (x >= 0.0f) ? ArmConfig::RIGHT_ELBOW : ArmConfig::LEFT_ELBOW;
  }

  // ---- Step 4: Try the preferred configuration ----
  bool preferNegative = (preferred == ArmConfig::RIGHT_ELBOW);

  if (solveForConfig(x, y, r, L1, L2, cosTheta2, preferNegative, angles)) {
    usedConfig = preferred;
#if DEBUG_KINEMATICS
    Serial.printf("IK: (%.2f, %.2f) -> theta1=%.2f° theta2=%.2f° [%s]\n",
                  x, y, angles.theta1, angles.theta2,
                  preferNegative ? "RIGHT_ELBOW" : "LEFT_ELBOW");
#endif
    return true;
  }

  // ---- Step 5: Fallback — try the other configuration ----
  ArmConfig fallback = (preferred == ArmConfig::RIGHT_ELBOW)
                           ? ArmConfig::LEFT_ELBOW
                           : ArmConfig::RIGHT_ELBOW;
  bool fallbackNegative = !preferNegative;

#if DEBUG_KINEMATICS
  Serial.printf("IK: preferred config failed for (%.2f, %.2f), trying fallback\n", x, y);
#endif

  if (solveForConfig(x, y, r, L1, L2, cosTheta2, fallbackNegative, angles)) {
    usedConfig = fallback;
#if DEBUG_KINEMATICS
    Serial.printf("IK: (%.2f, %.2f) -> theta1=%.2f° theta2=%.2f° [FALLBACK %s]\n",
                  x, y, angles.theta1, angles.theta2,
                  fallbackNegative ? "RIGHT_ELBOW" : "LEFT_ELBOW");
#endif
    return true;
  }

  // Both configurations failed
  Serial.printf("IK REJECT: no valid config for (%.1f, %.1f)\n", x, y);
  return false;
}

// Convenience overload: discards usedConfig (for callers that don't track it)
bool Kinematics::inverse(const Point2D &target, JointAngles &angles,
                         ArmConfig config) {
  ArmConfig usedConfig;
  return inverse(target, angles, config, usedConfig);
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

bool Kinematics::isReachable(const Point2D &point, ArmConfig config) {
  float r = sqrt(point.x * point.x + point.y * point.y);

  // Quick boundary check
  if (r > (L1 + L2) || r < WORKSPACE_R_MIN) {
    return false;
  }

  // Full validation: try computing IK with the specified config
  float cosTheta2 = (r * r - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
  cosTheta2 = constrain(cosTheta2, -1.0f, 1.0f);

  // Resolve AUTO
  ArmConfig resolved = config;
  if (resolved == ArmConfig::AUTO) {
    resolved = (point.x >= 0.0f) ? ArmConfig::RIGHT_ELBOW : ArmConfig::LEFT_ELBOW;
  }

  bool preferNegative = (resolved == ArmConfig::RIGHT_ELBOW);

  JointAngles testAngles;
  // Try preferred config
  if (solveForConfig(point.x, point.y, r, L1, L2, cosTheta2, preferNegative, testAngles)) {
    return true;
  }
  // Try fallback
  if (solveForConfig(point.x, point.y, r, L1, L2, cosTheta2, !preferNegative, testAngles)) {
    return true;
  }

  return false;
}

float Kinematics::getMaxReach() { return L1 + L2; }

float Kinematics::getMinReach() { return WORKSPACE_R_MIN; }
