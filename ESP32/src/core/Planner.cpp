#include "Planner.h"
#include <math.h>
#include <Arduino.h>

Planner::Planner(float speed, float acceleration)
    : speed(speed), acceleration(acceleration),
      interpolationInterval(INTERPOLATION_INTERVAL_MS / 1000.0f),
      useSCurve(false), jerkLimit(JERK_LIMIT) {
}

void Planner::setSpeed(float speed) {
    this->speed = speed;
    if (this->speed > MAX_SPEED) {
        this->speed = MAX_SPEED;
    }
    if (this->speed < 0) {
        this->speed = 0;
    }
}

void Planner::setAcceleration(float acceleration) {
    this->acceleration = acceleration;
}

int Planner::planPath(const Point2D& start, const Point2D& end, 
                      std::queue<Point2D>& motionQueue) {
    // Calculate total distance
    float dist = distance(start, end);
    
    if (dist < MIN_SEGMENT_LENGTH) {
        // Too short, just add the end point
        motionQueue.push(end);
        return 1;
    }
    
    // ================================================================
    // Trapezoidal velocity profile
    // ================================================================
    //
    //  Velocity
    //    ^
    //    |     ____________
    //    |    /            \        <- cruise at V_max
    //    |   /              \
    //    |  /                \
    //    | /                  \
    //    +----------------------> Time
    //     t_accel  t_cruise  t_decel
    //
    // If the segment is too short to reach full speed, we get a
    // triangular profile (accel directly into decel, no cruise).
    // ================================================================
    
    float vMax = speed;          // mm/s
    float accel = acceleration;  // mm/s²
    
    // Distance needed to accelerate from 0 to vMax
    // d_accel = v² / (2a)
    float dAccel = (vMax * vMax) / (2.0f * accel);
    
    float tAccel, tCruise, tDecel, totalTime;
    float vPeak;  // Actual peak velocity reached
    
    if (2.0f * dAccel >= dist) {
        // --- Triangular profile: can't reach full speed ---
        // Peak velocity: v_peak = sqrt(a * d)
        vPeak = sqrt(accel * dist);
        tAccel = vPeak / accel;
        tDecel = tAccel;
        tCruise = 0.0f;
        totalTime = tAccel + tDecel;
        dAccel = dist / 2.0f;  // Symmetric: half accel, half decel
    } else {
        // --- Trapezoidal profile: full accel → cruise → decel ---
        vPeak = vMax;
        tAccel = vMax / accel;
        tDecel = tAccel;  // Symmetric deceleration
        float dCruise = dist - 2.0f * dAccel;
        tCruise = dCruise / vMax;
        totalTime = tAccel + tCruise + tDecel;
    }
    
    // Calculate number of interpolation points based on total time
    int numPoints = (int)(totalTime / interpolationInterval) + 1;
    if (numPoints < 2) {
        numPoints = 2;
    }
    
    #if DEBUG_PLANNER
    Serial.printf("Planner: path (%.2f,%.2f)->(%.2f,%.2f)\n",
                  start.x, start.y, end.x, end.y);
    Serial.printf("Planner: dist=%.2fmm vPeak=%.1fmm/s accel=%.0fmm/s²\n",
                  dist, vPeak, accel);
    Serial.printf("Planner: tAccel=%.3fs tCruise=%.3fs tDecel=%.3fs total=%.3fs pts=%d\n",
                  tAccel, tCruise, tDecel, totalTime, numPoints);
    #endif
    
    // Generate interpolated points using the trapezoidal profile
    for (int i = 0; i <= numPoints; i++) {
        float t = (float)i / (float)numPoints * totalTime;  // Actual time in seconds
        
        // Calculate distance traveled at time t using the velocity profile
        float s;  // Distance along path at time t
        
        if (t <= tAccel) {
            // Phase 1: Acceleration — s = ½ · a · t²
            s = 0.5f * accel * t * t;
        } else if (t <= tAccel + tCruise) {
            // Phase 2: Cruise — s = d_accel + v_peak · (t - t_accel)
            float dt = t - tAccel;
            s = dAccel + vPeak * dt;
        } else {
            // Phase 3: Deceleration — s = d_total - ½ · a · t_remaining²
            float tRemaining = totalTime - t;
            if (tRemaining < 0.0f) tRemaining = 0.0f;
            s = dist - 0.5f * accel * tRemaining * tRemaining;
        }
        
        // Clamp to [0, dist] for numerical safety
        if (s < 0.0f) s = 0.0f;
        if (s > dist) s = dist;
        
        // Map distance to position along the line
        float ratio = s / dist;
        Point2D point;
        point.x = start.x + ratio * (end.x - start.x);
        point.y = start.y + ratio * (end.y - start.y);
        
        motionQueue.push(point);
        
        #if DEBUG_PLANNER
        if (i % 10 == 0 || i == numPoints) {
            Serial.printf("Planner: [%d] t=%.3fs s=%.2fmm (%.1f%%)\n",
                          i, t, s, ratio * 100.0f);
        }
        #endif
    }
    
    return numPoints + 1;
}

float Planner::distance(const Point2D& p1, const Point2D& p2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return sqrt(dx * dx + dy * dy);
}

void Planner::setSCurve(bool enable) {
    useSCurve = enable;
    // TODO: Implement S-curve acceleration profile
    // This would involve calculating velocity profiles with jerk limits
}
