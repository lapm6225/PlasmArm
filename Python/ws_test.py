"""
ws_test.py -- Test interactif de communication WebSocket avec l'ESP32 SCARA Robot

Usage:
    python ws_test.py [ip_esp32]

Commandes disponibles:
    connect <ip>      Se connecter au WebSocket de l'ESP32
    home              Envoyer commande HOME
    move x,y          Déplacer vers (x, y)
    move x,y,z        Déplacer vers (x, y, z)
    tool on/off       Contrôle de l'outil
    stop              Arrêt d'urgence
    speed <val>       Changer la vitesse (mm/s)
    load <file.dxf>   Charger un DXF et envoyer avec flow control
    status            Afficher le dernier status reçu
    info              Afficher l'état du flow control
    quit              Quitter

Dépendances:
    pip install websockets ezdxf
"""

import asyncio
import json
import sys
import threading
import time
from collections import deque
from typing import Optional, List, Dict, Any

try:
    import websockets
    import websockets.client
except ImportError:
    print("ERREUR: 'websockets' n'est pas installé.")
    print("  pip install websockets")
    sys.exit(1)

# ============================================================================
# Configuration
# ============================================================================
DEFAULT_IP = "192.168.4.1"
WS_PORT = 80
WS_PATH = "/ws"
DEFAULT_SPEED = 50.0
FLOW_CONTROL_BATCH = 5        # Nombre de commandes à envoyer par batch
RECONNECT_DELAY = 3.0         # Secondes entre tentatives de reconnexion
STREAM_WATCHDOG_TIMEOUT = 15.0  # Secondes sans progrès avant abandon
STREAM_MIN_CMDFREE = 2          # Slots libres minimum avant d'envoyer


