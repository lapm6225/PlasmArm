#!/usr/bin/env python3
"""
test_json_commands.py -- Test G-Code style JSON commands via WebSocket.

Tests all command types:
  MOVE_TO, TOOL (UP/DOWN), DELAY, CONFIG_CHANGE, HOME, STOP

Usage:
    python test_json_commands.py [--ip IP]
"""

import asyncio
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from robot_client import RobotClient, C

DEFAULT_IP = "192.168.4.1"  # AP mode default


async def run_test(ip: str):
    client = RobotClient(ip, verbose=True)

    print(f"\n{'=' * 60}")
    print(f"  G-Code State Machine Test")
    print(f"{'=' * 60}\n")

    # Connect
    try:
        await client.connect(timeout=5.0)
    except Exception as e:
        print(f"{C.RED}Connection failed: {e}{C.END}")
        return

    # --- G-Code sequence (same as user's example) ---
    commands = [
    {"type": "TOOL", "state": "UP"},
    {"type": "DELAY", "ms": 500},
    {"type": "MOVE_TO", "x": 200, "y": 200, "speed": 100},
    {"type": "TOOL", "state": "DOWN"},
    {"type": "DELAY", "ms": 500},
    {"type": "MOVE_TO", "x": 200, "y": 300, "speed": 100},
    {"type": "MOVE_TO", "x": 300, "y": 300, "speed": 100},
    {"type": "TOOL", "state": "UP"},
    {"type": "DELAY", "ms": 500},
    {"type": "MOVE_TO", "x": 200, "y": 200, "speed": 100},
    {"type": "TOOL", "state": "DOWN"},
    {"type": "DELAY", "ms": 500},
        
        ]
    """
    {"type": "TOOL", "state": "UP"},
    {"type": "DELAY", "ms": 500},
    {"type": "MOVE_TO", "x": 200, "y": 200, "speed": 100},
    {"type": "TOOL", "state": "DOWN"},
    {"type": "DELAY", "ms": 500},
    {"type": "MOVE_TO", "x": 200, "y": 300, "speed": 100},
    {"type": "MOVE_TO", "x": 300, "y": 300, "speed": 100},
    {"type": "TOOL", "state": "UP"},
    {"type": "DELAY", "ms": 500},
    {"type": "MOVE_TO", "x": 200, "y": 200, "speed": 100},
    {"type": "TOOL", "state": "DOWN"},
    {"type": "DELAY", "ms": 500},


    {"type": "SET_HOME"}
    """ 

    print(f"\n{C.BOLD}Sending {len(commands)} G-code commands...{C.END}")
    print(f"  Commands are processed SEQUENTIALLY by the state machine.")
    print(f"  Each must complete before the next starts.")
    print(f"  Home calibration (send before this test):")
    print(f'    {{"type": "SET_HOME"}}   -- torque OFF, move arm by hand')
    print(
        f'    {{"type": "SAVE_HOME"}}  -- save current position as (0,0), persists to NVS\n'
    )

    for i, cmd in enumerate(commands):
        cmd_type = cmd["type"]
        desc = ""
        if cmd_type == "TOOL":
            desc = f"state={cmd['state']}"
        elif cmd_type == "MOVE_TO":
            desc = f"({cmd['x']}, {cmd['y']}) spd={cmd['speed']}"
        elif cmd_type == "DELAY":
            desc = f"{cmd['ms']}ms"

        print(f"{C.DIM}--- Step {i + 1}/{len(commands)}: {cmd_type} {desc} ---{C.END}")
        await client.send_command(cmd)

        # Time for the state machine to process each command sequentially
        if cmd_type == "DELAY":
            ms = cmd.get("ms", 0)
            await asyncio.sleep(ms / 1000.0 + 0.5)
        elif cmd_type == "MOVE_TO":
            await asyncio.sleep(2.0)  # Wait for move to complete
        elif cmd_type == "TOOL":
            await asyncio.sleep(1.0)  # Wait for tool actuation

    # Wait for robot to finish
    print(f"\n{C.BOLD}Waiting for robot to idle...{C.END}")
    await client.wait_idle(timeout=30.0)

    # Summary
    client.print_stats()
    await client.disconnect()

    print(f"\n{C.GREEN}[OK]{C.END} G-code state machine test completed!\n")


def main():
    parser = argparse.ArgumentParser(description="Test G-code JSON commands")
    parser.add_argument(
        "--ip", default=DEFAULT_IP, help=f"ESP32 IP (default: {DEFAULT_IP})"
    )
    args = parser.parse_args()
    asyncio.run(run_test(args.ip))


if __name__ == "__main__":
    main()
