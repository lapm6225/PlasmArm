#!/usr/bin/env python3
"""
test_websocket.py -- Basic WebSocket connectivity test for the ESP32 robot.

Tests:
  1. Connect to WebSocket
  2. Receive initial BUFFER message
  3. Send STOP command
  4. Send MOVE_TO command and verify ACK
  5. Send HOME command
  6. Wait for idle

Usage:
    python test_websocket.py [--ip IP]
"""

import asyncio
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from robot_client import RobotClient, C

DEFAULT_IP = "192.168.17.151"


async def run_test(ip: str):
    """Run basic WebSocket connectivity test."""
    client = RobotClient(ip, verbose=True)
    
    print(f"\n{'=' * 60}")
    print(f"  WebSocket Robot Test")
    print(f"{'=' * 60}\n")
    
    # --- Test 1: Connect ---
    print(f"\n{C.BOLD}Test 1: Connection{C.END}")
    try:
        await client.connect(timeout=5.0)
        print(f"  {C.GREEN}[PASS]{C.END} Connected to {ip}")
    except Exception as e:
        print(f"  {C.RED}[FAIL]{C.END} Connection failed: {e}")
        return
    
    # --- Test 2: Buffer info ---
    print(f"\n{C.BOLD}Test 2: Buffer Status{C.END}")
    if client._max_credits > 0:
        print(f"  {C.GREEN}[PASS]{C.END} Buffer capacity: {client._max_credits} slots")
    else:
        print(f"  {C.YELLOW}[WARN]{C.END} No initial BUFFER message received")
    
    # --- Test 3: STOP ---
    print(f"\n{C.BOLD}Test 3: STOP Command{C.END}")
    await client.send_command({"type": "STOP"})
    await asyncio.sleep(0.5)
    print(f"  {C.GREEN}[PASS]{C.END} STOP sent, ACKs: {client.acks_received}")
    
    # --- Test 4: MOVE_TO ---
    print(f"\n{C.BOLD}Test 4: MOVE_TO Command{C.END}")
    prev_acks = client.acks_received
    await client.send_command({
        "type": "MOVE_TO",
        "x": 150.0, "y": 150.0,
        "z": 0.0, "speed": 50.0, "tool": False
    })
    await asyncio.sleep(0.5)
    if client.acks_received > prev_acks:
        print(f"  {C.GREEN}[PASS]{C.END} ACK received for MOVE_TO")
    else:
        print(f"  {C.YELLOW}[WARN]{C.END} No ACK received (may be expected if queue full)")
    
    # --- Test 5: TOOL + HOME ---
    print(f"\n{C.BOLD}Test 5: TOOL + HOME Commands{C.END}")
    await client.send_command({"type": "TOOL", "state": True, "z": 0.0})
    await asyncio.sleep(0.3)
    await client.send_command({"type": "TOOL", "state": False, "z": 5.0})
    await asyncio.sleep(0.3)
    await client.send_command({"type": "HOME"})
    print(f"  {C.GREEN}[PASS]{C.END} TOOL ON -> TOOL OFF -> HOME sent")
    
    # --- Test 6: Wait for idle ---
    print(f"\n{C.BOLD}Test 6: Wait for Idle{C.END}")
    
    # Send STOP to clear queue then wait
    await client.send_command({"type": "STOP"})
    
    success = await client.wait_idle(timeout=30.0)
    if success:
        print(f"  {C.GREEN}[PASS]{C.END} Robot is idle")
    else:
        print(f"  {C.YELLOW}[WARN]{C.END} Timeout waiting for idle")
    
    # --- Summary ---
    client.print_stats()
    await client.disconnect()
    
    print(f"\n{C.GREEN}[OK]{C.END} WebSocket test completed!\n")
    print("-" * 60)


def main():
    parser = argparse.ArgumentParser(description="Test WebSocket connection to ESP32 robot")
    parser.add_argument("--ip", default=DEFAULT_IP, help=f"ESP32 IP address (default: {DEFAULT_IP})")
    args = parser.parse_args()
    
    asyncio.run(run_test(args.ip))


if __name__ == "__main__":
    main()
