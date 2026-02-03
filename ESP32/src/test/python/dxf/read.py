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

x_end=0
y_end=0

# iterate over all entities in modelspace
msp = doc.modelspace()
for e in msp:
    print(e.dxftype())
    x_start=e.dxf.start[0]
    y_start=e.dxf.start[1]
    tool_down=True
    if x_start!=x_end or y_start!=y_end:
        #print("changing line")
        #envoye une commande monter outil
        print(f"envoye une commande monter outil")
        #envoyer une commande move_to (x_start,y_start)
        print(f"envoyer une commande move_to ({x_start},{y_start})")
        #envoyer une commande descendre outil
        print(f"envoyer une commande descendre outil")

    x_end=e.dxf.end[0]
    y_end=e.dxf.end[1]

    #envoyer commande move_to (x_end,y_end)
    print(f"envoyer commande move_to ({x_end},{y_end})")



# e.dxf.start or e.dxf.end # type ezdxf.acc.vector.Vec3

# e.dxf.start[0] # type float