# ============================================================================
# Robot Client WebSocket
# ============================================================================
class RobotWSClient:
    """Client WebSocket asynchrone pour communiquer avec l'ESP32."""

    def __init__(self):
        self.ws: Optional[websockets.client.WebSocketClientProtocol] = None
        self.connected: bool = False
        self.ip: str = DEFAULT_IP

        # Flow control — cmd_free is always the authoritative ESP32 value
        self.cmd_free = 30       # Slots libres rapportés par l'ESP32
        self.mot_free = 1000     # Slots motion queue

        # 'handled' counter synchronisation
        self.handled_offset = 0  # Valeur de handled au démarrage de la session
        self.last_handled = 0    # Dernier handled reçu de l'ESP32

        # in_flight: commandes envoyées depuis le dernier ACK/STATUS reçu
        # Incrémenté à l'envoi, remis à 0 à chaque ACK/STATUS avec handled
        self.in_flight = 0

        # flow_event: signalé quand cmd_free > STREAM_MIN_CMDFREE
        self.flow_event = asyncio.Event()
        self.flow_event.set()

        # Dernier status reçu
        self.last_status = {}
        self.msg_count = 0
        self.ack_count = 0

        # Streaming DXF
        self.streaming = False
        self.stream_total = 0
        self.stream_sent = 0
        self.stream_done = asyncio.Event()
        self._last_progress_time = 0.0  # Pour le watchdog

    async def connect(self, ip: str):
        """Se connecter au WebSocket de l'ESP32."""
        self.ip = ip
        uri = f"ws://{ip}:{WS_PORT}{WS_PATH}"
        print(f"  Connexion à {uri}...")
        try:
            self.ws = await websockets.connect(
                uri,
                # Ping désactivé: ESPAsyncWebServer ne gère pas bien les pings
                # sous charge. Les STATUS messages servent de heartbeat.
                ping_interval=None,
                close_timeout=5,
                max_size=2**20,  # 1 MB pour les gros messages
            )
            self.connected = True
            # Réinitialiser les compteurs à chaque connexion
            self.in_flight = 0
            self.cmd_free = 30
            print(f"  ✅ Connecté à {uri}")
            return True
        except Exception as e:
            print(f"  ❌ Échec de connexion: {e}")
            self.connected = False
            return False

    async def disconnect(self):
        """Fermer la connexion WebSocket."""
        if self.ws:
            await self.ws.close()
            self.ws = None
        self.connected = False
        print("  Déconnecté.")

    async def send_json(self, data: dict):
        """Envoyer un message JSON sur le WebSocket."""
        if not self.connected or not self.ws:
            print("  ❌ Non connecté! Utilisez 'connect <ip>'")
            return False
        try:
            msg = json.dumps(data)
            await self.ws.send(msg)
            self.in_flight += 1  # Optimistic: decremented when ACK/STATUS arrives
            self._update_flow_control()
            return True
        except Exception as e:
            print(f"  ❌ Erreur d'envoi: {e}")
            self.connected = False
            return False

    async def receiver_loop(self):
        """Boucle de réception des messages WebSocket (tourne en arrière-plan)."""
        while self.connected and self.ws:
            try:
                message = await asyncio.wait_for(self.ws.recv(), timeout=1.0)
                self.msg_count += 1
                self._handle_message(message)
            except asyncio.TimeoutError:
                continue
            except websockets.exceptions.ConnectionClosed as e:
                print(f"\n  ⚠️  Connexion fermée: {e}")
                self.connected = False
                break
            except Exception as e:
                print(f"\n  ⚠️  Erreur de réception: {e}")
                break

    def _handle_message(self, raw: str):
        """Traiter un message JSON reçu de l'ESP32."""
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            raw_str = str(raw)
            print(f"\n  ⚠️  Message non-JSON: {raw_str[:80]}")
            return

        msg_type = data.get("type", "UNKNOWN")

        # Synchronisation du compteur 'handled' — toujours resynchroniser in_flight
        # depuis le ground truth de l'ESP32 quand disponible
        if "handled" in data:
            new_handled = data["handled"]
            if new_handled != self.last_handled:
                self.last_handled = new_handled
                # Progrès! Réinitialiser le watchdog
                self._last_progress_time = time.monotonic()

        if msg_type == "ACK":
            self.ack_count += 1
            # L'ESP32 vient d'accepter une commande: cmd_free est maintenant
            # authoritative. Remettre in_flight à 0 car l'ESP32 a confirmé.
            self.cmd_free = data.get("cmdFree", self.cmd_free)
            self.in_flight = max(0, self.in_flight - 1)
            self._update_flow_control()

        elif msg_type == "BUFFER":
            # Message initial à la connexion: calibrer l'offset
            if self.last_handled == 0:
                self.handled_offset = data.get("handled", 0)
                self.last_handled = self.handled_offset
                self._last_progress_time = time.monotonic()
            self.cmd_free = data.get("cmdFree", self.cmd_free)
            self.mot_free = data.get("motFree", self.mot_free)
            self.in_flight = 0  # Resync complet au BUFFER
            self._update_flow_control()

        elif msg_type == "STATUS":
            self.last_status = data
            # STATUS est authoritative: utiliser cmd_free de l'ESP32 directement
            # et limiter in_flight à ce qui est raisonnable
            self.cmd_free = data.get("cmdFree", self.cmd_free)
            self.mot_free = data.get("motFree", self.mot_free)
            # Corriger in_flight si l'ESP32 dit qu'il a plus de place
            # qu'on ne pensait (i.e., on a sur-compté in_flight)
            max_possible_in_flight = 30 - self.cmd_free
            if self.in_flight > max_possible_in_flight:
                self.in_flight = max(0, max_possible_in_flight)
            self._update_flow_control()
            # Afficher le status pendant le streaming
            if self.streaming:
                moving = "🔄" if data.get("isMoving") else "✅"
                pos = f"({data.get('x', 0):.1f}, {data.get('y', 0):.1f})"
                angles = f"θ({data.get('theta1', 0):.1f}, {data.get('theta2', 0):.1f})"
                progress = f"[{self.stream_sent}/{self.stream_total}]"
                eff_free = self.cmd_free - self.in_flight
                print(f"\r  {moving} {progress} {pos} {angles} cmdFree={self.cmd_free} in_flight={self.in_flight} eff={eff_free}    ", end="", flush=True)

        elif msg_type == "ERROR":
            msg = data.get("msg", "unknown")
            print(f"\n  ❌ ESP32 ERROR: {msg}")
            if msg == "Buffer Full":
                print("\n  ❌ FATAL: Le mécanisme de flow control a failli — commande ignorée par l'ESP32!")
                self.cmd_free = 0
                self.in_flight = 0  # Reset pour éviter le deadlock
                self.streaming = False
            self._update_flow_control()

        else:
            print(f"\n  📩 {msg_type}: {json.dumps(data)}")

    def _update_flow_control(self):
        """Signale flow_event quand l'ESP32 a assez de place pour recevoir."""
        effective_free = self.cmd_free - self.in_flight
        if effective_free >= STREAM_MIN_CMDFREE:
            self.flow_event.set()
        else:
            self.flow_event.clear()

    # ────────────────────────────────────────────────────────────────────
    # Commandes de haut niveau
    # ────────────────────────────────────────────────────────────────────

    async def send_home(self):
        """Envoyer la commande HOME."""
        print("  📤 HOME")
        return await self.send_json({"type": "HOME"})

    async def send_move(self, x: float, y: float, z: float = 0.0,
                        speed: float = DEFAULT_SPEED, tool: bool = False):
        """Envoyer une commande MOVE_TO."""
        cmd = {
            "type": "MOVE_TO",
            "x": x,
            "y": y,
            "z": z,
            "speed": speed,
            "tool": tool,
        }
        print(f"  📤 MOVE_TO ({x:.2f}, {y:.2f}) z={z:.1f} speed={speed:.0f} tool={'ON' if tool else 'OFF'}")
        return await self.send_json(cmd)

    async def send_tool(self, state: bool, z: float = 0.0):
        """Envoyer une commande TOOL."""
        cmd = {"type": "TOOL", "state": state, "z": z}
        label = "ON" if state else "OFF"
        print(f"  📤 TOOL {label} z={z:.1f}")
        return await self.send_json(cmd)

    async def send_stop(self):
        """Envoyer la commande STOP."""
        print("  📤 STOP (arrêt d'urgence)")
        return await self.send_json({"type": "STOP"})

    async def send_set_speed(self, speed: float):
        """Envoyer la commande SET_SPEED."""
        print(f"  📤 SET_SPEED {speed:.1f} mm/s")
        return await self.send_json({"type": "SET_SPEED", "speed": speed})

    async def stream_commands(self, commands: list):
        """
        Envoyer une liste de commandes avec flow control robuste.

        Flow control:
        - cmd_free (ESP32) est la source de vérité
        - in_flight est l'optimistic counter local (decremented on ACK, capped by STATUS)
        - On envoie jusqu'à min(FLOW_CONTROL_BATCH, cmd_free - in_flight) commandes
        - Si bloqué plus de STREAM_WATCHDOG_TIMEOUT secondes → abandon
        """
        # Reset complet des compteurs de session pour ce stream
        self.in_flight = 0
        self.streaming = True
        self.stream_total = len(commands)
        self.stream_sent = 0
        self.stream_done.clear()
        self._last_progress_time = time.monotonic()

        print(f"\n  🚀 Début du streaming: {len(commands)} commandes")
        print(f"     Flow control: batch={FLOW_CONTROL_BATCH}, min_free={STREAM_MIN_CMDFREE}")
        print(f"     cmdFree actuel: {self.cmd_free}")
        print()

        try:
            i = 0
            while i < len(commands):
                if not self.connected:
                    print("\n  ❌  Connexion perdue pendant l'envoi !")
                    return False

                effective_free = self.cmd_free - self.in_flight

                if effective_free < STREAM_MIN_CMDFREE:
                    # Vérifier le watchdog AVANT d'attendre
                    elapsed = time.monotonic() - self._last_progress_time
                    if elapsed > STREAM_WATCHDOG_TIMEOUT:
                        print(f"\n  ❌ WATCHDOG: Pas de progrès depuis {elapsed:.1f}s"
                              f" (cmdFree={self.cmd_free}, in_flight={self.in_flight},"
                              f" handled={self.last_handled})")
                        return False

                    print(f"\n  ⚠️  En attente de place... cmdFree={self.cmd_free},"
                          f" in_flight={self.in_flight}, handled={self.last_handled}")

                    # Attendre avec timeout court, puis yield pour l'event loop
                    # (permet aux pings et autres frames WebSocket d'être traités)
                    try:
                        self.flow_event.clear()
                        await asyncio.wait_for(self.flow_event.wait(), timeout=0.5)
                    except asyncio.TimeoutError:
                        pass
                    # Toujours yield pour ne pas bloquer l'event loop
                    await asyncio.sleep(0)
                    continue

                # Progrès possible: réinitialiser le watchdog
                self._last_progress_time = time.monotonic()

                # Calculer le batch: limité par la place disponible
                can_send = min(FLOW_CONTROL_BATCH, len(commands) - i, effective_free)
                if can_send <= 0:
                    await asyncio.sleep(0)  # yield
                    continue

                for _ in range(can_send):
                    if i >= len(commands):
                        break
                    ok = await self.send_json(commands[i])
                    if not ok:
                        print(f"\n  ❌ Échec à la commande {i}/{len(commands)}")
                        return False
                    i += 1
                    self.stream_sent = i

                self._update_flow_control()

                # Yield pour laisser receiver_loop traiter les ACKs entrants
                await asyncio.sleep(0)

            print(f"\n\n  ✅ Streaming terminé: {self.stream_sent}/{self.stream_total} commandes envoyées")
            return True

        except Exception as e:
            print(f"\n  ❌ Erreur pendant le streaming: {e}")
            import traceback
            traceback.print_exc()
            return False
        finally:
            self.streaming = False
            self.stream_done.set()

    def print_status(self):
        """Afficher le dernier status reçu."""
        if not self.last_status:
            print("  Aucun status reçu encore.")
            return

        s = self.last_status
        print(f"\n  ╔══════════════════════════════════════╗")
        print(f"  ║         STATUS ESP32                 ║")
        print(f"  ╠══════════════════════════════════════╣")
        print(f"  ║  Position: ({s.get('x', 0):8.2f}, {s.get('y', 0):8.2f})   ║")
        print(f"  ║  Z:        {s.get('z', 0):8.2f}                ║")
        print(f"  ║  Angles:   θ1={s.get('theta1', 0):7.2f}° θ2={s.get('theta2', 0):7.2f}° ║")
        print(f"  ║  Tool:     {'ON ' if s.get('tool') else 'OFF'}                   ║")
        print(f"  ║  Moving:   {'YES' if s.get('isMoving') else 'NO '}                   ║")
        print(f"  ║  Homed:    {'YES' if s.get('isHomed') else 'NO '}                   ║")
        print(f"  ╠══════════════════════════════════════╣")
        print(f"  ║  cmdFree:  {s.get('cmdFree', '?'):>4}  motFree: {s.get('motFree', '?'):>5}  ║")
        print(f"  ║  Messages: {self.msg_count:>4}  ACKs:    {self.ack_count:>5}  ║")
        print(f"  ╚══════════════════════════════════════╝")

    def print_info(self):
        """Afficher l'état du flow control."""
        eff = self.cmd_free - self.in_flight
        print(f"\n  Flow Control:")
        print(f"    cmdFree (ESP32): {self.cmd_free}")
        print(f"    in_flight:       {self.in_flight}")
        print(f"    effective_free:  {eff}")
        print(f"    motFree (ESP32): {self.mot_free}")
        print(f"    last_handled:    {self.last_handled}")
        print(f"    Batch d'envoi:   {FLOW_CONTROL_BATCH}")
        print(f"    Bloqué:          {'NON' if self.flow_event.is_set() else 'OUI'}")
        print(f"    Streaming:       {'OUI' if self.streaming else 'NON'}")
        if self.streaming:
            print(f"    Progression: {self.stream_sent}/{self.stream_total}")
            stall = time.monotonic() - self._last_progress_time
            print(f"    Dernière prog:   {stall:.1f}s ago")
        print(f"    Messages reçus:  {self.msg_count}")
        print(f"    ACKs reçus:      {self.ack_count}")


