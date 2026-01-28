#ifndef PLANNER_H
#define PLANNER_H

#include "Types.h"
#include "../Config.h"
#include <queue>

/**
 * @file Planner.h
 * @brief Trajectory planning and interpolation
 * 
 * Handles look-ahead planning and generates intermediate points
 * for smooth motion. Implements linear interpolation with
 * configurable time intervals.
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
     * @brief Plan a path from start to end position
     * Generates intermediate points and pushes them to the motion queue
     * 
     * @param start Starting position
     * @param end Ending position
     * @param motionQueue Queue to push interpolated points to
     * @return Number of points generated
     */
    int planPath(const Point2D& start, const Point2D& end, 
                 std::queue<Point2D>& motionQueue);
    
    /**
     * @brief Calculate distance between two points
     * @param p1 First point
     * @param p2 Second point
     * @return Distance in mm
     */
    static float distance(const Point2D& p1, const Point2D& p2);
    
    /**
     * @brief Enable/disable S-curve acceleration profile
     * @param enable true to enable S-curve, false for linear
     */
    void setSCurve(bool enable);
};

#endif // PLANNER_H
