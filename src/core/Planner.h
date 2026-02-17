#ifndef PLANNER_H
#define PLANNER_H

#include "Types.h"
#include "../Config.h"
#include <queue>

/**
 * @file Planner.h
 * @brief Trajectory planning and interpolation (4D: X, Y, Z, Tool)
 * 
 * Generates intermediate TargetState points for smooth motion.
 * Each interpolated point carries the full state: position (X,Y),
 * tool height (Z), and tool activation state.
 * 
 * The planner linearly interpolates X, Y, and Z between the start
 * and end states. The toolActive flag is copied from the end state
 * to every interpolated point (tool state is set at segment start).
 */

class Planner {
private:
    float speed;              // Current speed in mm/s
    float acceleration;       // Acceleration in mm/s²
    float interpolationInterval;  // Time between points in seconds
    
    // S-curve parameters (for future enhancement)
    bool useSCurve;
    float jerkLimit;
    
public:
    /**
     * @brief Constructor
     * @param speed Default speed in mm/s
     * @param acceleration Acceleration in mm/s²
     */
    Planner(float speed = DEFAULT_SPEED, 
            float acceleration = ACCELERATION);
    
    /**
     * @brief Set movement speed
     * @param speed Speed in mm/s
     */
    void setSpeed(float speed);
    
    /**
     * @brief Set acceleration
     * @param acceleration Acceleration in mm/s²
     */
    void setAcceleration(float acceleration);
    
    /**
     * @brief Plan a 4D path from start to end state.
     * 
     * Generates intermediate TargetState points with:
     *   - Linear interpolation of X, Y, Z
     *   - toolActive copied from 'end' to every point
     * 
     * @param start Starting state (position + tool)
     * @param end   Ending state (position + tool)
     * @param motionQueue Queue to push interpolated TargetState points to
     * @return Number of points generated
     */
    int planPath(const TargetState& start, const TargetState& end, 
                 std::queue<TargetState>& motionQueue);

    /**
     * @brief Calculate 3D distance between two TargetStates (XYZ)
     */
    static float distance3D(const TargetState& a, const TargetState& b);
    
    /**
     * @brief Calculate 2D distance between two Point2D
     */
    static float distance(const Point2D& p1, const Point2D& p2);
    
    /**
     * @brief Enable/disable S-curve acceleration profile
     * @param enable true to enable S-curve, false for linear
     */
    void setSCurve(bool enable);
};

#endif // PLANNER_H
