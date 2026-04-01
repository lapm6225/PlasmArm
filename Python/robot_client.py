"""
robot_client.py -- Reusable async WebSocket client for the ESP32 SCARA robot.

Flow Control:
  1. On connect, the ESP32 sends {"type":"BUFFER","cmdFree":N}
  2. We track 'credits' = cmdFree - in_flight (commands sent but not ACKed)
  3. Each send increments in_flight, each ACK decrements it
  4. If usable credits reach 0, send_command() blocks until room is available

This properly accounts for commands in transit between Python and ESP32.

Usage:
    client = RobotClient("192.168.17.151")
    await client.connect()
    await client.send_command({"type":"MOVE_TO","x":100,"y":50})
    await client.disconnect()
"""

import asyncio
import websockets
import json
import time
from typing import Optional, Callable

# ANSI colors for terminal output
class C:
    HEADER  = '\033[95m'
    BLUE    = '\033[94m'
    CYAN    = '\033[96m'
    GREEN   = '\033[92m'
    YELLOW  = '\033[93m'
    RED     = '\033[91m'
    BOLD    = '\033[1m'
    DIM     = '\033[2m'
    END     = '\033[0m'


class RobotClient:
    """Async WebSocket client with buffer-based flow control."""

    def __init__(self, ip: str, port: int = 80, verbose: bool = True):
        self.uri = f"ws://{ip}:{port}/ws"
        self.ip = ip
        self.ws: Optional[websockets.WebSocketClientProtocol] = None
        self.connected = False
        self.verbose = verbose

        # Flow control
        # _server_free: last reported cmdFree from ESP32
        # _in_flight:   commands sent but not yet ACKed
        # usable credits = _server_free - _in_flight
        self._server_free = 0            # Last cmdFree from ESP32
        self._in_flight = 0              # Sent but not ACKed
        self._can_send = asyncio.Event() # Set when usable credits > 0
        self._max_credits = 0            # Max buffer size reported by ESP32

        # State tracking (updated from STATUS messages)
        self.position = {"x": 0.0, "y": 0.0, "z": 0.0}
        self.tool_active = False
        self.is_moving = False
        self.is_homed = False
        self.cmd_free = 0
        self.mot_free = 0

        # Listener task handle
        self._listener_task: Optional[asyncio.Task] = None

        # Stats
        self.commands_sent = 0
        self.acks_received = 0
        self.status_updates = 0

        # Callbacks (optional)
        self.on_status: Optional[Callable] = None
        self.on_disconnect: Optional[Callable] = None

    @property
    def _credit_count(self):
        """Usable credits = server free slots minus commands in flight."""
        return max(0, self._server_free - self._in_flight)

    def _log(self, msg: str, color: str = C.DIM):
        if self.verbose:
            ts = time.strftime("%H:%M:%S")
            print(f"{C.DIM}[{ts}]{C.END} {color}{msg}{C.END}")

    def _log_rx(self, msg_type: str, details: str = ""):
        self._log(f"  <- {msg_type} {details}", C.CYAN)

    def _log_tx(self, msg_type: str, details: str = ""):
        self._log(f"  -> {msg_type} {details}", C.GREEN)

    async def connect(self, timeout: float = 5.0):
        """Connect to the robot's WebSocket server."""
        self._log(f"Connecting to {self.uri}...", C.YELLOW)
        try:
            self.ws = await asyncio.wait_for(
                websockets.connect(self.uri, ping_interval=10, ping_timeout=5),
                timeout=timeout
            )
            self.connected = True
            self._log(f"Connected to {self.ip}", C.GREEN)

            # Start background listener
            self._listener_task = asyncio.create_task(self._listener())

            # Wait briefly for initial BUFFER message from ESP32
            await asyncio.sleep(0.3)

            if self._server_free > 0:
                self._log(f"Buffer capacity: {self._server_free} slots", C.BLUE)
            else:
                self._log("No initial BUFFER message -- defaulting to 10 credits", C.YELLOW)
                self._update_server_free(10)

        except asyncio.TimeoutError:
            self._log(f"Connection timeout after {timeout}s", C.RED)
            raise ConnectionError(f"Cannot connect to {self.uri}")
        except Exception as e:
            self._log(f"Connection failed: {e}", C.RED)
            raise

    async def disconnect(self):
        """Gracefully close the WebSocket connection."""
        if self._listener_task:
            self._listener_task.cancel()
            try:
                await self._listener_task
            except asyncio.CancelledError:
                pass
        if self.ws:
            await self.ws.close()
        self.connected = False
        self._log("Disconnected.", C.YELLOW)

    def _update_server_free(self, server_free: int):
        """Update server-reported free slots and wake senders if possible."""
        self._server_free = server_free
        self._max_credits = max(self._max_credits, server_free)
        if self._credit_count > 0:
            self._can_send.set()
        else:
            self._can_send.clear()

    def _ack_received(self, server_free: int):
        """An ACK means one in-flight command was accepted."""
        self._in_flight = max(0, self._in_flight - 1)
        self._server_free = server_free
        self.acks_received += 1
        if self._credit_count > 0:
            self._can_send.set()
        else:
            self._can_send.clear()

    async def _listener(self):
        """Background task: listens for all messages from ESP32."""
        try:
            async for raw in self.ws:
                try:
                    data = json.loads(raw)
                except json.JSONDecodeError:
                    self._log(f"Non-JSON message: {raw[:60]}", C.YELLOW)
                    continue

                msg_type = data.get("type", "UNKNOWN")

                if msg_type == "ACK":
                    cmd_free = data.get("cmdFree", self._server_free)
                    self._ack_received(cmd_free)
                    self._log_rx("ACK", f"cmdFree={cmd_free} (inflight={self._in_flight})")

                elif msg_type == "BUFFER":
                    # Authoritative buffer status (also sent on connect)
                    cmd_free = data.get("cmdFree", 0)
                    mot_free = data.get("motFree", 0)
                    self._update_server_free(cmd_free)
                    self.cmd_free = cmd_free
                    self.mot_free = mot_free
                    self._log_rx("BUFFER", f"cmdFree={cmd_free} motFree={mot_free}")

                elif msg_type == "STATUS":
                    # Periodic position/state update
                    self.position = {
                        "x": data.get("x", 0.0),
                        "y": data.get("y", 0.0),
                        "z": data.get("z", 0.0),
                    }
                    self.tool_active = data.get("tool", False)
                    self.is_moving = data.get("isMoving", False)
                    self.is_homed = data.get("isHomed", False)
                    self.cmd_free = data.get("cmdFree", self.cmd_free)
                    self.mot_free = data.get("motFree", self.mot_free)
                    self.status_updates += 1

                    # Update server_free from status too
                    if "cmdFree" in data:
                        self._update_server_free(data["cmdFree"])

                    if self.on_status:
                        self.on_status(data)

                elif msg_type == "ERROR":
                    self._log_rx("ERROR", f"{data.get('msg', '?')}")

                else:
                    self._log_rx(msg_type, json.dumps(data)[:80])

        except websockets.exceptions.ConnectionClosed as e:
            self._log(f"Connection closed: {e}", C.RED)
            self.connected = False
            if self.on_disconnect:
                self.on_disconnect()
        except asyncio.CancelledError:
            pass  # Normal shutdown

    async def send_command(self, cmd: dict, timeout: float = 10.0):
        """
        Send a command to the robot, respecting flow control.
        
        Blocks if the ESP32's command buffer is full (no usable credits).
        Raises TimeoutError if we wait longer than `timeout` seconds.
        """
        if not self.connected or not self.ws:
            raise ConnectionError("Not connected to robot")

        # Wait for at least 1 usable credit
        while self._credit_count <= 0:
            self._can_send.clear()
            try:
                await asyncio.wait_for(self._can_send.wait(), timeout=timeout)
            except asyncio.TimeoutError:
                raise TimeoutError(
                    f"Buffer full for {timeout}s -- ESP32 not consuming commands? "
                    f"server_free={self._server_free} in_flight={self._in_flight}"
                )

        # Mark as in-flight BEFORE sending (conservative)
        self._in_flight += 1
        if self._credit_count <= 0:
            self._can_send.clear()

        # Send
        payload = json.dumps(cmd)
        await self.ws.send(payload)
        self.commands_sent += 1

        # Throttle: give ESP32 time to process and send ACK.
        # Without this, rapid sends overwhelm the WS outbound queue.
        # 10ms = max ~100 cmd/s, plenty for robot motion.
        await asyncio.sleep(0.01)

        cmd_type = cmd.get("type", "?")
        if cmd_type == "MOVE_TO":
            self._log_tx("MOVE_TO", f"({cmd.get('x',0):.1f}, {cmd.get('y',0):.1f}, z={cmd.get('z',0):.1f}, tool={cmd.get('tool',False)})")
        elif cmd_type == "TOOL":
            self._log_tx("TOOL", f"state={cmd.get('state',False)} z={cmd.get('z',0):.1f}")
        else:
            self._log_tx(cmd_type, json.dumps(cmd)[:60])

    async def wait_idle(self, timeout: float = 120.0, poll_interval: float = 0.5):
        """Wait until the robot reports isMoving=False."""
        self._log("Waiting for robot to finish moving...", C.DIM)
        start = time.time()
        while time.time() - start < timeout:
            if not self.is_moving and self.status_updates > 0:
                self._log("Robot idle.", C.GREEN)
                return True
            await asyncio.sleep(poll_interval)
        self._log(f"Timeout after {timeout}s -- robot may still be moving", C.YELLOW)
        return False

    def print_stats(self):
        """Print session statistics."""
        print(f"\n{C.BOLD}{'=' * 50}")
        print(f"  Session Statistics")
        print(f"{'=' * 50}{C.END}")
        print(f"  Commands sent:    {self.commands_sent}")
        print(f"  ACKs received:    {self.acks_received}")
        print(f"  In-flight:        {self._in_flight}")
        print(f"  Status updates:   {self.status_updates}")
        print(f"  Final position:   ({self.position['x']:.2f}, {self.position['y']:.2f}, z={self.position['z']:.2f})")
        print(f"  Tool active:      {self.tool_active}")
        print(f"  Is moving:        {self.is_moving}")
        print(f"  Buffer (cmd):     {self.cmd_free} free")
        print(f"  Buffer (motion):  {self.mot_free} free")
        print(f"{C.BOLD}{'=' * 50}{C.END}\n")
