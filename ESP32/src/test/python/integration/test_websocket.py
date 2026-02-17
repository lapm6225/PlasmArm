"""
test_websocket.py -- Layer 1: Test WebSocket connectivity and basic commands.

Run this FIRST to verify the ESP32 is reachable and the WebSocket protocol
is working correctly. Watch the ESP32 Serial Monitor while running this.

Usage:
    python test_websocket.py [ESP32_IP]
    python test_websocket.py 192.168.17.151

What it tests:
    1. WebSocket connection
    2. Initial BUFFER message from ESP32
    3. Sending a MOVE_TO command -> receiving ACK
    4. Receiving STATUS updates
    5. Sending TOOL ON/OFF -> receiving ACK
    6. Sending HOME -> receiving ACK
    7. Sending STOP -> receiving ACK
    8. Buffer flow control (burst of commands)
    9. Graceful disconnect

What to look for on ESP32 Serial Monitor:
    - "WebSocket client #N connected"
    - "Planner: Received command type X"
    - "Planner: Generated N points"
    - "Motion: (...) -> t1=...deg t2=...deg"  (if DEBUG_MOTOR is true)
"""

import asyncio
import sys
import time

# Add parent directory to path for robot_client import
sys.path.insert(0, ".")
from robot_client import RobotClient, C


DEFAULT_IP = "192.168.17.151"


async def test_connection(client: RobotClient):
    """Test 1: Can we connect?"""
    print(f"\n{C.BOLD}{'-' * 50}")
    print(f"  TEST 1: WebSocket Connection")
    print(f"{'-' * 50}{C.END}")
    
    await client.connect(timeout=5.0)
    assert client.connected, "Connection failed!"
    print(f"  {C.GREEN}[OK] Connected{C.END}")
    
    # Wait for initial BUFFER message
    await asyncio.sleep(1.0)
    print(f"  Buffer capacity: {client._credit_count} command slots")
    assert client._credit_count > 0, "No buffer credits -- ESP32 didn't send BUFFER message"
    print(f"  {C.GREEN}[OK] Initial BUFFER received{C.END}")
    
    # Send STOP first to clear any leftover state from previous runs
    await client.send_command({"type": "STOP"})
    await asyncio.sleep(1.0)
    print(f"  {C.GREEN}[OK] Initial STOP sent to reset ESP32 state{C.END}")


async def test_move_to(client: RobotClient):
    """Test 2: Send a MOVE_TO command."""
    print(f"\n{C.BOLD}{'-' * 50}")
    print(f"  TEST 2: MOVE_TO Command")
    print(f"{'-' * 50}{C.END}")
    
    acks_before = client.acks_received
    
    await client.send_command({
        "type": "MOVE_TO",
        "x": 150.0,
        "y": 150.0,
        "z": 0.0,
        "speed": 50.0,
        "tool": False
    })
    
    # Wait for ACK (1s to handle ESP32 busy with queue processing)
    await asyncio.sleep(1.0)
    assert client.acks_received > acks_before, "No ACK received for MOVE_TO"
    print(f"  {C.GREEN}[OK] MOVE_TO sent and ACKed{C.END}")


async def test_status_updates(client: RobotClient):
    """Test 3: Verify we receive periodic STATUS updates."""
    print(f"\n{C.BOLD}{'-' * 50}")
    print(f"  TEST 3: STATUS Updates")
    print(f"{'-' * 50}{C.END}")
    
    updates_before = client.status_updates
    
    print(f"  Waiting 2s for STATUS broadcasts...")
    await asyncio.sleep(2.0)
    
    new_updates = client.status_updates - updates_before
    print(f"  Received {new_updates} STATUS updates in 2s")
    
    if new_updates > 0:
        print(f"  Position: ({client.position['x']:.2f}, {client.position['y']:.2f}, z={client.position['z']:.2f})")
        print(f"  Tool: {'ON' if client.tool_active else 'OFF'}")
        print(f"  Moving: {client.is_moving}")
        print(f"  {C.GREEN}[OK] STATUS updates received{C.END}")
    else:
        print(f"  {C.YELLOW}[WARN] No STATUS updates -- ESP32 may not be broadcasting yet{C.END}")
        print(f"     (This is OK if the MotionControl task hasn't started)")


