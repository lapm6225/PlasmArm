import asyncio
import json
import sys
import time
from typing import Optional
from PyQt6.QtCore import QThread, pyqtSignal

try:
    import websockets
    import websockets.client
except ImportError:
    print("ERROR: 'websockets' is not installed.")
    print("  pip install websockets")
    sys.exit(1)

# ============================================================================
# Configuration constants
# ============================================================================
DEFAULT_IP = "192.168.4.1"
WS_PORT = 80
WS_PATH = "/ws"
DEFAULT_SPEED = 50.0
FLOW_CONTROL_BATCH = 5        # Number of commands sent per batch
RECONNECT_DELAY = 3.0         # Seconds between reconnection attempts
STREAM_WATCHDOG_TIMEOUT = 15.0  # Timeout without progress before aborting
STREAM_MIN_CMDFREE = 2          # Minimum free slots required before sending


# ============================================================================
# Robot WebSocket Client
# ============================================================================
class RobotWSClient:
    """Asynchronous WebSocket client used to communicate with the ESP32."""

    def __init__(self):
        # WebSocket connection handle
        self.ws: Optional[websockets.client.WebSocketClientProtocol] = None
        self.connected: bool = False
        self.ip: str = DEFAULT_IP

        # Flow control counters (cmd_free is authoritative from ESP32)
        self.cmd_free = 30       # Free command slots reported by ESP32
        self.mot_free = 1000     # Free motion queue slots

        # Synchronization for handled counter
        self.handled_offset = 0  # Initial handled value at session start
        self.last_handled = 0    # Last handled value received

        # Commands sent but not yet acknowledged
        self.in_flight = 0

        # Event triggered when enough free slots are available
        self.flow_event = asyncio.Event()
        self.flow_event.set()

        # Last STATUS message received
        self.last_status = {}
        self.msg_count = 0
        self.ack_count = 0

        # Streaming state
        self.streaming = False
        self.stream_total = 0
        self.stream_sent = 0
        self.stream_done = asyncio.Event()
        self._last_progress_time = 0.0  # Watchdog timestamp

    async def connect(self, ip: str, timeout: float = 5.0):
        """Connect to the ESP32 WebSocket server."""
        self.ip = ip
        uri = f"ws://{ip}:{WS_PORT}{WS_PATH}"
        print(f"  Connecting to {uri}...")
        try:
            # Attempt connection with timeout
            self.ws = await asyncio.wait_for(
                websockets.connect(
                    uri,
                    ping_interval=None,
                    ping_timeout=None,
                    max_size=2**20,  # Allow large messages
                ),
                timeout=timeout
            )
            self.connected = True

            # Reset counters on new connection
            self.in_flight = 0
            self.cmd_free = 30

            print(f"  ✅ Connected to {uri}")
            return True

        except asyncio.TimeoutError:
            print(f"  ❌ Connection failed: Timeout after {timeout}s.")
            self.connected = False
            return False

        except Exception as e:
            print(f"  ❌ Connection error: {e}")
            self.connected = False
            return False

    async def disconnect(self):
        """Close the WebSocket connection."""
        if self.ws:
            await self.ws.close()
            self.ws = None
        self.connected = False
        print("  Disconnected.")

    async def send_json(self, data: dict):
        """Send a JSON message through the WebSocket."""
        if not self.connected or not self.ws:
            print("  ❌ Not connected! Use 'connect <ip>' first.")
            return False
        try:
            msg = json.dumps(data)
            await self.ws.send(msg)

            # Optimistic increment: decremented when ACK/STATUS arrives
            self.in_flight += 1
            self._update_flow_control()
            return True

        except Exception as e:
            print(f"  ❌ Send error: {e}")
            self.connected = False
            return False

    async def receiver_loop(self):
        """Background loop receiving messages from the ESP32."""
        while self.connected and self.ws:
            try:
                # Wait for incoming message with timeout
                message = await asyncio.wait_for(self.ws.recv(), timeout=1.0)
                self.msg_count += 1
                self._handle_message(message)

            except asyncio.TimeoutError:
                continue  # Normal idle timeout

            except websockets.exceptions.ConnectionClosed as e:
                print(f"\n  ⚠️ Connection closed: {e}")
                self.connected = False
                break

            except Exception as e:
                print(f"\n  ⚠️ Receive error: {e}")
                break

    def _handle_message(self, raw: str):
        """Process a JSON message received from the ESP32."""
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            print(f"\n  ⚠️ Non‑JSON message: {str(raw)[:80]}")
            return

        msg_type = data.get("type", "UNKNOWN")

        # Update handled counter and reset watchdog on progress
        if "handled" in data:
            new_handled = data["handled"]
            if new_handled != self.last_handled:
                self.last_handled = new_handled
                self._last_progress_time = time.monotonic()

        # --- ACK message ---
        if msg_type == "ACK":
            self.ack_count += 1
            self.cmd_free = data.get("cmdFree", self.cmd_free)
            self.in_flight = max(0, self.in_flight - 1)
            self._update_flow_control()

        # --- BUFFER message (initial sync) ---
        elif msg_type == "BUFFER":
            if self.last_handled == 0:
                self.handled_offset = data.get("handled", 0)
                self.last_handled = self.handled_offset
                self._last_progress_time = time.monotonic()

            self.cmd_free = data.get("cmdFree", self.cmd_free)
            self.mot_free = data.get("motFree", self.mot_free)
            self.in_flight = 0  # Full resync
            self._update_flow_control()

        # --- STATUS message ---
        elif msg_type == "STATUS":
            self.last_status = data

            # STATUS is authoritative for flow control
            self.cmd_free = data.get("cmdFree", self.cmd_free)
            self.mot_free = data.get("motFree", self.mot_free)

            # Correct in_flight if overestimated
            max_possible = 30 - self.cmd_free
            if self.in_flight > max_possible:
                self.in_flight = max(0, max_possible)

            self._update_flow_control()

            # Live progress display during streaming
            if self.streaming:
                moving = "🔄" if data.get("isMoving") else "✅"
                pos = f"({data.get('x', 0):.1f}, {data.get('y', 0):.1f})"
                angles = f"θ({data.get('theta1', 0):.1f}, {data.get('theta2', 0):.1f})"
                progress = f"[{self.stream_sent}/{self.stream_total}]"
                eff_free = self.cmd_free - self.in_flight
                print(f"\r  {moving} {progress} {pos} {angles} cmdFree={self.cmd_free} in_flight={self.in_flight} eff={eff_free}    ", end="", flush=True)

        # --- ERROR message ---
        elif msg_type == "ERROR":
            msg = data.get("msg", "unknown")
            print(f"\n  ❌ ESP32 ERROR: {msg}")

            if msg == "Buffer Full":
                print("\n  ❌ FATAL: Flow control failure — ESP32 dropped a command!")
                self.cmd_free = 0
                self.in_flight = 0
                self.streaming = False

            self._update_flow_control()

        else:
            print(f"\n  📩 {msg_type}: {json.dumps(data)}")

    def _update_flow_control(self):
        """Update flow_event depending on available command slots."""
        effective_free = self.cmd_free - self.in_flight
        if effective_free >= STREAM_MIN_CMDFREE:
            self.flow_event.set()
        else:
            self.flow_event.clear()

    # ----------------------------------------------------------------------
    # High‑level robot commands
    # ----------------------------------------------------------------------

    async def send_home(self):
        """Send HOME command."""
        print("HOME")
        return await self.send_json({"type": "HOME"})

    async def send_move(self, x: float, y: float, z: float = 0.0,
                        speed: float = DEFAULT_SPEED, tool: bool = False):
        """Send MOVE_TO command."""
        cmd = {
            "type": "MOVE_TO",
            "x": x,
            "y": y,
            "z": z,
            "speed": speed,
            "tool": tool,
        }
        print(f"MOVE_TO ({x:.2f}, {y:.2f}) z={z:.1f} speed={speed:.0f} tool={'ON' if tool else 'OFF'}")
        return await self.send_json(cmd)

    async def send_tool(self, state: bool, z: float = 0.0):
        """Send TOOL command."""
        cmd = {"type": "TOOL", "state": state, "z": z}
        print(f"TOOL {'ON' if state else 'OFF'} z={z:.1f}")
        return await self.send_json(cmd)

    async def send_stop(self):
        """Send emergency STOP command."""
        print("STOP (emergency stop)")
        self.streaming = False
        return await self.send_json({"type": "STOP"})

    async def send_set_speed(self, speed: float):
        """Send SET_SPEED command."""
        print(f"SET_SPEED {speed:.1f} mm/s")
        return await self.send_json({"type": "SET_SPEED", "speed": speed})

    async def stream_commands(self, commands: list):
        """
        Stream a list of commands with robust flow control.

        Flow control rules:
        - cmd_free from ESP32 is authoritative
        - in_flight is optimistic and corrected by ACK/STATUS
        - send up to min(batch, cmd_free - in_flight)
        - watchdog aborts if no progress for too long
        """
        # Reset session counters
        self.in_flight = 0
        self.streaming = True
        self.stream_total = len(commands)
        self.stream_sent = 0
        self.stream_done.clear()
        self._last_progress_time = time.monotonic()

        print(f"\n  🚀 Starting streaming: {len(commands)} commands")
        print(f"     Flow control: batch={FLOW_CONTROL_BATCH}, min_free={STREAM_MIN_CMDFREE}")
        print(f"     Current cmdFree: {self.cmd_free}\n")

        try:
            i = 0
            while i < len(commands) and self.streaming:

                if not self.connected:
                    print("\n  ❌ Connection lost during streaming!")
                    return False

                effective_free = self.cmd_free - self.in_flight

                # Not enough space → wait
                if effective_free < STREAM_MIN_CMDFREE:
                    elapsed = time.monotonic() - self._last_progress_time

                    # Watchdog timeout
                    if elapsed > STREAM_WATCHDOG_TIMEOUT:
                        print(f"\n  ❌ WATCHDOG: No progress for {elapsed:.1f}s")
                        return False

                    print(f"\n  ⚠️ Waiting for free slots... cmdFree={self.cmd_free}, in_flight={self.in_flight}")

                    # Wait for flow_event or timeout
                    try:
                        self.flow_event.clear()
                        await asyncio.wait_for(self.flow_event.wait(), timeout=0.5)
                    except asyncio.TimeoutError:
                        pass

                    await asyncio.sleep(0)
                    continue

                # Progress detected → reset watchdog
                self._last_progress_time = time.monotonic()

                # Determine how many commands to send
                can_send = min(FLOW_CONTROL_BATCH, len(commands) - i, effective_free)
                if can_send <= 0:
                    await asyncio.sleep(0)
                    continue

                # Send batch
                for _ in range(can_send):
                    if i >= len(commands):
                        break
                    ok = await self.send_json(commands[i])
                    if not ok:
                        print(f"\n  ❌ Failed at command {i}/{len(commands)}")
                        return False
                    i += 1
                    self.stream_sent = i

                self._update_flow_control()
                await asyncio.sleep(0)

            print(f"\n\n  ✅ Streaming complete: {self.stream_sent}/{self.stream_total} commands sent")
            return True

        except Exception as e:
            print(f"\n  ❌ Streaming error: {e}")
            import traceback
            traceback.print_exc()
            return False

        finally:
            self.streaming = False
            self.stream_done.set()

    # ----------------------------------------------------------------------
    # Utility for PyQt integration
    # ----------------------------------------------------------------------
    def get_loop(self):
        """Return the running asyncio loop."""
        return asyncio.get_running_loop()


