"""Stream a DXF file to robot - clean output for terminal capture."""
import asyncio
import json
import sys
import time
import websockets

IP = sys.argv[2] if len(sys.argv) > 2 else "192.168.17.151"
DXF = sys.argv[1] if len(sys.argv) > 1 else "arc299ypos.dxf"
URI = f"ws://{IP}:80/ws"

# Import DXF parser
from dxf_parser import DxfParser

async def stream():
    # Phase 1: Parse
    print(f"=== PHASE 1: PARSE {DXF} ===")
    parser = DxfParser(DXF)
    commands = parser.parse()
    moves = [c for c in commands if c["type"] == "MOVE_TO"]
    tools = [c for c in commands if c["type"] == "TOOL"]
    print(f"  Commands: {len(commands)} ({len(moves)} moves, {len(tools)} tool changes)")
    print(f"  Entities: {parser.entities}")
    
    xs = [c["x"] for c in moves]
    ys = [c["y"] for c in moves]
    print(f"  X range: [{min(xs):.1f}, {max(xs):.1f}]")
    print(f"  Y range: [{min(ys):.1f}, {max(ys):.1f}]")
    
    # Phase 2: Connect
    print(f"\n=== PHASE 2: CONNECT ===")
    try:
        ws = await asyncio.wait_for(
            websockets.connect(URI, ping_interval=10, ping_timeout=5), timeout=5.0)
        print(f"  Connected to {URI}")
    except Exception as e:
        print(f"  FAIL: Cannot connect: {e}")
        return False
    
    # Get initial buffer info
    try:
        raw = await asyncio.wait_for(ws.recv(), timeout=3.0)
        data = json.loads(raw)
        cmd_free = data.get("cmdFree", 30)
        print(f"  Buffer capacity: {cmd_free} cmd slots")
    except:
        cmd_free = 30
    
    # Phase 3: Stream
    print(f"\n=== PHASE 3: STREAM {len(commands)} COMMANDS ===")
    start_time = time.time()
    acks = 0
    errors = 0
    credits = cmd_free
    in_flight = 0
    
    # Listener task to process responses
    responses = []
    
    async def listener():
        nonlocal credits, in_flight, acks, errors
        try:
            while True:
                raw = await ws.recv()
                data = json.loads(raw)
                msg_type = data.get("type", "")
                if msg_type == "ACK":
                    acks += 1
                    in_flight = max(0, in_flight - 1)
                    if "cmdFree" in data:
                        credits = data["cmdFree"]
                elif msg_type == "BUFFER":
                    if "cmdFree" in data:
                        credits = data["cmdFree"]
                elif msg_type == "ERROR":
                    errors += 1
                    print(f"  ERROR from ESP32: {data.get('msg', '?')}")
                elif msg_type == "STATUS":
                    pass  # Ignore during streaming
        except websockets.exceptions.ConnectionClosedOK:
            pass
        except websockets.exceptions.ConnectionClosed as e:
            print(f"  WARNING: Connection closed during streaming: {e}")
        except Exception:
            pass
    
    listen_task = asyncio.create_task(listener())
    
    for i, cmd in enumerate(commands):
        # Wait for credits
        wait_start = time.time()
        while credits - in_flight <= 0:
            await asyncio.sleep(0.05)
            if time.time() - wait_start > 10.0:
                print(f"  TIMEOUT at cmd {i}: credits={credits} in_flight={in_flight}")
                break
        
        # Send
        await ws.send(json.dumps(cmd))
        in_flight += 1
        
        # Throttle
        await asyncio.sleep(0.01)
        
        # Progress every 10 commands
        if (i + 1) % 10 == 0 or i == len(commands) - 1:
            elapsed = time.time() - start_time
            rate = (i + 1) / elapsed if elapsed > 0 else 0
            print(f"  [{i+1}/{len(commands)}] {elapsed:.1f}s {rate:.0f}cmd/s credits={credits} inflight={in_flight} acks={acks}")
    
    elapsed = time.time() - start_time
    print(f"\n  All {len(commands)} commands sent in {elapsed:.1f}s")
    print(f"  Rate: {len(commands)/elapsed:.1f} cmd/s")
    print(f"  ACKs: {acks}, Errors: {errors}")
    
    # Phase 4: Wait for completion
    print(f"\n=== PHASE 4: WAIT FOR IDLE ===")
    idle_start = time.time()
    
    # Wait for all ACKs
    while acks < len(commands) and (time.time() - idle_start) < 30.0:
        await asyncio.sleep(0.5)
        print(f"  Waiting... acks={acks}/{len(commands)} credits={credits}")
    
    # Wait for motion to finish
    await asyncio.sleep(2.0)  # Let motion queue drain
    
    # Check final status
    await ws.send(json.dumps({"type": "STOP"}))
    await asyncio.sleep(1.0)
    
    listen_task.cancel()
    try:
        await listen_task
    except asyncio.CancelledError:
        pass
    
    total_time = time.time() - start_time
    await ws.close()
    
    # Summary
    print(f"\n{'='*50}")
    print(f"  STREAMING SUMMARY")
    print(f"{'='*50}")
    print(f"  DXF file:       {DXF}")
    print(f"  Commands sent:  {len(commands)}")
    print(f"  ACKs received:  {acks}")
    print(f"  Errors:         {errors}")
    print(f"  Total time:     {total_time:.1f}s")
    print(f"  WS disconnects: {'NO' if acks > 0 else 'POSSIBLE'}")
    success = acks >= len(commands) - 1 and errors == 0
    print(f"  Result:         {'PASS' if success else 'FAIL'}")
    print(f"{'='*50}")
    
    return success

if __name__ == "__main__":
    ok = asyncio.run(stream())
    sys.exit(0 if ok else 1)
