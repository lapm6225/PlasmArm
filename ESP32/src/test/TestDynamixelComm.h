#ifndef TEST_DYNAMIXEL_COMM_H
#define TEST_DYNAMIXEL_COMM_H

#include "../hardware/DynamixelController.h"
#include <Arduino.h>

class TestDynamixelComm {
public:
    static void run(DynamixelController* dxlCtrl);
private:
    static void printHelp();
    static void processCommand(const String& cmd, DynamixelController* dxlCtrl);
};

#endif