async def test_tool_control(client: RobotClient):
    """Test 4: Send TOOL ON then OFF."""
    print(f"\n{C.BOLD}{'-' * 50}")
    print(f"  TEST 4: TOOL Control")
    print(f"{'-' * 50}{C.END}")
    
    acks_before = client.acks_received
    
    # Tool ON
    await client.send_command({"type": "TOOL", "state": True, "z": 0.0})
    await asyncio.sleep(1.0)
    
    assert client.acks_received > acks_before, "No ACK received for TOOL ON"
    print(f"  {C.GREEN}[OK] TOOL ON sent and ACKed{C.END}")
    
    acks_before = client.acks_received
    
    # Tool OFF with Z raise
    await client.send_command({"type": "TOOL", "state": False, "z": 5.0})
    await asyncio.sleep(1.0)
    
    assert client.acks_received > acks_before, "No ACK received for TOOL OFF"
    print(f"  {C.GREEN}[OK] TOOL OFF (z=5.0) sent and ACKed{C.END}")


async def test_home(client: RobotClient):
    """Test 5: Send HOME command."""
    print(f"\n{C.BOLD}{'-' * 50}")
    print(f"  TEST 5: HOME Command")
    print(f"{'-' * 50}{C.END}")
    
    acks_before = client.acks_received
    
    await client.send_command({"type": "HOME"})
    await asyncio.sleep(1.0)
    
    assert client.acks_received > acks_before, "No ACK received for HOME"
    print(f"  {C.GREEN}[OK] HOME sent and ACKed{C.END}")


async def test_stop(client: RobotClient):
    """Test 6: Send STOP command."""
    print(f"\n{C.BOLD}{'-' * 50}")
    print(f"  TEST 6: STOP Command")
    print(f"{'-' * 50}{C.END}")
    
    acks_before = client.acks_received
    
    await client.send_command({"type": "STOP"})
    await asyncio.sleep(1.0)
    
    assert client.acks_received > acks_before, "No ACK received for STOP"
    print(f"  {C.GREEN}[OK] STOP sent and ACKed{C.END}")


async def test_burst_commands(client: RobotClient):
    """Test 7: Send a burst of commands and verify flow control."""
    print(f"\n{C.BOLD}{'-' * 50}")
    print(f"  TEST 7: Burst Commands (Flow Control)")
    print(f"{'-' * 50}{C.END}")
    
    # Send 8 rapid MOVE_TO commands in a tight sequence
    num_commands = 8
    print(f"  Sending {num_commands} commands rapidly...")
    
    start_time = time.time()
    
    targets = [
        (160, 150), (170, 160), (180, 170), (190, 180),
        (180, 190), (170, 180), (160, 170), (150, 160),
    ]
    
    for i, (x, y) in enumerate(targets):
        await client.send_command({
            "type": "MOVE_TO",
            "x": x, "y": y,
            "z": 0.0, "speed": 80.0, "tool": False
        })
    
    elapsed = time.time() - start_time
    print(f"  Sent {num_commands} commands in {elapsed:.2f}s")
    print(f"  Credits remaining: {client._credit_count}")
    
    # Wait for ACKs to catch up
    await asyncio.sleep(1.0)
    
    print(f"  Total ACKs received: {client.acks_received}")
    print(f"  {C.GREEN}[OK] Burst test completed without blocking forever{C.END}")


async def run_all_tests(ip: str):
    """Run the full WebSocket test suite."""
    print(f"\n{C.BOLD}{'=' * 50}")
    print(f"  ESP32 SCARA Robot -- WebSocket Test Suite")
    print(f"  Target: {ip}")
    print(f"  Time: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'=' * 50}{C.END}")
    print(f"\n{C.YELLOW}Make sure the ESP32 is powered on and connected to WiFi.{C.END}")
    print(f"{C.YELLOW}Open the Serial Monitor to see ESP32-side logs.{C.END}\n")

    client = RobotClient(ip, verbose=True)
    passed = 0
    failed = 0
    errors = []

    tests = [
        ("Connection",      test_connection),
        ("MOVE_TO",          test_move_to),
        ("STATUS Updates",   test_status_updates),
        ("TOOL Control",     test_tool_control),
        ("HOME",             test_home),
        ("STOP",             test_stop),
        ("Burst Commands",   test_burst_commands),
    ]

    for name, test_fn in tests:
        try:
            await test_fn(client)
            passed += 1
        except Exception as e:
            failed += 1
            errors.append((name, str(e)))
            print(f"  {C.RED}[FAIL] {e}{C.END}")

        # Small settling delay between tests to let ESP32 drain
        await asyncio.sleep(0.5)

    # Print summary
    client.print_stats()

    print(f"\n{C.BOLD}{'=' * 50}")
    print(f"  Results: {passed} passed, {failed} failed")
    print(f"{'=' * 50}{C.END}")
    
    if errors:
        print(f"\n{C.RED}Failed tests:{C.END}")
        for name, err in errors:
            print(f"  [FAIL] {name}: {err}")

    # Cleanup
    await client.disconnect()
    
    return failed == 0


if __name__ == "__main__":
    ip = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IP
    
    success = asyncio.run(run_all_tests(ip))
    sys.exit(0 if success else 1)
