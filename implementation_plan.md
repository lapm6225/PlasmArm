# Implementation Plan for TOOL Command Fix and WebServer Cleanup

## Goal Description

The user reports that the `TOOL` command sent via WebSocket does not correctly actuate the tool (up/down). The current state machine treats tool actions as part of the `EXECUTING` state, using `TOOL_UP` and `TOOL_DOWN` commands that simply enqueue a point with a Z value and tool flag. There is also a `TOOL_ACTUATING` state that is currently unused. Additionally, the HTTP routes in `WebServer.cpp` are no longer needed and can be removed to simplify the code.

The plan will:
1. Refactor `SG90` methods (`down` and `up`) to return a `bool` indicating completion.
2. Introduce a new `TOOL_ACTUATING` state handling where the state machine waits for the servo actuation to finish before proceeding.
3. Update `Command::TOOL_UP` and `Command::TOOL_DOWN` handling to use the new SG90 interface and transition to `TOOL_ACTUATING`.
4. Remove unused HTTP route handling from `WebServer.cpp` (root, move, home, status) while preserving WebSocket functionality.
5. Ensure the Python test script `test_json_commands.py` works with the updated protocol.

## User Review Required

> [!IMPORTANT]
> The following decisions need user confirmation:
> - **Removal of HTTP routes**: Confirm that it is acceptable to delete the HTTP handlers (`handleRoot`, `handleMove`, `handleHome`, `handleStatus`). If any UI depends on them, they may need to be retained.
> - **Servo actuation return semantics**: The plan changes `SG90::down` and `SG90::up` to return `bool`. Confirm that this change aligns with any other code that may call these methods directly.
> - **State machine transition**: The new `TOOL_ACTUATING` state will replace the previous `EXECUTING` handling for tool commands. Confirm that this behavior (waiting for servo completion before next command) is desired.

## Proposed Changes

---
### Component: `hardware/SG90.h`
- Modify method signatures:
  - `bool down(int stepDelayMs = 20);`
  - `bool up(int degrees = 60, int stepDelayMs = 20);`
- Update documentation comments.

---
### Component: `hardware/SG90.cpp`
- Implement `down` and `up` to return `true` when the limit switch is triggered (down) or target angle reached (up).
- Ensure they still update `_angle` appropriately.

---
### Component: `ESP32/src/main.cpp` (State Machine)
- Add handling for `PlannerState::TOOL_ACTUATING`:
  - When a `TOOL_UP` or `TOOL_DOWN` command is received, invoke the corresponding `SG90` method (via a global `SG90` instance) and set state to `TOOL_ACTUATING`.
  - In the `TOOL_ACTUATING` block, poll the return value; when `true`, transition to `IDLE`.
- Remove the previous `TOOL_UP`/`TOOL_DOWN` handling that queued a point; instead, directly control the servo.
- Ensure `robotState.toolActive` and `robotState.toolZ` are updated after actuation completes.

---
### Component: `ESP32/src/web/WebServer.cpp`
- Delete the HTTP route handlers (`handleRoot`, `handleMove`, `handleHome`, `handleStatus`) and their registrations (lines 30‑41, 53‑71).
- Keep only the WebSocket setup and related functions.
- Adjust any references to removed functions.

---
### Component: `ESP32/src/web/WebServer.h`
- Remove declarations for the deleted HTTP handler functions.

---
### Component: `ESP32/src/core/Types.h` (if needed)
- Verify that `PlannerState` enum includes `TOOL_ACTUATING` (already present).

---
### Component: `Python/test_json_commands.py`
- No code changes required; the script will send `{"type":"TOOL","state":"UP"}` or `"DOWN"` which will be parsed as before.
- Ensure the WebSocket ACK flow remains unchanged.

## Open Questions

> [!WARNING]
> - Does any external UI (e.g., a web page served by the ESP32) rely on the HTTP endpoints that will be removed?
> - Are there any other parts of the codebase that directly call `SG90::down`/`up` expecting a `void` return?

## Verification Plan

### Automated Tests
- Build the firmware and run the existing unit tests (`RUN_UNIT_TESTS`). Ensure they still pass.
- Use the Python script `test_json_commands.py` to send `TOOL_UP` and `TOOL_DOWN` commands and verify that the servo actuation completes (mocked by checking log output for "TOOL actuation complete").

### Manual Verification
- Deploy the firmware to an ESP32 with the SG90 attached.
- Connect via WebSocket from the Python script and issue `TOOL` commands.
- Observe the servo moving up and down and the state machine logs indicating transition through `TOOL_ACTUATING`.
- Verify that the HTTP server no longer responds to the removed endpoints (e.g., `GET /` returns 404).

---
**End of Plan**
