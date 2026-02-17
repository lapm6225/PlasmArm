"""
dxf_parser.py — DXF file parser for SCARA robot commands.

Converts DXF entities (LINEs, POLYLINEs, ARCs, CIRCLEs) into a sequence
of robot commands compatible with the ESP32 WebSocket protocol.

Supported entities:
    - LINE: direct start→end segments
    - LWPOLYLINE / POLYLINE: multi-vertex paths
    - ARC: circular arcs (approximated as line segments)
    - CIRCLE: full circles (approximated as line segments)

Output format:
    List of dicts ready for send_command():
    [
        {"type": "TOOL", "state": False, "z": 5.0},          # lift tool
        {"type": "MOVE_TO", "x": 10.0, "y": 20.0, "z": 5.0, "tool": False},  # rapid travel
        {"type": "TOOL", "state": True, "z": 0.0},           # lower tool
        {"type": "MOVE_TO", "x": 50.0, "y": 20.0, "z": 0.0, "tool": True},   # cut
        ...
    ]
"""

import math
import ezdxf
from typing import List, Tuple

# ANSI colors
class C:
    GREEN  = '\033[92m'
    YELLOW = '\033[93m'
    RED    = '\033[91m'
    DIM    = '\033[2m'
    BOLD   = '\033[1m'
    END    = '\033[0m'


# Default parameters
TRAVEL_Z = 5.0     # Z height for rapid travel (tool raised)
CUT_Z = 0.0        # Z height for cutting (tool lowered)
CUT_SPEED = 30.0   # Speed during cutting (mm/s)
TRAVEL_SPEED = 80.0  # Speed during rapid travel (mm/s)
ARC_SEGMENTS = 20   # Number of line segments to approximate an arc
GAP_TOLERANCE = 0.5 # mm — if next shape starts within this distance, skip travel move


