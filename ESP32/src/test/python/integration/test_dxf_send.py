"""
test_dxf_send.py -- Layer 2: Full DXF -> ESP32 integration test.

Parses a DXF file, converts it to robot commands, and streams them
to the ESP32 via WebSocket with proper flow control.

Usage:
    python test_dxf_send.py <file.dxf> [ESP32_IP]
    python test_dxf_send.py ../dxf/drawing_simple.dxf 192.168.17.151

What it tests:
    1. DXF parsing (entities -> commands)
    2. Command preview (dry run)
    3. WebSocket connection
    4. Streaming commands with buffer flow control
    5. Waiting for completion
    6. Position verification

What to look for on ESP32 Serial Monitor:
    - Each "Planner: Received command type X" in sequence
    - Tool state changes: "Planner: Tool ON/OFF, Z=X.XX"
    - Motion control IK calculations (if DEBUG_MOTOR=true)
    - Buffer broadcasts when queue empties
"""

import asyncio
import sys
import time
import os

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from robot_client import RobotClient, C
from dxf_parser import parse_dxf, preview_commands


DEFAULT_IP = "192.168.17.151"


async def test_dxf_dry_run(filename: str):
    """Phase 1: Parse DXF and show what would be sent (no connection needed)."""
    print(f"\n{C.BOLD}{'=' * 60}")
    print(f"  PHASE 1: DXF Parsing (Dry Run)")
    print(f"{'=' * 60}{C.END}")
    
    commands = parse_dxf(filename, verbose=True)
    
    if not commands:
        print(f"  {C.RED}[FAIL] No commands generated from DXF file{C.END}")
        return None
    
    preview_commands(commands, max_lines=40)
    
    print(f"\n  {C.GREEN}[OK] DXF parsed successfully: {len(commands)} commands{C.END}")
    return commands


async def test_dxf_send(filename: str, ip: str):
    """Phase 2: Send DXF commands to ESP32 via WebSocket."""
    # Phase 1: Parse
    commands = await test_dxf_dry_run(filename)
    if commands is None:
        return False
    
    # Ask user before sending
    print(f"\n{C.YELLOW}Ready to send {len(commands)} commands to {ip}")
    print(f"Press Enter to continue, or Ctrl+C to abort...{C.END}")
    try:
        input()
    except KeyboardInterrupt:
        print("\nAborted.")
        return False
    
    # Phase 2: Connect and send
    print(f"\n{C.BOLD}{'=' * 60}")
    print(f"  PHASE 2: Streaming to ESP32")
    print(f"{'=' * 60}{C.END}")
    
    client = RobotClient(ip, verbose=True)
    
    try:
        await client.connect(timeout=5.0)
    except Exception as e:
        print(f"  {C.RED}[FAIL] Connection failed: {e}{C.END}")
        return False
    
    # Send commands with progress reporting
    total = len(commands)
    start_time = time.time()
    last_progress = 0
    
    try:
        for i, cmd in enumerate(commands):
            await client.send_command(cmd, timeout=15.0)
            
            # Progress bar (ASCII safe)
            progress = int((i + 1) / total * 100)
            if progress >= last_progress + 5 or i == total - 1:
                elapsed = time.time() - start_time
                bar_len = 30
                filled = int(bar_len * (i + 1) / total)
                bar = "#" * filled + "." * (bar_len - filled)
                rate = (i + 1) / elapsed if elapsed > 0 else 0
                
                print(f"\r  [{bar}] {progress:3d}% ({i+1}/{total}) "
                      f"| {elapsed:.1f}s | {rate:.1f} cmd/s "
                      f"| credits={client._credit_count}", end="")
                last_progress = progress
        
        print()  # Newline after progress bar
        
        elapsed = time.time() - start_time
        print(f"\n  {C.GREEN}[OK] All {total} commands sent in {elapsed:.1f}s{C.END}")
        print(f"  Average rate: {total/elapsed:.1f} commands/second")
        
    except TimeoutError as e:
        print(f"\n  {C.RED}[FAIL] Timeout during send: {e}{C.END}")
        print(f"  Commands sent: {client.commands_sent}/{total}")
        await client.disconnect()
        return False
    except KeyboardInterrupt:
        print(f"\n  {C.YELLOW}[WARN] Interrupted! {client.commands_sent}/{total} commands sent{C.END}")
        # Send STOP for safety
        try:
            await client.send_command({"type": "STOP"})
        except:
            pass
        await client.disconnect()
        return False
    
    # Phase 3: Wait for completion
    print(f"\n{C.BOLD}{'=' * 60}")
    print(f"  PHASE 3: Waiting for Robot to Finish")
    print(f"{'=' * 60}{C.END}")
    
    # Give the robot time to process the motion queue
    print(f"  Waiting for motion queue to drain...")
    await asyncio.sleep(2.0)
    
    # Poll position until robot stops
    idle = await client.wait_idle(timeout=60.0, poll_interval=1.0)
    
    if idle:
        print(f"  {C.GREEN}[OK] Robot finished all movements{C.END}")
    else:
        print(f"  {C.YELLOW}[WARN] Robot may still be processing{C.END}")
    
    # Print stats
    client.print_stats()
    
    # Cleanup
    await client.disconnect()
    
    return True


async def test_dxf_interactive(ip: str):
    """Interactive mode: choose file, adjust parameters, send."""
    print(f"\n{C.BOLD}{'=' * 60}")
    print(f"  ESP32 SCARA Robot -- DXF Integration Test")
    print(f"  Target: {ip}")
    print(f"{'=' * 60}{C.END}")
    
    # List available DXF files
    dxf_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "dxf")
    dxf_files = []
    
    if os.path.exists(dxf_dir):
        dxf_files = [f for f in os.listdir(dxf_dir) if f.endswith(".dxf")]
    
    if dxf_files:
        print(f"\nAvailable DXF files in test/python/dxf/:")
        for i, f in enumerate(dxf_files):
            size = os.path.getsize(os.path.join(dxf_dir, f))
            print(f"  {i+1}. {f} ({size} bytes)")
    
    while True:
        print(f"\n{C.CYAN}Enter DXF file path (or number from list, 'q' to quit):{C.END}")
        choice = input("> ").strip()
        
        if choice.lower() in ("q", "quit", "exit"):
            break
        
        # Handle number selection
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(dxf_files):
                filename = os.path.join(dxf_dir, dxf_files[idx])
            else:
                print(f"  {C.RED}Invalid selection{C.END}")
                continue
        except ValueError:
            filename = choice
        
        if not os.path.exists(filename):
            print(f"  {C.RED}File not found: {filename}{C.END}")
            continue
        
        # Parse and send
        success = await test_dxf_send(filename, ip)
        
        if success:
            print(f"\n{C.GREEN}[OK] DXF test completed successfully!{C.END}")
        else:
            print(f"\n{C.RED}[FAIL] DXF test had issues -- check ESP32 Serial Monitor{C.END}")
        
        print(f"\n{'-' * 60}")


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] != "--interactive":
        # Direct mode: provide DXF file as argument
        filename = sys.argv[1]
        ip = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_IP
        
        success = asyncio.run(test_dxf_send(filename, ip))
        sys.exit(0 if success else 1)
    else:
        # Interactive mode
        ip = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_IP
        asyncio.run(test_dxf_interactive(ip))
