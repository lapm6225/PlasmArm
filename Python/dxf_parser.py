"""
dxf_parser.py -- Parse DXF files and generate robot commands.

Reads a DXF file, extracts geometry (arcs, lines, circles),
and generates a sequence of MOVE_TO and TOOL commands for the robot.
    
"""

import math
import json
import ezdxf
from typing import List, Dict, Tuple

# Default parameters
DEFAULT_CUT_SPEED = 30.0      # mm/s during cutting
DEFAULT_TRAVEL_SPEED = 80.0   # mm/s during travel (tool off)
DEFAULT_Z_TRAVEL = 5.0        # Z height during travel
DEFAULT_Z_CUT = 0.0           # Z height during cutting
DEFAULT_ARC_SEGMENTS = 20     # Number of line segments per arc

# Reachability constants: should match ESP32 Config.h
WORKSPACE_R_MIN = 50.0
WORKSPACE_R_MAX = 317.6  # typical ~300; keep conservative
THETA1_MIN = 0.0
THETA1_MAX = 180.0
THETA2_MIN = -150.0
THETA2_MAX = 150.0

ARM_LENGTH_1 = 220.05
ARM_LENGTH_2 = 217.65


def _clamp(v, minv, maxv):
    return max(minv, min(maxv, v))


class SCARAKinematics:
    """Simple SCARA kinematics model (like ESP32 core)."""

    def __init__(self, l1=ARM_LENGTH_1, l2=ARM_LENGTH_2):
        self.l1 = l1
        self.l2 = l2

    def inverse(self, x: float, y: float, preferred: str = None):
        """Return (theta1, theta2, config) or None if unreachable."""
        r = math.hypot(x, y)

        if r > (self.l1 + self.l2) or r < WORKSPACE_R_MIN:
            return None

        cos_theta2 = (r * r - self.l1 * self.l1 - self.l2 * self.l2) / (2.0 * self.l1 * self.l2)
        cos_theta2 = _clamp(cos_theta2, -1.0, 1.0)

        if preferred is None:
            preferred = "RIGHT_ELBOW" if x >= 0.0 else "LEFT_ELBOW"

        def solve(use_right):
            theta2_rad = -math.acos(cos_theta2) if use_right else math.acos(cos_theta2)
            theta2 = math.degrees(theta2_rad)

            alpha = math.degrees(math.atan2(y, x))
            sin_beta = _clamp((self.l2 * math.sin(theta2_rad)) / r if r != 0 else 0.0, -1.0, 1.0)
            beta = math.degrees(math.asin(sin_beta))
            theta1 = alpha - beta

            # normalize to 0..360, then clamp to 0..180 for this robot
            if theta1 < 0:
                theta1 += 360.0

            if theta1 < THETA1_MIN or theta1 > THETA1_MAX:
                return None
            if theta2 < THETA2_MIN or theta2 > THETA2_MAX:
                return None

            return theta1, theta2

        if preferred == "RIGHT_ELBOW":
            result = solve(True)
            if result:
                return result[0], result[1], "RIGHT_ELBOW"
            fallback = solve(False)
            if fallback:
                return fallback[0], fallback[1], "LEFT_ELBOW"
        else:
            result = solve(False)
            if result:
                return result[0], result[1], "LEFT_ELBOW"
            fallback = solve(True)
            if fallback:
                return fallback[0], fallback[1], "RIGHT_ELBOW"

        return None