def parse_dxf(filename: str, 
              travel_z: float = TRAVEL_Z,
              cut_z: float = CUT_Z,
              cut_speed: float = CUT_SPEED,
              travel_speed: float = TRAVEL_SPEED,
              arc_segments: int = ARC_SEGMENTS,
              gap_tolerance: float = GAP_TOLERANCE,
              offset_x: float = 0.0,
              offset_y: float = 0.0,
              scale: float = 1.0,
              verbose: bool = True) -> List[dict]:
    """
    Parse a DXF file and return a list of robot commands.
    
    Args:
        filename: Path to .dxf file
        travel_z: Z height during rapid travel (tool up)
        cut_z: Z height during cutting (tool down)
        cut_speed: Speed for cutting moves (mm/s)
        travel_speed: Speed for rapid travel (mm/s)
        arc_segments: Number of segments to approximate arcs/circles
        gap_tolerance: Max distance to skip travel move between shapes
        offset_x/y: Offset all coordinates
        scale: Scale factor for all coordinates
        verbose: Print parsing details
    
    Returns:
        List of command dicts ready for RobotClient.send_command()
    """
    if verbose:
        print(f"\n{C.BOLD}Parsing DXF: {filename}{C.END}")
    
    doc = ezdxf.readfile(filename)
    msp = doc.modelspace()
    
    # Collect all segments as lists of (x, y) points
    # Each segment is a "shape" — a continuous cutting path
    shapes: List[List[Tuple[float, float]]] = []
    
    entity_counts = {}
    
    for entity in msp:
        etype = entity.dxftype()
        entity_counts[etype] = entity_counts.get(etype, 0) + 1
        
        if etype == "LINE":
            x1 = entity.dxf.start[0] * scale + offset_x
            y1 = entity.dxf.start[1] * scale + offset_y
            x2 = entity.dxf.end[0] * scale + offset_x
            y2 = entity.dxf.end[1] * scale + offset_y
            shapes.append([(x1, y1), (x2, y2)])
        
        elif etype in ("LWPOLYLINE", "POLYLINE"):
            points = []
            with entity.points("xy") as pts:
                for pt in pts:
                    px = pt[0] * scale + offset_x
                    py = pt[1] * scale + offset_y
                    points.append((px, py))
            if entity.closed and len(points) > 1:
                points.append(points[0])  # Close the polyline
            if len(points) >= 2:
                shapes.append(points)
        
        elif etype == "ARC":
            cx = entity.dxf.center[0] * scale + offset_x
            cy = entity.dxf.center[1] * scale + offset_y
            r = entity.dxf.radius * scale
            start_angle = math.radians(entity.dxf.start_angle)
            end_angle = math.radians(entity.dxf.end_angle)
            
            # Handle wrap-around
            if end_angle <= start_angle:
                end_angle += 2 * math.pi
            
            points = []
            for i in range(arc_segments + 1):
                t = i / arc_segments
                angle = start_angle + t * (end_angle - start_angle)
                px = cx + r * math.cos(angle)
                py = cy + r * math.sin(angle)
                points.append((px, py))
            shapes.append(points)
        
        elif etype == "CIRCLE":
            cx = entity.dxf.center[0] * scale + offset_x
            cy = entity.dxf.center[1] * scale + offset_y
            r = entity.dxf.radius * scale
            
            points = []
            for i in range(arc_segments + 1):
                angle = 2 * math.pi * i / arc_segments
                px = cx + r * math.cos(angle)
                py = cy + r * math.sin(angle)
                points.append((px, py))
            shapes.append(points)
        
        elif etype == "SPLINE":
            # Approximate spline using flattening
            try:
                points = [(pt[0] * scale + offset_x, pt[1] * scale + offset_y) 
                          for pt in entity.flattening(0.5)]
                if len(points) >= 2:
                    shapes.append(points)
            except Exception:
                if verbose:
                    print(f"  {C.YELLOW}WARNING: Skipping SPLINE (flattening failed){C.END}")

    if verbose:
        print(f"  Entities found: {entity_counts}")
        print(f"  Shapes extracted: {len(shapes)}")
        total_points = sum(len(s) for s in shapes)
        print(f"  Total vertices: {total_points}")
    
    # Convert shapes into robot commands
    commands: List[dict] = []
    current_x, current_y = 0.0, 0.0  # Track current position
    tool_is_on = False
    
    for i, shape in enumerate(shapes):
        if len(shape) < 2:
            continue
        
        start_x, start_y = shape[0]
        
        # Check if we need to travel to the start of this shape
        dist = math.sqrt((start_x - current_x)**2 + (start_y - current_y)**2)
        
        if dist > gap_tolerance:
            # Need to travel: lift tool → move → lower tool
            if tool_is_on:
                commands.append({"type": "TOOL", "state": False, "z": travel_z})
                tool_is_on = False
            
            commands.append({
                "type": "MOVE_TO",
                "x": round(start_x, 3),
                "y": round(start_y, 3),
                "z": travel_z,
                "speed": travel_speed,
                "tool": False
            })
        
        # Lower tool for cutting
        if not tool_is_on:
            commands.append({"type": "TOOL", "state": True, "z": cut_z})
            tool_is_on = True
        
        # Cut along the shape
        for px, py in shape[1:]:
            commands.append({
                "type": "MOVE_TO",
                "x": round(px, 3),
                "y": round(py, 3),
                "z": cut_z,
                "speed": cut_speed,
                "tool": True
            })
            current_x, current_y = px, py
    
    # End: lift tool
    if tool_is_on:
        commands.append({"type": "TOOL", "state": False, "z": travel_z})
    
    if verbose:
        move_count = sum(1 for c in commands if c["type"] == "MOVE_TO")
        tool_count = sum(1 for c in commands if c["type"] == "TOOL")
        print(f"  Commands generated: {len(commands)} total")
        print(f"    MOVE_TO: {move_count}")
        print(f"    TOOL:    {tool_count}")
        
        # Bounding box
        all_x = [c["x"] for c in commands if "x" in c]
        all_y = [c["y"] for c in commands if "y" in c]
        if all_x and all_y:
            print(f"  Bounding box: X=[{min(all_x):.1f}, {max(all_x):.1f}] "
                  f"Y=[{min(all_y):.1f}, {max(all_y):.1f}]")
    
    return commands


def preview_commands(commands: List[dict], max_lines: int = 30):
    """Print a preview of the command sequence."""
    print(f"\n{C.BOLD}Command Preview ({len(commands)} total):{C.END}")
    print("-" * 70)
    
    for i, cmd in enumerate(commands):
        if i >= max_lines:
            print(f"  ... and {len(commands) - max_lines} more commands")
            break
        
        t = cmd["type"]
        if t == "MOVE_TO":
            tool_str = "[CUT]" if cmd.get("tool") else "[TRAVEL]"
            print(f"  {i:4d} {tool_str:9s} MOVE ({cmd['x']:8.2f}, {cmd['y']:8.2f}) "
                  f"z={cmd.get('z', 0):.1f} spd={cmd.get('speed', 0):.0f}")
        elif t == "TOOL":
            state = "ON " if cmd["state"] else "OFF"
            print(f"  {i:4d} TOOL {state} z={cmd.get('z', 0):.1f}")
        else:
            print(f"  {i:4d} {t}: {cmd}")
    
    print("-" * 70)


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python dxf_parser.py <file.dxf> [scale] [offset_x] [offset_y]")
        sys.exit(1)
    
    filename = sys.argv[1]
    scale = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
    offset_x = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
    offset_y = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
    
    commands = parse_dxf(filename, scale=scale, offset_x=offset_x, offset_y=offset_y)
    preview_commands(commands)
