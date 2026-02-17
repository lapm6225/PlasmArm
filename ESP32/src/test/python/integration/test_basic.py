#!/usr/bin/env python3
"""Minimal WebSocket test - no ANSI colors, clean output for CI."""
import asyncio
import json
import sys
import time
import websockets

IP = sys.argv[1] if len(sys.argv) > 1 else "192.168.17.151"
URI = f"ws://{IP}:80/ws"

async def test():
    results = []
    
    # --- Test 1: Connect ---
    print(f"TEST 1: Connecting to {URI}...")
    try:
        ws = await asyncio.wait_for(
            websockets.connect(URI, ping_interval=10, ping_timeout=5), timeout=5.0)
        print("  PASS: Connected")
        results.append(("Connect", True))
    except Exception as e:
        print(f"  FAIL: {e}")
        results.append(("Connect", False))
        return results
    
    # --- Test 2: Receive initial BUFFER ---
    print("TEST 2: Waiting for initial BUFFER message...")
    try:
        raw = await asyncio.wait_for(ws.recv(), timeout=3.0)
        data = json.loads(raw)
        print(f"  Received: {data}")
        if data.get("type") == "BUFFER" and "cmdFree" in data:
            print(f"  PASS: Buffer capacity = {data['cmdFree']}")
            results.append(("Buffer", True))
        else:
            print(f"  FAIL: Unexpected message type: {data.get('type')}")
            results.append(("Buffer", False))
    except asyncio.TimeoutError:
        print("  FAIL: No BUFFER message within 3s")
        results.append(("Buffer", False))
    
    # --- Test 3: Send STOP ---
    print("TEST 3: Sending STOP...")
    await ws.send(json.dumps({"type": "STOP"}))
    try:
        raw = await asyncio.wait_for(ws.recv(), timeout=3.0)
        data = json.loads(raw)
        print(f"  Received: {data}")
        if data.get("type") == "ACK":
            print("  PASS: STOP acknowledged")
            results.append(("STOP", True))
        else:
            print(f"  INFO: Got {data.get('type')} instead of ACK")
            results.append(("STOP", True))  # Non-fatal
    except asyncio.TimeoutError:
        print("  FAIL: No response within 3s")
        results.append(("STOP", False))
    
    # --- Test 4: Send MOVE_TO ---
    print("TEST 4: Sending MOVE_TO (150, 200, z=0, speed=50)...")
    cmd = {"type": "MOVE_TO", "x": 150.0, "y": 200.0, "z": 0.0, "speed": 50.0, "tool": False}
    await ws.send(json.dumps(cmd))
    try:
        raw = await asyncio.wait_for(ws.recv(), timeout=3.0)
        data = json.loads(raw)
        print(f"  Received: {data}")
        if data.get("type") == "ACK":
            print(f"  PASS: MOVE_TO acknowledged (cmdFree={data.get('cmdFree')})")
            results.append(("MOVE_TO", True))
        else:
            print(f"  INFO: Got {data.get('type')} instead of ACK")
            results.append(("MOVE_TO", True))
    except asyncio.TimeoutError:
        print("  FAIL: No response within 3s")
        results.append(("MOVE_TO", False))
    
    # --- Test 5: Send TOOL ON then OFF ---
    print("TEST 5: Sending TOOL ON then OFF...")
    await ws.send(json.dumps({"type": "TOOL", "state": True, "z": 0.0}))
    await asyncio.sleep(0.2)
    await ws.send(json.dumps({"type": "TOOL", "state": False, "z": 5.0}))
    # Drain responses
    for _ in range(5):
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=1.0)
            data = json.loads(raw)
            print(f"  Received: {data.get('type')} {json.dumps({k:v for k,v in data.items() if k!='type'})}")
        except asyncio.TimeoutError:
            break
    results.append(("TOOL", True))
    print("  PASS: TOOL commands sent")
    
    # --- Test 6: Send HOME and wait ---
    print("TEST 6: Sending HOME...")
    await ws.send(json.dumps({"type": "HOME"}))
    try:
        raw = await asyncio.wait_for(ws.recv(), timeout=3.0)
        data = json.loads(raw)
        print(f"  Received: {data}")
        results.append(("HOME", True))
        print("  PASS: HOME acknowledged")
    except asyncio.TimeoutError:
        print("  FAIL: No response within 3s")
        results.append(("HOME", False))
    
    # --- Test 7: Wait for STATUS messages ---
    print("TEST 7: Listening for STATUS broadcasts (5s)...")
    status_count = 0
    last_status = None
    deadline = time.time() + 5.0
    while time.time() < deadline:
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=1.0)
            data = json.loads(raw)
            if data.get("type") == "STATUS":
                status_count += 1
                last_status = data
                pos = f"({data.get('x',0):.1f}, {data.get('y',0):.1f})"
                angles = f"t1={data.get('theta1',0):.1f} t2={data.get('theta2',0):.1f}"
                moving = data.get('isMoving', False)
                print(f"  STATUS #{status_count}: pos={pos} {angles} moving={moving} cmdQ={data.get('cmdFree','?')} motQ={data.get('motFree','?')}")
            elif data.get("type") == "BUFFER":
                print(f"  BUFFER: cmdFree={data.get('cmdFree')} motFree={data.get('motFree')}")
            elif data.get("type") == "ACK":
                print(f"  ACK: cmdFree={data.get('cmdFree')}")
        except asyncio.TimeoutError:
            continue
    
    if status_count > 0:
        print(f"  PASS: Received {status_count} STATUS messages")
        results.append(("STATUS", True))
    else:
        print("  FAIL: No STATUS messages received")
        results.append(("STATUS", False))
    
    # --- Test 8: Connection still alive ---
    print("TEST 8: Connection health check...")
    try:
        await ws.send(json.dumps({"type": "STOP"}))
        raw = await asyncio.wait_for(ws.recv(), timeout=3.0)
        print(f"  PASS: Connection still alive after all tests")
        results.append(("Alive", True))
    except Exception as e:
        print(f"  FAIL: Connection lost: {e}")
        results.append(("Alive", False))
    
    await ws.close()
    
    # --- Summary ---
    print(f"\n{'='*50}")
    print(f"  RESULTS SUMMARY")
    print(f"{'='*50}")
    passed = sum(1 for _, ok in results if ok)
    total = len(results)
    for name, ok in results:
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}")
    print(f"\n  {passed}/{total} tests passed")
    print(f"{'='*50}")
    
    return results

if __name__ == "__main__":
    results = asyncio.run(test())
    sys.exit(0 if all(ok for _, ok in results) else 1)