class DxfParser:
    """Parse DXF files and generate robot commands."""

    def __init__(self, filename: str,
                 cut_speed: float = DEFAULT_CUT_SPEED,
                 travel_speed: float = DEFAULT_TRAVEL_SPEED,
                 z_travel: float = DEFAULT_Z_TRAVEL,
                 z_cut: float = DEFAULT_Z_CUT,
                 arc_segments: int = DEFAULT_ARC_SEGMENTS):
        self.filename = filename
        self.cut_speed = cut_speed
        self.travel_speed = travel_speed
        self.z_travel = z_travel
        self.z_cut = z_cut
        self.arc_segments = arc_segments
        self.entities: Dict[str, int] = {}

    def parse(self) -> List[dict]:
        """
        Parse the DXF file and return a list of robot commands with precomputed joint angles.

        Returns:
            List of command dicts ready to send via RobotClient.
        """
        doc = ezdxf.readfile(self.filename)
        msp = doc.modelspace()

        # Extract geometric shapes from DXF entities
        shapes = self._extract_shapes(msp)

        # Generate high-level commands from shapes
        commands = self._generate_commands(shapes)

        # Add inverse kinematics and reachability metadata
        annotated = self._annotate_kinematics(commands)

        # Split any switch points into before/after command entries
        annotated = DxfParser.expand_switch_segments(annotated)

        return annotated

    def _extract_shapes(self, msp) -> List[List[Tuple[float, float]]]:
        """Extract shapes as lists of (x, y) vertices from DXF entities."""
        shapes = []
        self.entities = {}
        
        for entity in msp:
            etype = entity.dxftype()
            self.entities[etype] = self.entities.get(etype, 0) + 1
            
            if etype == 'LINE':
                start = (entity.dxf.start.x, entity.dxf.start.y)
                end = (entity.dxf.end.x, entity.dxf.end.y)
                shapes.append([start, end])
            
            elif etype == 'ARC':
                center = (entity.dxf.center.x, entity.dxf.center.y)
                radius = entity.dxf.radius
                start_angle = math.radians(entity.dxf.start_angle)
                end_angle = math.radians(entity.dxf.end_angle)
                
                # Handle angle wrapping
                if end_angle <= start_angle:
                    end_angle += 2 * math.pi
                
                points = []
                for i in range(self.arc_segments + 1):
                    t = i / self.arc_segments
                    angle = start_angle + t * (end_angle - start_angle)
                    x = center[0] + radius * math.cos(angle)
                    y = center[1] + radius * math.sin(angle)
                    points.append((x, y))
                shapes.append(points)
            
            elif etype == 'CIRCLE':
                center = (entity.dxf.center.x, entity.dxf.center.y)
                radius = entity.dxf.radius
                points = []
                for i in range(self.arc_segments + 1):
                    angle = 2 * math.pi * i / self.arc_segments
                    x = center[0] + radius * math.cos(angle)
                    y = center[1] + radius * math.sin(angle)
                    points.append((x, y))
                shapes.append(points)
            
            elif etype == 'LWPOLYLINE':
                points = [(p[0], p[1]) for p in entity.get_points()]
                if entity.closed:
                    points.append(points[0])
                shapes.append(points)
            
            elif etype == 'POLYLINE':
                points = [(v.dxf.location.x, v.dxf.location.y) for v in entity.vertices]
                if entity.is_closed:
                    points.append(points[0])
                shapes.append(points)
        
        return shapes

    def _generate_commands(self, shapes: List[List[Tuple[float, float]]]) -> List[dict]:
        """Generate robot commands from extracted shapes."""
        commands = []
        
        for shape in shapes:
            if len(shape) < 2:
                continue
            
            # Travel to start of shape (tool off, Z up)
            start = shape[0]
            commands.append({
                "type": "MOVE_TO",
                "x": round(start[0], 2),
                "y": round(start[1], 2),
                "z": self.z_travel,
                "speed": self.travel_speed,
                "tool": False
            })
            
            # Tool on at start
            commands.append({
                "type": "TOOL",
                "state": True,
                "z": self.z_cut
            })
            
            # Cut through all remaining vertices
            for point in shape[1:]:
                commands.append({
                    "type": "MOVE_TO",
                    "x": round(point[0], 2),
                    "y": round(point[1], 2),
                    "z": self.z_cut,
                    "speed": self.cut_speed,
                    "tool": True
                })
            
            # Tool off at end
            commands.append({
                "type": "TOOL",
                "state": False,
                "z": self.z_travel
            })
        
        return commands

    def get_stats(self, commands: List[dict]) -> dict:
        """Get statistics about the parsed commands."""
        move_count = sum(1 for c in commands if c["type"] == "MOVE_TO")
        tool_count = sum(1 for c in commands if c["type"] == "TOOL")
        
        # Bounding box
        xs = [c["x"] for c in commands if c["type"] == "MOVE_TO"]
        ys = [c["y"] for c in commands if c["type"] == "MOVE_TO"]
        
        return {
            "total": len(commands),
            "move_to": move_count,
            "tool": tool_count,
            "entities": dict(self.entities),
            "shapes": len([s for s in self._extract_shapes(ezdxf.readfile(self.filename).modelspace()) if len(s) >= 2]),
            "bbox": {
                "x_min": min(xs) if xs else 0,
                "x_max": max(xs) if xs else 0,
                "y_min": min(ys) if ys else 0,
                "y_max": max(ys) if ys else 0,
            }
        }

    @staticmethod
    def _ik_solutions(x: float, y: float, L1: float = ARM_LENGTH_1, L2: float = ARM_LENGTH_2):
        """Return all valid IK solutions (theta1, theta2) and config for a point."""
        r = math.hypot(x, y)

        if y < 0.0 or r < WORKSPACE_R_MIN or r > WORKSPACE_R_MAX:
            return []

        cos_theta2 = (r * r - L1 * L1 - L2 * L2) / (2.0 * L1 * L2)
        if abs(cos_theta2) > 1.0:
            return []

        theta2a_rad = math.acos(max(-1.0, min(1.0, cos_theta2)))
        theta2b_rad = -theta2a_rad
        alpha = math.degrees(math.atan2(y, x))

        solutions = []
        for theta2_rad, cfg in [(theta2a_rad, "LEFT_ELBOW"), (theta2b_rad, "RIGHT_ELBOW")]:
            sin_beta = (L2 * math.sin(theta2_rad)) / r if r > 0 else 0.0
            sin_beta = max(-1.0, min(1.0, sin_beta))
            beta = math.degrees(math.asin(sin_beta))
            theta1 = alpha - beta
            theta2 = math.degrees(theta2_rad)

            # Normalize angles to [-180, 180] for comparison
            while theta1 <= -180.0:
                theta1 += 360.0
            while theta1 > 180.0:
                theta1 -= 360.0

            if THETA1_MIN <= theta1 <= THETA1_MAX and THETA2_MIN <= theta2 <= THETA2_MAX:
                solutions.append({"theta1": theta1, "theta2": theta2, "config": cfg})

        return solutions

    def _annotate_kinematics(self, commands: List[dict]) -> List[dict]:
        """Add joint-angle data and reachability info to commands."""
        state = {
            "x": 0.0,
            "y": 0.0,
            "z": self.z_travel,
            "tool": False,
            "config": "AUTO",
            "theta1": 0.0,
            "theta2": 0.0,
            "segment_id": 0,  # indice de phase de configuration
        }

        annotated = []

        for cmd in commands:
            cmd2 = dict(cmd)
            cmd2["has_joint_angles"] = False
            cmd2["ik_valid"] = True
            cmd2["arm_config"] = state["config"]
            cmd2["theta1"] = state["theta1"]
            cmd2["theta2"] = state["theta2"]
            cmd2["theta1_before"] = state["theta1"]
            cmd2["theta2_before"] = state["theta2"]
            cmd2["config_before"] = state["config"]
            cmd2["config_after"] = state["config"]

            if cmd["type"] == "MOVE_TO":
                x = cmd.get("x", state["x"])
                y = cmd.get("y", state["y"])
                z = cmd.get("z", state["z"])

                # Encourage elbow selection based on x sign
                preferred = "RIGHT_ELBOW" if x >= 0.0 else "LEFT_ELBOW"
                solutions = self._ik_solutions(x, y)

                if not solutions:
                    cmd2["ik_valid"] = False
                    cmd2["arm_config"] = "UNREACHABLE"
                    cmd2["has_joint_angles"] = False
                    cmd2["config_switch"] = False
                    cmd2["from_arm_config"] = state["config"]
                    cmd2["to_arm_config"] = "UNREACHABLE"
                else:
                    # Prefer current arm config if possible for smoother motion
                    preferred_current = state["config"] if state["config"] in ["RIGHT_ELBOW", "LEFT_ELBOW"] else preferred
                    candidate = next((s for s in solutions if s["config"] == preferred_current), None)
                    if candidate is None:
                        # fallback to the original preferred by x sign
                        candidate = next((s for s in solutions if s["config"] == preferred), solutions[0])

                    cmd2["theta1"] = candidate["theta1"]
                    cmd2["theta2"] = candidate["theta2"]
                    cmd2["arm_config"] = candidate["config"]
                    cmd2["has_joint_angles"] = True

                    # Provide pre/post angles for precise switch handling
                    cmd2["config_after"] = candidate["config"]

                    # Config switching marker
                    cmd2["config_before"] = state["config"]
                    cmd2["config_after"] = candidate["config"]
                    cmd2["config_switch"] = (state["config"] != candidate["config"])

                    if cmd2["config_switch"]:
                        # on démarre un nouveau segment d'élbow configuration
                        state["segment_id"] += 1

                    state.update({
                        "x": x,
                        "y": y,
                        "z": z,
                        "tool": cmd.get("tool", state["tool"]),
                        "config": candidate["config"],
                        "theta1": candidate["theta1"],
                        "theta2": candidate["theta2"],
                    })

                state["x"] = x
                state["y"] = y
                state["z"] = z

            elif cmd["type"] == "TOOL":
                # Keep last safe joint values for tool command
                state["tool"] = cmd.get("state", state["tool"])
                state["z"] = cmd.get("z", state["z"])

                cmd2["theta1"] = state["theta1"]
                cmd2["theta2"] = state["theta2"]
                cmd2["arm_config"] = state["config"]
                cmd2["has_joint_angles"] = True
                cmd2["config_switch"] = False
                cmd2["config_before"] = state["config"]
                cmd2["config_after"] = state["config"]
                cmd2["theta1_before"] = state["theta1"]
                cmd2["theta2_before"] = state["theta2"]

            # Attribuer le segment actuel (avant ou après switch) à la commande
            cmd2["segment_id"] = state["segment_id"]
            cmd2["is_switch_point"] = cmd2.get("config_switch", False)

            annotated.append(cmd2)

        return annotated

    @staticmethod
    def _command_minimal(cmd: dict) -> dict:
        """Normalized command output for JSON export and robot follow-up."""
        out = {}
        if cmd.get("type") == "MOVE_TO":
            out = {
                "type": "MOVE_TO",
                "x": cmd.get("x"),
                "y": cmd.get("y"),
                "z": cmd.get("z"),
                "speed": cmd.get("speed"),
                "tool": cmd.get("tool"),
                "arm_config": cmd.get("arm_config"),
                "theta1": cmd.get("theta1"),
                "theta2": cmd.get("theta2"),
                "config_before": cmd.get("config_before"),
                "config_after": cmd.get("config_after"),
                "config_switch": bool(cmd.get("config_switch", False)),
                "segment_id": cmd.get("segment_id"),
                "is_switch_point": bool(cmd.get("is_switch_point", False)),
            }
        elif cmd.get("type") == "TOOL":
            out = {
                "type": "TOOL",
                "state": cmd.get("state"),
                "z": cmd.get("z"),
                "arm_config": cmd.get("arm_config"),
                "theta1": cmd.get("theta1"),
                "theta2": cmd.get("theta2"),
                "segment_id": cmd.get("segment_id"),
            }
        else:
            out = dict(cmd)
        return out

    @staticmethod
    def expand_switch_segments(commands: List[dict]) -> List[dict]:
        """Return command list with switch points split into before/after segments."""
        expanded = []
        for cmd in commands:
            if cmd.get("type") == "MOVE_TO" and cmd.get("config_switch"):
                # If we switch from a generic AUTO state, no valid 'before' posture exists.
                if cmd.get("config_before") != "AUTO":
                    before = {
                        **cmd,
                        "arm_config": cmd.get("config_before"),
                        "theta1": cmd.get("theta1_before", cmd.get("theta1")),
                        "theta2": cmd.get("theta2_before", cmd.get("theta2")),
                        "config_switch": False,
                        "is_switch_point": False,
                        "segment_part": "before",
                    }
                    expanded.append(before)

                after = {
                    **cmd,
                    "arm_config": cmd.get("config_after"),
                    "theta1": cmd.get("theta1"),
                    "theta2": cmd.get("theta2"),
                    "config_switch": False,
                    "is_switch_point": True,
                    "segment_part": "after",
                }

                expanded.append(after)
            else:
                expanded.append(cmd)
        return expanded

    def save_commands_json(self, commands: List[dict], file_path: str, split_switch: bool = False):
        """Write commands as JSON file."""
        if split_switch:
            commands = DxfParser.expand_switch_segments(commands)

        commands = [DxfParser._command_minimal(c) for c in commands]

        with open(file_path, "w", encoding="utf-8") as f:
            json.dump(commands, f, indent=2, ensure_ascii=False)

    @staticmethod
    def is_reachable(x: float, y: float) -> bool:
        """Check whether point is reachable (workspace + joint limits)."""
        return len(DxfParser._ik_solutions(x, y)) > 0

    @staticmethod
    def annotate_reachability(commands: List[dict]) -> List[dict]:
        """Mark each MOVE_TO command with a 'reachable' bool."""
        annotated = []
        for cmd in commands:
            if cmd.get("type") == "MOVE_TO":
                reachable = DxfParser.is_reachable(cmd.get("x", 0.0), cmd.get("y", 0.0))
                new_cmd = dict(cmd)
                new_cmd["reachable"] = reachable
                annotated.append(new_cmd)
            else:
                annotated.append(dict(cmd))
        return annotated

    @staticmethod
    def save_commands_json(commands: List[dict], file_path: str, pretty: bool = True):
        """Save the command list as a JSON file (array of command objects)."""
        with open(file_path, "w", encoding="utf-8") as f:
            if pretty:
                json.dump(commands, f, indent=2, ensure_ascii=False)
            else:
                json.dump(commands, f, ensure_ascii=False)

    @staticmethod
    def save_commands_json_lines(commands: List[dict], file_path: str):
        """Save one JSON command per line (compatible avec command.txt style)."""
        with open(file_path, "w", encoding="utf-8") as f:
            for cmd in commands:
                f.write(json.dumps(cmd, ensure_ascii=False) + "\n")

    def print_preview(self, commands: List[dict], max_lines: int = 40):
        """Print a human-readable preview of the commands."""
        print(f"\nCommand Preview ({len(commands)} total):")
        print("-" * 70)
        
        for i, cmd in enumerate(commands):
            if i >= max_lines and i < len(commands) - 1:
                print(f"  ... and {len(commands) - max_lines} more commands")
                break
            
            if cmd["type"] == "MOVE_TO":
                tool_on = cmd.get("tool", False)
                label = "[CUT]    " if tool_on else "[TRAVEL] "
                print(f"  {i:4d} {label} MOVE ({cmd['x']:8.2f}, {cmd['y']:8.2f}) z={cmd['z']:.1f} spd={cmd['speed']:.0f}")
            elif cmd["type"] == "TOOL":
                state = "ON" if cmd["state"] else "OFF"
                print(f"  {i:4d} TOOL {state} z={cmd['z']:.1f}")
            else:
                print(f"  {i:4d} {cmd['type']}")
        
        print("-" * 70)


