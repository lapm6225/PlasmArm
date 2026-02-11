import ezdxf
import sys
import math

def arc_to_lines(center, radius, start_angle, end_angle, segments=32):
    """Approximation d'un arc en segments de lignes."""
    lines = []
    start = math.radians(start_angle)
    end = math.radians(end_angle)

    # Gestion des arcs qui passent par 0°
    if end < start:
        end += 2 * math.pi

    step = (end - start) / segments

    for i in range(segments):
        a1 = start + i * step
        a2 = start + (i + 1) * step
        p1 = (center[0] + radius * math.cos(a1),
              center[1] + radius * math.sin(a1))
        p2 = (center[0] + radius * math.cos(a2),
              center[1] + radius * math.sin(a2))
        lines.append((p1, p2))

    return lines


def circle_to_lines(center, radius, segments=64):
    """Approximation d'un cercle en segments de lignes."""
    return arc_to_lines(center, radius, 0, 360, segments)


def print_all_as_lines(filename):
    try:
        doc = ezdxf.readfile(filename)
    except Exception as e:
        print(f"Erreur : {e}")
        return

    msp = doc.modelspace()
    print(f"\n--- Conversion du fichier : {filename} ---\n")

    for e in msp:
        t = e.dxftype()

        # --- LINE ---
        if t == "LINE":
            p1 = e.dxf.start
            p2 = e.dxf.end
            print(f"LINE : {p1} -> {p2}")

        # --- LWPOLYLINE ---
        elif t == "LWPOLYLINE":
            pts = e.get_points()
            for i in range(len(pts) - 1):
                print(f"LINE (from LWPOLYLINE) : {pts[i]} -> {pts[i+1]}")
            if e.closed:
                print(f"LINE (closing) : {pts[-1]} -> {pts[0]}")

        # --- POLYLINE + VERTEX ---
        elif t == "POLYLINE":
            verts = [v.dxf.location for v in e.vertices]
            for i in range(len(verts) - 1):
                print(f"LINE (from POLYLINE) : {verts[i]} -> {verts[i+1]}")
            if e.is_closed:
                print(f"LINE (closing) : {verts[-1]} -> {verts[0]}")

        # --- ARC ---
        elif t == "ARC":
            lines = arc_to_lines(e.dxf.center, e.dxf.radius,
                                 e.dxf.start_angle, e.dxf.end_angle)
            for p1, p2 in lines:
                print(f"LINE (from ARC) : {p1} -> {p2}")

        # --- CIRCLE ---
        elif t == "CIRCLE":
            lines = circle_to_lines(e.dxf.center, e.dxf.radius)
            for p1, p2 in lines:
                print(f"LINE (from CIRCLE) : {p1} -> {p2}")

        # --- SPLINE ---
        elif t == "SPLINE":
            try:
                tool = e.construction_tool()
                points = list(tool.approximate(200))
            except Exception:
                pts = e.fit_points or e.control_points
                points = [(p[0], p[1]) for p in pts]
            for i in range(len(points) - 1):
                print(f"LINE (from SPLINE) : {points[i]} -> {points[i+1]}")

    print("\n--- Fin de la conversion ---\n")


if __name__ == "__main__":
    if len("rectangle.dxf") < 2:
        print("Usage : python script.py fichier.dxf")
    else:
        print_all_as_lines("rectangle.dxf")
        #print_all_as_lines("export_robot.dxf")