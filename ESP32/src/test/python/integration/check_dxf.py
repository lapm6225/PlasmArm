"""Quick analysis of available DXF files."""
from dxf_parser import DxfParser
import os

files = ["arc299.5.dxf", "arc299ypos.dxf", "onshape_simple.dxf", "grosse_spline.dxf"]

for fname in files:
    if not os.path.exists(fname):
        print(f"{fname}: NOT FOUND")
        continue
    try:
        parser = DxfParser(fname)
        cmds = parser.parse()
        moves = [c for c in cmds if c["type"] == "MOVE_TO"]
        xs = [c["x"] for c in moves]
        ys = [c["y"] for c in moves]
        print(f"\n{fname}: {len(cmds)} commands, {len(moves)} moves")
        print(f"  Entities: {parser.entities}")
        print(f"  X range: [{min(xs):.1f}, {max(xs):.1f}]")
        print(f"  Y range: [{min(ys):.1f}, {max(ys):.1f}]")
        
        # Check workspace validity
        bad = 0
        for c in moves:
            r = (c["x"]**2 + c["y"]**2)**0.5
            if c["y"] < 0 or r > 300 or r < 50:
                bad += 1
        if bad > 0:
            print(f"  WARNING: {bad}/{len(moves)} points OUTSIDE workspace (y<0 or r>300 or r<50)")
        else:
            print(f"  OK: All points within workspace")
        
        # Show first few
        for i, c in enumerate(cmds[:6]):
            if c["type"] == "MOVE_TO":
                r = (c["x"]**2 + c["y"]**2)**0.5
                print(f"    [{i}] MOVE ({c['x']:.1f}, {c['y']:.1f}) r={r:.1f} tool={c.get('tool', False)}")
            else:
                st = c.get("state", "")
                print(f"    [{i}] {c['type']} state={st}")
        if len(cmds) > 6:
            print(f"    ... and {len(cmds)-6} more")
    except Exception as e:
        print(f"{fname}: ERROR: {e}")
