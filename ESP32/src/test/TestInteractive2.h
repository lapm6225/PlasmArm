#ifndef TEST_INTERACTIVE2_H
#define TEST_INTERACTIVE2_H

#include "../core/Kinematics.h"
#include "../core/Planner.h"
#include "../core/Types.h"
#include "../hardware/DynamixelController.h"
#include "../hardware/SG90.h"
#include <queue>
#include <ArduinoJson.h>

/**
 * @file TestInteractive2.h
 * @brief Interactive integration test with state machine (G-Code style commands)
 * 
 * Allows entering JSON commands via Serial console:
 * {"type":"MOVE_TO", "x":100, "y":200, "speed":50}
 * {"type":"TOOL", "state":"DOWN"}
 * {"type":"DELAY", "ms":250}
 * {"type":"CONFIG_CHANGE", "config":0} (0 = Left, 1 = Right)
 */

enum class CommandType {
    MOVE_TO,
    TOOL,
    DELAY,
    CONFIG_CHANGE
};

enum class ToolState {
    UP,
    DOWN
};

enum class ExecState {
    IDLE,
    MOVING,
    WAITING_TIME
};

struct RobotCommand {
    CommandType type;
    
    // Champs pour MOVE_TO
    Point2D target;
    float speed;
    
    // Champ pour TOOL
    ToolState toolState;
    
    // Champ pour DELAY
    uint32_t delayMs;
    
    // Champ pour CONFIG_CHANGE
    int newConfig; 
};

class TestInteractive2 {
public:
    static void run(DynamixelController* dxlCtrl);
    
private:
    static void printHelp();
    static void processCommand(const String& input, 
                              Kinematics& kin, 
                              Planner& planner,
                              DynamixelController* dxlCtrl,
                              Point2D& currentPos,
                              ArmConfig& currentConfig,
                              std::queue<RobotCommand>& executionQueue);
                              
    static void executeMove(const Point2D& start,
                           const Point2D& target,
                           Kinematics& kin,
                           Planner& planner,
                           DynamixelController* dxlCtrl,
                           bool showDetails,
                           ArmConfig& currentConfig);
                           
    static void printPoint(const Point2D& p, const char* label = "");
    static void printAngles(const JointAngles& angles, const char* label = "");
};

#endif // TEST_INTERACTIVE2_H