def commands_to_gcode(commands: list, file_path: str, comment: bool = True):
    """Sauvegarde la séquence de commandes en style G-code simple."""
    with open(file_path, "w", encoding="utf-8") as f:
        f.write("; G-code-like command file generated by dxf_parser.py\n")
        f.write("; Format: G0/G1 X.. Y.. Z.. F.. | M3/M5 tool ON/OFF\n")
        f.write("; MOVE_TO retains z, speed, tool ; TOOL controls tool on/off + z\n")

        for i, cmd in enumerate(commands):
            if comment:
                f.write(f"; {i:04d} {cmd}\n")

            if cmd["type"] == "TOOL":
                f.write("M3\n" if cmd.get("state", False) else "M5\n")
                z = cmd.get("z", 0.0)
                f.write(f"G0 Z{z:.3f}\n")
            elif cmd["type"] == "MOVE_TO":
                x = cmd.get("x", 0.0)
                y = cmd.get("y", 0.0)
                z = cmd.get("z", 0.0)
                speed = cmd.get("speed", 0.0)
                tool = cmd.get("tool", False)
                gcode = "G1" if tool else "G0"
                f.write(f"{gcode} X{x:.3f} Y{y:.3f} Z{z:.3f} F{speed:.1f}\n")
            else:
                # Autres types, traduction basique
                if cmd["type"] == "HOME":
                    f.write("G28 ; home\n")
                elif cmd["type"] == "STOP":
                    f.write("M112 ; emergency stop\n")
                elif cmd["type"] == "SET_SPEED":
                    f.write(f"F{cmd.get('speed', 0.0):.1f}\n")
                else:
                    f.write(f"; UNKNOWN command type: {cmd['type']}\n")