# ============================================================================
# Chargement DXF
# ============================================================================
def load_dxf_commands(filename: str) -> list:
    """Charger un fichier DXF et générer les commandes robot."""
    try:
        from dxf_parser import DxfParser
    except ImportError:
        print("  ❌ Impossible d'importer dxf_parser.py")
        print("     Assurez-vous d'être dans le dossier Python/")
        return []

    try:
        parser = DxfParser(filename)
        commands = parser.parse()
        stats = parser.get_stats(commands)
        print(f"\n  📂 DXF chargé: {filename}")
        print(f"     Commandes:  {stats['total']}")
        print(f"     MOVE_TO:    {stats['move_to']}")
        print(f"     TOOL:       {stats['tool']}")
        print(f"     Entités:    {stats['entities']}")
        bbox = stats['bbox']
        print(f"     BBox:       X[{bbox['x_min']:.1f}, {bbox['x_max']:.1f}] "
              f"Y[{bbox['y_min']:.1f}, {bbox['y_max']:.1f}]")

        # Aperçu
        parser.print_preview(commands, max_lines=10)
        return commands
    except FileNotFoundError:
        print(f"  ❌ Fichier non trouvé: {filename}")
        return []
    except Exception as e:
        print(f"  ❌ Erreur de parsing DXF: {e}")
        return []


