#include "TestTypes.h"

void TestTypes::runAllTests(TestRunner& runner) {
    runner.printHeader("TYPES");
    
    // Point2D tests
    runner.runTest("Point2D: Default constructor", testPoint2D_DefaultConstructor);
    runner.runTest("Point2D: Parameterized constructor", testPoint2D_ParameterizedConstructor);
    runner.runTest("Point2D: Equality operator", testPoint2D_Equality);
    
    // JointAngles tests
    runner.runTest("JointAngles: Default constructor", testJointAngles_DefaultConstructor);
    runner.runTest("JointAngles: Parameterized constructor", testJointAngles_ParameterizedConstructor);
    
    // RobotState tests
    runner.runTest("RobotState: Default constructor", testRobotState_DefaultConstructor);
    runner.runTest("RobotState: Initialization", testRobotState_Initialization);
    
    // Command tests
    runner.runTest("Command: Default constructor", testCommand_DefaultConstructor);
    runner.runTest("Command: Types", testCommand_Types);
}

bool TestTypes::testPoint2D_DefaultConstructor() {
    Point2D p;
    TestRunner runner(false);
    
    return runner.assertNear(0.0f, p.x, 0.01f) &&
           runner.assertNear(0.0f, p.y, 0.01f);
}

bool TestTypes::testPoint2D_ParameterizedConstructor() {
    Point2D p(123.45f, 67.89f);
    TestRunner runner(false);
    
    return runner.assertNear(123.45f, p.x, 0.01f) &&
           runner.assertNear(67.89f, p.y, 0.01f);
}

bool TestTypes::testPoint2D_Equality() {
    TestRunner runner(false);
    
    Point2D p1(100.0f, 200.0f);
    Point2D p2(100.0f, 200.0f);
    Point2D p3(100.0f, 201.0f);
    
    return runner.assertTrue(p1 == p2) &&
           runner.assertFalse(p1 == p3);
}

bool TestTypes::testJointAngles_DefaultConstructor() {
    JointAngles angles;
    TestRunner runner(false);
    
    return runner.assertNear(0.0f, angles.theta1, 0.01f) &&
           runner.assertNear(0.0f, angles.theta2, 0.01f);
}

bool TestTypes::testJointAngles_ParameterizedConstructor() {
    JointAngles angles(45.0f, 90.0f);
    TestRunner runner(false);
    
    return runner.assertNear(45.0f, angles.theta1, 0.01f) &&
           runner.assertNear(90.0f, angles.theta2, 0.01f);
}

bool TestTypes::testRobotState_DefaultConstructor() {
    RobotState state;
    TestRunner runner(false);
    
    return runner.assertNear(0.0f, state.currentPosition.x, 0.01f) &&
           runner.assertNear(0.0f, state.currentPosition.y, 0.01f) &&
           runner.assertNear(0.0f, state.currentAngles.theta1, 0.01f) &&
           runner.assertNear(0.0f, state.currentAngles.theta2, 0.01f) &&
           runner.assertFalse(state.isMoving) &&
           runner.assertFalse(state.isHomed);
}

bool TestTypes::testRobotState_Initialization() {
    RobotState state;
    state.currentPosition = Point2D(150.0f, 200.0f);
    state.currentAngles = JointAngles(45.0f, 30.0f);
    state.isMoving = true;
    state.isHomed = true;
    
    TestRunner runner(false);
    
    return runner.assertNear(150.0f, state.currentPosition.x, 0.01f) &&
           runner.assertNear(200.0f, state.currentPosition.y, 0.01f) &&
           runner.assertNear(45.0f, state.currentAngles.theta1, 0.01f) &&
           runner.assertNear(30.0f, state.currentAngles.theta2, 0.01f) &&
           runner.assertTrue(state.isMoving) &&
           runner.assertTrue(state.isHomed);
}

bool TestTypes::testCommand_DefaultConstructor() {
    Command cmd;
    TestRunner runner(false);
    
    return runner.assertEqual((int)Command::MOVE_TO, (int)cmd.type) &&
           runner.assertNear(0.0f, cmd.target.x, 0.01f) &&
           runner.assertNear(0.0f, cmd.target.y, 0.01f) &&
           runner.assertNear(0.0f, cmd.speed, 0.01f);
}

bool TestTypes::testCommand_Types() {
    TestRunner runner(false);
    
    Command cmd1(Command::MOVE_TO, Point2D(100.0f, 200.0f), 50.0f);
    Command cmd2(Command::HOME, Point2D(0.0f, 0.0f));
    Command cmd3(Command::STOP, Point2D(0.0f, 0.0f));
    
    return runner.assertEqual((int)Command::MOVE_TO, (int)cmd1.type) &&
           runner.assertNear(100.0f, cmd1.target.x, 0.01f) &&
           runner.assertNear(200.0f, cmd1.target.y, 0.01f) &&
           runner.assertNear(50.0f, cmd1.speed, 0.01f) &&
           runner.assertEqual((int)Command::HOME, (int)cmd2.type) &&
           runner.assertEqual((int)Command::STOP, (int)cmd3.type);
}
