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
    
    // Calculate total time needed
    float totalTime = dist / speed;
    
    // Calculate number of interpolation points
    int numPoints = (int)(totalTime / interpolationInterval) + 1;
    
    // Ensure at least 2 points (start and end)
    if (numPoints < 2) {
        numPoints = 2;
    }
    
    #if DEBUG_PLANNER
    Serial.printf("Planner: Planning path from (%.2f, %.2f) to (%.2f, %.2f)\n",
                  start.x, start.y, end.x, end.y);
    Serial.printf("Planner: Distance=%.2fmm, Time=%.2fs, Points=%d\n",
                  dist, totalTime, numPoints);
    #endif
    
    // Generate interpolated points
    for (int i = 0; i <= numPoints; i++) {
        float t = (float)i / (float)numPoints;  // Parameter from 0 to 1
        
        // Linear interpolation
        Point2D point;
        point.x = start.x + t * (end.x - start.x);
        point.y = start.y + t * (end.y - start.y);
        
        motionQueue.push(point);
        
        #if DEBUG_PLANNER
        if (i % 10 == 0 || i == numPoints) {
            Serial.printf("Planner: Point %d: (%.2f, %.2f)\n", i, point.x, point.y);
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