# ============================================================================
# Input console non-bloquant
# ============================================================================
class AsyncInput:
    """Lecture non-bloquante de l'input console via un thread."""

    def __init__(self, loop: asyncio.AbstractEventLoop):
        self.loop = loop
        self.queue = asyncio.Queue()
        self.running = True
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        while self.running:
            try:
                line = input()
                self.loop.call_soon_threadsafe(self.queue.put_nowait, line)
            except EOFError:
                break
            except Exception:
                break

    async def get(self) -> str:
        return await self.queue.get()

    def stop(self):
        self.running = False


# ============================================================================
# Boucle principale
# ============================================================================
def print_help():
    print("""
  ╔══════════════════════════════════════════════════════╗
  ║        COMMANDES DISPONIBLES                         ║
  ╠══════════════════════════════════════════════════════╣
  ║  connect <ip>    Se connecter à l'ESP32              ║
  ║  home            Aller à la position home            ║
  ║  move x,y        Déplacer vers (x, y)                ║
  ║  move x,y,z      Déplacer vers (x, y, z)             ║
  ║  tool on/off     Contrôle de l'outil                 ║
  ║  stop            Arrêt d'urgence                     ║
  ║  speed <val>     Changer la vitesse (mm/s)           ║
  ║  load <file>     Charger et envoyer un DXF           ║
  ║  status          Afficher dernier status              ║
  ║  info            État du flow control                 ║
  ║  help            Cette aide                          ║
  ║  quit / exit     Quitter                             ║
  ╚══════════════════════════════════════════════════════╝
""")