# ============================================================================
# PyQt Worker Thread for Robot Communication
# ============================================================================
class RobotWorker(QThread):
    connected_signal = pyqtSignal(bool)
    status_received_signal = pyqtSignal(dict)
    error_signal = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.client = RobotWSClient()
        self.loop = None
        self._ip_to_connect = None
        self._disconnect_requested = False
        self._stop_requested = False

    def run(self):
        """Start the asyncio event loop inside the QThread."""
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)

        # Run main async task
        self.loop.run_until_complete(self._main_task())
        self.loop.close()

    async def _main_task(self):
        """Main supervision loop: manages connection and forwards STATUS updates."""
        while not self._stop_requested:

            # Handle disconnect request
            if self._disconnect_requested:
                if self.client.connected:
                    await self.client.disconnect()
                self._disconnect_requested = False

            # Handle connection request
            if self._ip_to_connect:
                ip = self._ip_to_connect
                self._ip_to_connect = None

                success = await self.client.connect(ip)
                self.connected_signal.emit(success)

                if success:
                    # Start receiver loop in background
                    asyncio.create_task(self.client.receiver_loop())

            # Emit last STATUS periodically
            if self.client.connected:
                if hasattr(self.client, 'last_status') and self.client.last_status:
                    self.status_received_signal.emit(self.client.last_status)

            await asyncio.sleep(0.05)

        # Cleanup on exit
        if self.client.connected:
            await self.client.disconnect()

    def connect_robot(self, ip):
        """Request connection (thread‑safe)."""
        self._ip_to_connect = ip if len(ip) > 1 else DEFAULT_IP

    def disconnect_robot(self):
        """Request disconnection (thread‑safe)."""
        self._disconnect_requested = True

    def stop(self):
        """Stop the worker thread cleanly."""
        self._stop_requested = True

    def trigger_emergency_stop(self):
        """Send emergency STOP (thread‑safe)."""
        if self.loop and self.client.connected:
            self.client.streaming = False
            asyncio.run_coroutine_threadsafe(self.client.send_stop(), self.loop)

    def send_cmd(self, cmd_dict):
        """Send a command to the robot (thread‑safe)."""
        if self.loop and self.client.connected:
            asyncio.run_coroutine_threadsafe(self.client.send_json(cmd_dict), self.loop)

    def stream_commands(self, commands):
        """Start DXF command streaming (thread‑safe)."""
        if self.loop and self.client.connected:
            asyncio.run_coroutine_threadsafe(self.client.stream_commands(commands), self.loop)


# ============================================================================
# DXF Loading Helper
# ============================================================================
def load_dxf_commands(filename: str) -> list:
    """Load a DXF file and generate robot commands."""
    try:
        from dxf_parser import DxfParser
    except ImportError:
        print("  ❌ Cannot import dxf_parser.py")
        print("     Make sure you are in the correct folder.")
        return []

    try:
        parser = DxfParser(filename)
        commands = parser.parse()
        stats = parser.get_stats(commands)
        print(f"\n  📂 DXF loaded: {filename}")
        return commands

    except Exception as e:
        print(f"  ❌ DXF parsing error: {e}")
        return []
