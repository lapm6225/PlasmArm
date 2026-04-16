# dxf_parser.py -- Parse DXF files and generate robot commands.
#
# Reads a DXF file, extracts geometry (arcs, lines, circles),
# and generates a sequence of MOVE_TO and TOOL commands for the robot.
#
# Usage:
#     from dxf_parser import DxfParser
#     parser = DxfParser("drawing.dxf")
#     commands = parser.parse()

import math
import ezdxf
from typing import List, Dict, Tuple

# Default parameters for robot motion and geometry approximation
DEFAULT_CUT_SPEED = 30.0      # mm/s during cutting
DEFAULT_TRAVEL_SPEED = 80.0   # mm/s during travel (tool off)
DEFAULT_Z_TRAVEL = 5.0        # Z height during travel
DEFAULT_Z_CUT = 0.0           # Z height during cutting
DEFAULT_ARC_SEGMENTS = 20     # Number of line segments used to approximate arcs


class DxfParser:
    """Parse DXF files and generate robot commands."""

    def __init__(self, filename: str,
                 cut_speed: float = DEFAULT_CUT_SPEED,
                 travel_speed: float = DEFAULT_TRAVEL_SPEED,
                 z_travel: float = DEFAULT_Z_TRAVEL,
                 z_cut: float = DEFAULT_Z_CUT,
                 arc_segments: int = DEFAULT_ARC_SEGMENTS):
        # Store configuration and DXF filename
        self.filename = filename
        self.cut_speed = cut_speed
        self.travel_speed = travel_speed
        self.z_travel = z_travel
        self.z_cut = z_cut
        self.arc_segments = arc_segments

        # Dictionary counting DXF entity types encountered
        self.entities: Dict[str, int] = {}

    def parse(self) -> List[dict]:
        """
        Parse the DXF file and return a list of robot commands.
        Loads the DXF, extracts geometry, and converts it into MOVE_TO/TOOL commands.
        """
        doc = ezdxf.readfile(self.filename)
        msp = doc.modelspace()
        
        # Convert DXF entities into lists of (x, y) points
        shapes = self._extract_shapes(msp)
        
        # Convert shapes into robot commands
        commands = self._generate_commands(shapes)
        
        return commands

    def _extract_shapes(self, msp) -> List[List[Tuple[float, float]]]:
        """
        Extract shapes as lists of (x, y) vertices from DXF entities.
        Supports LINE, ARC, CIRCLE, LWPOLYLINE, POLYLINE.
        """
        shapes = []
        self.entities = {}
        
        for entity in msp:
            etype = entity.dxftype()
            self.entities[etype] = self.entities.get(etype, 0) + 1
            
            # Simple line segment
            if etype == 'LINE':
                start = (entity.dxf.start.x, entity.dxf.start.y)
                end = (entity.dxf.end.x, entity.dxf.end.y)
                shapes.append([start, end])
            
            # Arc approximated as multiple line segments
            elif etype == 'ARC':
                center = (entity.dxf.center.x, entity.dxf.center.y)
                radius = entity.dxf.radius
                start_angle = math.radians(entity.dxf.start_angle)
                end_angle = math.radians(entity.dxf.end_angle)
                
                # Ensure correct angular direction
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
            
            # Circle approximated as arc_segments segments
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
            
            # Lightweight polyline
            elif etype == 'LWPOLYLINE':
                points = [(p[0], p[1]) for p in entity.get_points()]
                if entity.closed:
                    points.append(points[0])  # Close shape
                shapes.append(points)
            
            # Classic polyline
            elif etype == 'POLYLINE':
                points = [(v.dxf.location.x, v.dxf.location.y) for v in entity.vertices]
                if entity.is_closed:
                    points.append(points[0])
                shapes.append(points)
        
        return shapes

    def _generate_commands(self, shapes: List[List[Tuple[float, float]]]) -> List[dict]:
        """
        Convert extracted shapes into robot commands.
        Each shape becomes:
            - TOOL UP (if needed)
            - MOVE_TO start (travel)
            - TOOL DOWN + DELAY
            - MOVE_TO each vertex (cutting)
        """
        commands = []
        last_x = 0
        last_y = 0

        for shape in shapes:
            if len(shape) < 2:
                continue  # Ignore degenerate shapes

            start = shape[0]

            # If not already at the start point, lift tool
            if last_x != round(start[0], 2) or last_y != round(start[1], 2):
                commands.append({"type": "TOOL", "state": "UP"})

            # Travel move to start of shape
            commands.append({
                "type": "MOVE_TO",
                "x": round(start[0], 2),
                "y": round(start[1], 2),
                "speed": self.travel_speed,
            })
            
            # Tool down before cutting
            commands.append({"type": "TOOL", "state": "DOWN"})
            commands.append({"type": "DELAY", "ms": 200})

            # Cutting moves along the shape
            for point in shape[1:]:
                commands.append({
                    "type": "MOVE_TO",
                    "x": round(point[0], 2),
                    "y": round(point[1], 2),
                    "speed": self.cut_speed,
                })

            # Update last known position
            last_x = round(point[0], 2)
            last_y = round(point[1], 2)
        
        # Ensure tool is lifted at the end
        commands.append({"type": "TOOL", "state": "UP"})
        return commands

    def get_stats(self, commands: List[dict]) -> dict:
        """
        Compute statistics about the parsed commands:
        - number of MOVE_TO and TOOL commands
        - entity counts
        - number of shapes
        - bounding box of all MOVE_TO points
        """
        move_count = sum(1 for c in commands if c["type"] == "MOVE_TO")
        tool_count = sum(1 for c in commands if c["type"] == "TOOL")
        
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

    def print_preview(self, commands: List[dict], max_lines: int = 40):
        """
        Print a readable preview of the command list.
        Useful for debugging and verifying tool transitions.
        """
        print(f"\nCommand Preview ({len(commands)} total):")
        print("-" * 70)
        
        for i, cmd in enumerate(commands):
            if i >= max_lines and i < len(commands) - 1:
                print(f"  ... and {len(commands) - max_lines} more commands")
                break
            
            if cmd["type"] == "MOVE_TO":
                tool_on = cmd.get("tool", False)
                label = "[CUT]    " if tool_on else "[TRAVEL] "
                print(f"  {i:4d} {label} MOVE ({cmd['x']:8.2f}, {cmd['y']:8.2f})  spd={cmd['speed']:.0f}")
            elif cmd["type"] == "TOOL":
                state = "ON" if cmd["state"] else "OFF"
                print(f"  {i:4d} TOOL {state}")
            else:
                print(f"  {i:4d} {cmd['type']}")
        
        print("-" * 70)