async def main():
    print()
    print("  ╔══════════════════════════════════════════════════════╗")
    print("  ║   PlasmArm - Test WebSocket Communication           ║")
    print("  ║   ESP32 SCARA Robot Controller                      ║")
    print("  ╚══════════════════════════════════════════════════════╝")
    print()

    client = RobotWSClient()
    receiver_task: Optional[asyncio.Task] = None
    loop = asyncio.get_event_loop()
    ainput = AsyncInput(loop)

    # Connexion automatique si IP passée en argument
    if len(sys.argv) > 1:
        ip = sys.argv[1]
        if await client.connect(ip):
            receiver_task = asyncio.create_task(client.receiver_loop())

    print_help()

    current_speed = DEFAULT_SPEED

    try:
        while True:
            # Afficher le prompt
            prompt = f"  [{'🟢' if client.connected else '🔴'}] > "
            print(prompt, end="", flush=True)

            # Attendre l'input
            raw = await ainput.get()
            raw = raw.strip()
            if not raw:
                continue

            parts = raw.split()
            cmd = parts[0].lower()

            # ─── connect ─────────────────────────────────────────
            if cmd == "connect":
                ip = parts[1] if len(parts) > 1 else DEFAULT_IP
                if client.connected:
                    await client.disconnect()
                    if receiver_task:
                        receiver_task.cancel()
                        try:
                            await receiver_task
                        except asyncio.CancelledError:
                            pass
                if await client.connect(ip):
                    receiver_task = asyncio.create_task(client.receiver_loop())

            # ─── disconnect ──────────────────────────────────────
            elif cmd in ("disconnect", "disc"):
                if receiver_task:
                    receiver_task.cancel()
                    try:
                        await receiver_task
                    except asyncio.CancelledError:
                        pass
                    receiver_task = None
                await client.disconnect()

            # ─── home ────────────────────────────────────────────
            elif cmd == "home":
                await client.send_home()

            # ─── move ────────────────────────────────────────────
            elif cmd == "move":
                if len(parts) < 2:
                    print("  Usage: move x,y  ou  move x,y,z")
                    continue
                try:
                    coords = parts[1].split(",")
                    x = float(coords[0])
                    y = float(coords[1])
                    z = float(coords[2]) if len(coords) > 2 else 0.0
                    await client.send_move(x, y, z, speed=current_speed)
                except (ValueError, IndexError):
                    print("  ❌ Format invalide. Ex: move 200,150")

            # ─── tool ────────────────────────────────────────────
            elif cmd == "tool":
                if len(parts) < 2:
                    print("  Usage: tool on/off")
                    continue
                state = parts[1].lower() in ("on", "1", "true", "oui")
                z = float(parts[2]) if len(parts) > 2 else 0.0
                await client.send_tool(state, z)

            # ─── stop ────────────────────────────────────────────
            elif cmd == "stop":
                await client.send_stop()

            # ─── speed ───────────────────────────────────────────
            elif cmd == "speed":
                if len(parts) < 2:
                    print(f"  Vitesse actuelle: {current_speed:.1f} mm/s")
                    continue
                try:
                    current_speed = float(parts[1])
                    await client.send_set_speed(current_speed)
                except ValueError:
                    print("  ❌ Valeur invalide. Ex: speed 80")

            # ─── load DXF ────────────────────────────────────────
            elif cmd == "load":
                if len(parts) < 2:
                    print("  Usage: load <fichier.dxf>")
                    continue
                filename = " ".join(parts[1:])  # Support espaces dans le nom
                commands = load_dxf_commands(filename)
                if commands:
                    print(f"\n  Envoyer {len(commands)} commandes? (oui/non)")
                    confirm = await ainput.get()
                    if confirm.strip().lower() in ("oui", "o", "yes", "y"):
                        await client.stream_commands(commands)
                    else:
                        print("  Annulé.")

            # ─── status ──────────────────────────────────────────
            elif cmd == "status":
                client.print_status()

            # ─── info ────────────────────────────────────────────
            elif cmd == "info":
                client.print_info()

            # ─── help ────────────────────────────────────────────
            elif cmd in ("help", "h", "?"):
                print_help()

            # ─── quit ────────────────────────────────────────────
            elif cmd in ("quit", "exit", "q"):
                print("  Au revoir! 👋")
                break

            # ─── inconnu ─────────────────────────────────────────
            else:
                print(f"  ❌ Commande inconnue: '{cmd}'. Tapez 'help'.")

    except KeyboardInterrupt:
        print("\n  Interruption clavier.")
    finally:
        ainput.stop()
        if receiver_task:
            receiver_task.cancel()
            try:
                await receiver_task
            except asyncio.CancelledError:
                pass
        if client.connected:
            await client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
