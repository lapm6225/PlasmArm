#!/usr/bin/env python3
"""
test_dxf_send.py -- Parse a DXF file and stream commands to ESP32.

Usage:
    python test_dxf_send.py <dxf_file> [--ip IP] [--dry-run]
    
Examples:
    python test_dxf_send.py arc299.5.dxf
    python test_dxf_send.py arc299.5.dxf --dry-run
    python test_dxf_send.py arc299.5.dxf --ip 192.168.17.151
"""

import asyncio
import argparse
import os
import sys
import time

# Add parent directory so we can import our modules
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from dxf_parser import DxfParser
from robot_client import RobotClient, C

DEFAULT_IP = "192.168.17.151"


def phase_header(num: int, title: str):
    """Print a phase header."""
    print(f"\n{C.BOLD}{'=' * 60}")
    print(f"  PHASE {num}: {title}")
    print(f"{'=' * 60}{C.END}\n")


def parse_dxf(filename: str) -> list:
    """Phase 1: Parse DXF and show preview (dry run)."""
    phase_header(1, "DXF Parsing (Dry Run)")
    
    parser = DxfParser(filename)
    print(f"Parsing DXF: {os.path.basename(filename)}")
    
    commands = parser.parse()
    
    # Stats
    entities = parser.entities
    move_count = sum(1 for c in commands if c["type"] == "MOVE_TO")
    tool_count = sum(1 for c in commands if c["type"] == "TOOL")
    xs = [c["x"] for c in commands if c["type"] == "MOVE_TO"]
    ys = [c["y"] for c in commands if c["type"] == "MOVE_TO"]
    
    shapes_count = 0
    doc = __import__("ezdxf").readfile(filename)
    for _ in doc.modelspace():
        shapes_count += 1
    
    print(f"  Entities found: {entities}")
    print(f"  Shapes extracted: {shapes_count}")
    vertices = sum(1 for c in commands if c["type"] == "MOVE_TO")
    print(f"  Total vertices: {vertices}")
    print(f"  Commands generated: {len(commands)} total")
    print(f"    MOVE_TO: {move_count}")
    print(f"    TOOL:    {tool_count}")
    if xs and ys:
        print(f"  Bounding box: X=[{min(xs):.1f}, {max(xs):.1f}] Y=[{min(ys):.1f}, {max(ys):.1f}]")
    
    parser.print_preview(commands)
    
    print(f"\n  {C.GREEN}[OK]{C.END} DXF parsed successfully: {len(commands)} commands")
    return commands


async def stream_to_robot(commands: list, ip: str):
    """Phase 2 & 3: Stream commands to ESP32 and wait for completion."""
    
    # ---- Phase 2: Stream ----
    phase_header(2, "Streaming to ESP32")
    
    client = RobotClient(ip, verbose=True)
    
    try:
        await client.connect(timeout=5.0)
    except Exception as e:
        print(f"  {C.RED}[FAIL]{C.END} Cannot connect: {e}")
        return False
    
    total = len(commands)
    start_time = time.time()
    bar_width = 30
    
    try:
        for i, cmd in enumerate(commands):
            await client.send_command(cmd)
            
            # Progress bar every 3 commands
            if (i + 1) % 3 == 0 or i == total - 1:
                pct = (i + 1) / total
                filled = int(bar_width * pct)
                bar = '#' * filled + '.' * (bar_width - filled)
                elapsed = time.time() - start_time
                rate = (i + 1) / elapsed if elapsed > 0 else 0
                credits = client._credit_count
                print(f"  [{bar}] {pct:4.0%} ({i+1}/{total}) | {elapsed:.1f}s | {rate:.1f} cmd/s | credits={credits}", end='\r')
        
        print()  # Newline after progress bar
        
        elapsed = time.time() - start_time
        rate = total / elapsed if elapsed > 0 else 0
        print(f"\n  {C.GREEN}[OK]{C.END} All {total} commands sent in {elapsed:.1f}s")
        print(f"  Average rate: {rate:.1f} commands/second")
    
    except Exception as e:
        print(f"\n  {C.RED}[FAIL]{C.END} Error during streaming: {e}")
        await client.disconnect()
        return False
    
    # ---- Phase 3: Wait ----
    phase_header(3, "Waiting for Robot to Finish")
    print("  Waiting for motion queue to drain...")
    
    success = await client.wait_idle(timeout=120.0)
    
    if success:
        print(f"  {C.GREEN}[OK]{C.END} Robot finished all movements")
    else:
        print(f"  {C.YELLOW}[WARN]{C.END} Timeout -- robot may still be moving")
    
    client.print_stats()
    await client.disconnect()
    
    return success


def main():
    parser = argparse.ArgumentParser(description="Stream DXF to ESP32 robot")
    parser.add_argument("dxf_file", help="Path to DXF file")
    parser.add_argument("--ip", default=DEFAULT_IP, help=f"ESP32 IP address (default: {DEFAULT_IP})")
    parser.add_argument("--dry-run", action="store_true", help="Parse only, don't connect to robot")
    args = parser.parse_args()
    
    if not os.path.exists(args.dxf_file):
        print(f"Error: File not found: {args.dxf_file}")
        sys.exit(1)
    
    # Phase 1: Always parse
    commands = parse_dxf(args.dxf_file)
    
    if args.dry_run:
        print(f"\n{C.GREEN}[OK]{C.END} DXF test completed (dry run)!\n")
        print("-" * 60)
        return
    
    # Phase 2 & 3: Stream to robot
    success = asyncio.run(stream_to_robot(commands, args.ip))
    
    if success:
        print(f"\n{C.GREEN}[OK]{C.END} DXF test completed successfully!\n")
    else:
        print(f"\n{C.RED}[FAIL]{C.END} DXF test completed with errors\n")
        sys.exit(1)
    
    print("-" * 60)


if __name__ == "__main__":
    main()
