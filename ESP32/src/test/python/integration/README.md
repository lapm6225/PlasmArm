# ESP32 SCARA Robot — Integration Tests

## Prerequisites

```bash
pip install websockets ezdxf
```

## Test Layers

Run them **in order** — each layer builds on the previous one.

### Layer 1: WebSocket Test (no DXF needed)

Tests basic connectivity, all command types, and flow control.

```bash
cd src/test/python/integration

# Use default IP (192.168.17.151)
python test_websocket.py

# Or specify IP
python test_websocket.py 192.168.1.50
```

**What to check on ESP32 Serial Monitor:**
- `WebSocket client #N connected`
- `Planner: Received command type X`  (for each command)
- `Planner: Generated N points`  (for MOVE_TO)
- `Planner: Tool ON/OFF, Z=X.XX` (for TOOL commands)
- `Planner: Homing sequence completed` (for HOME)
- `Planner: Emergency stop!` (for STOP)

### Layer 2: DXF Parser (offline, no ESP32 needed)

Test the DXF → command conversion without connecting to the robot.

```bash
# Preview what commands a DXF file generates
python dxf_parser.py ../dxf/drawing_simple.dxf

# With scale and offset
python dxf_parser.py ../dxf/drawing.dxf 1.0 50.0 50.0
```

### Layer 3: Full DXF → ESP32 Integration

Parses a DXF and streams it to the ESP32 with flow control.

```bash
# Direct mode (specify file)
python test_dxf_send.py ../dxf/drawing_simple.dxf 192.168.17.151

# Interactive mode (pick from file list)
python test_dxf_send.py --interactive
```

## Architecture

```
robot_client.py      Reusable async WebSocket client
                     Handles: connect, flow control (BUFFER/ACK), send, status
                     
dxf_parser.py        DXF → command list converter
                     Handles: LINE, POLYLINE, ARC, CIRCLE, SPLINE
                     Generates: MOVE_TO + TOOL commands with travel moves

test_websocket.py    Layer 1 test — WebSocket protocol validation
test_dxf_send.py     Layer 2 test — Full DXF streaming pipeline
```

## Flow Control Protocol

```
Python Host                      ESP32
    │                              │
    ├──[connect]──────────────────►│
    │◄─{"type":"BUFFER","cmdFree":30}──│  "I have 30 slots"
    │                              │
    ├──{"type":"MOVE_TO",...}──────►│
    │◄─{"type":"ACK","cmdFree":29}─│  "Got it, 29 left"
    │                              │
    ├──[send batch]───────────────►│
    │◄─{"type":"ACK","cmdFree":0}──│  "Full! Wait."
    │                              │
    │   ... python pauses ...      │  Robot is processing
    │                              │
    │◄─{"type":"BUFFER","cmdFree":5}──│  "Room again!"
    ├──[send 5 more]──────────────►│
```

## Debugging Tips

1. **ESP32 not reachable?**
   - Check WiFi connection in Serial Monitor
   - Verify IP address matches

2. **Commands sent but robot doesn't move?**
   - Check Serial Monitor for "IK failed" messages
   - Your DXF coordinates may be outside the arm's workspace
   - Use `dxf_parser.py` to check bounding box vs arm reach

3. **Flow control stuck?**
   - Enable `verbose=True` on `RobotClient` to see credit counts
   - Watch for `BUFFER` and `ACK` messages in the logs
   - If credits reach 0, the Planner task may be blocked

4. **Tool not activating?**
   - Check `TOOL_PIN` (GPIO 25) wiring
   - Look for "Planner: Tool ON/OFF" in Serial Monitor
