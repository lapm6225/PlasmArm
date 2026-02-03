import sys
import ezdxf

try:
    doc = ezdxf.readfile("drawing.dxf")
except IOError:
    print(f"Not a DXF file or a generic I/O error.")
    sys.exit(1)
except ezdxf.DXFStructureError:
    print(f"Invalid or corrupted DXF file.")
    sys.exit(2)


msp = doc.modelspace()


# iterate over all entities in modelspace
msp = doc.modelspace()
for e in msp:
    print(e.dxftype())
    x_start=e.dxf.start[0]
    y_start=e.dxf.start[1]
    x_end=e.dxf.end[0]
    y_end=e.dxf.end[1]
    print("x: ",x_start," y: ",y_start)
    print("x: ",x_end," y: ",y_end)


# e.dxf.start or e.dxf.end # type ezdxf.acc.vector.Vec3


# e.dxf.start[0] # type float