def gcode_to_commands(file_path: str) -> list:
    """Lit un fichier g-code-like et retourne une liste de commandes JSON pour ESP32."""
    import re
    commands = []

    with open(file_path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith(";"):
                continue

            # DOMMAGE: commandes en majuscules ou minuscules
            parts = line.split()
            cmd = parts[0].upper()
            params = {p[0].upper(): float(p[1:]) for p in parts[1:] if len(p) > 1 and p[0].upper() in "XYZF"}

            if cmd == "M3":
                commands.append({"type": "TOOL", "state": True, "z": None})
            elif cmd == "M5":
                commands.append({"type": "TOOL", "state": False, "z": None})
            elif cmd in ("G0", "G1"):
                move_cmd = {
                    "type": "MOVE_TO",
                    "x": params.get("X", 0.0),
                    "y": params.get("Y", 0.0),
                    "z": params.get("Z", 0.0),
                    "speed": params.get("F", 0.0),
                    "tool": (cmd == "G1")
                }
                commands.append(move_cmd)
            elif cmd == "G28":
                commands.append({"type": "HOME", "x": 0.0, "y": 0.0, "z": 0.0})
            elif cmd == "M112":
                commands.append({"type": "STOP"})
            elif cmd.startswith("F") and len(cmd) > 1:
                # Mise à jour de la vitesse globale
                try:
                    speed = float(cmd[1:])
                    commands.append({"type": "SET_SPEED", "speed": speed})
                except ValueError:
                    pass

    # Remplacer les TOOL z=None par l'état de z inchangé + séparer si nécessaire
    last_z = None
    out = []
    for c in commands:
        if c["type"] == "TOOL" and c.get("z") is None:
            if last_z is not None:
                c["z"] = last_z
            else:
                c["z"] = 0.0
        if c["type"] == "MOVE_TO":
            last_z = c.get("z", last_z)
        out.append(c)

    return out

