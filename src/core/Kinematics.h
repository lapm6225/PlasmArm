#ifndef KINEMATICS_H
#define KINEMATICS_H

#include "Types.h"
#include "../Config.h"

/**
 * @file Kinematics.h
 * @brief Forward and Inverse Kinematics for 2-DOF SCARA robot
 * 
 * Pure mathematical class with no hardware dependencies.
 * Uses law of cosines for inverse kinematics calculation.
 */

class Kinematics {
private:
    float L1;  // Length of first link
    float L2;  // Length of second link
    
public:
    /**
     * @brief Constructor
     * @param l1 Length of first link (base to elbow) in mm
     * @param l2 Length of second link (elbow to end effector) in mm
     */
    Kinematics(float l1 = ARM_LENGTH_1, float l2 = ARM_LENGTH_2);
    
    /**
     * @brief Set arm lengths
     * @param l1 Length of first link
     * @param l2 Length of second link
     */
    void setArmLengths(float l1, float l2);
    
    /**
     * @brief Calculate inverse kinematics
     * Converts Cartesian coordinates (x, y) to joint angles (theta1, theta2)
     * 
     * @param target Target position in Cartesian space (mm)
     * @param angles Output joint angles (degrees)
     * @return true if position is reachable, false otherwise
     */
    bool inverse(const Point2D& target, JointAngles& angles);
    
    /**
     * @brief Calculate forward kinematics
     * Converts joint angles (theta1, theta2) to Cartesian coordinates (x, y)
     * 
     * @param angles Input joint angles (degrees)
     * @param position Output Cartesian position (mm)
     */
    void forward(const JointAngles& angles, Point2D& position);
    
    /**
     * @brief Check if a point is within the workspace
     * @param point Point to check
     * @return true if reachable, false otherwise
     */
    bool isReachable(const Point2D& point);
    
    /**
     * @brief Get workspace radius (maximum reach)
     * @return Maximum reach distance in mm
     */
    float getMaxReach();
    
    /**
     * @brief Get minimum reach distance
     * @return Minimum reach distance in mm
     */
    float getMinReach();
};

#endif // KINEMATICS_H
