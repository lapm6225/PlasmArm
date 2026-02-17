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

int Planner::planPath(const TargetState& start, const TargetState& end, 
                      std::queue<TargetState>& motionQueue) {
    // Calculate total 3D distance (XYZ)
    float dist = distance3D(start, end);
    
    if (dist < MIN_SEGMENT_LENGTH) {
        // Too short to interpolate, just push the end state as-is
        motionQueue.push(end);
        return 1;
    }
    
    // Calculate total time needed based on XYZ distance
    float totalTime = dist / speed;
    
    // Calculate number of interpolation points
    int numPoints = (int)(totalTime / interpolationInterval) + 1;
    
    // Ensure at least 2 points (start vicinity and end)
    if (numPoints < 2) {
        numPoints = 2;
    }
    
    #if DEBUG_PLANNER
    Serial.printf("Planner: Path (%.2f,%.2f,%.2f,T=%d) -> (%.2f,%.2f,%.2f,T=%d)\n",
                  start.x, start.y, start.z, start.toolActive,
                  end.x, end.y, end.z, end.toolActive);
    Serial.printf("Planner: Dist=%.2fmm, Time=%.2fs, Points=%d\n",
                  dist, totalTime, numPoints);
    #endif
    
    // Generate interpolated points
    for (int i = 0; i <= numPoints; i++) {
        float t = (float)i / (float)numPoints;  // Parameter from 0 to 1
        
        TargetState point;
        // Linear interpolation of X, Y, Z
        point.x = start.x + t * (end.x - start.x);
        point.y = start.y + t * (end.y - start.y);
        point.z = start.z + t * (end.z - start.z);
        // Tool state: use the END state for the entire segment
        // This means "move to destination with tool in this state"
        point.toolActive = end.toolActive;
        
        motionQueue.push(point);
        
        #if DEBUG_PLANNER
        if (i % 10 == 0 || i == numPoints) {
            Serial.printf("Planner: Pt %d: (%.2f, %.2f, %.2f) T=%d\n", 
                          i, point.x, point.y, point.z, point.toolActive);
        }
        #endif
    }
    
    return numPoints + 1;
}

float Planner::distance3D(const TargetState& a, const TargetState& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

float Planner::distance(const Point2D& p1, const Point2D& p2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return sqrt(dx * dx + dy * dy);
}

void Planner::setSCurve(bool enable) {
    useSCurve = enable;
    // TODO: Implement S-curve acceleration profile
}
