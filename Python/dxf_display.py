from PyQt6 import QtWidgets
from PyQt6.QtWidgets import QGraphicsLineItem, QGraphicsItem, QFileDialog
from PyQt6.QtCore import Qt, QPointF
from PyQt6.QtGui import QPen
import ezdxf
import os
from dxf_parser import DxfParser


# ============================================================
#  GLOBAL DARK THEME FOR ALL DIALOGS
# ============================================================
DIALOG_STYLE = """
    QWidget {
        background-color: #20242c;
        color: #e6f1ff;
    }
    QLineEdit {
        background-color: #262b34;
        color: #e6f1ff;
        border: 1px solid #2f3540;
        border-radius: 4px;
        padding: 4px 6px;
    }
    QPushButton {
        background-color: #262b34;
        color: #e6f1ff;
        border: 1px solid #2f3540;
        border-radius: 4px;
        padding: 4px 10px;
    }
    QPushButton:hover {
        background-color: #2b303a;
        border-color: #3a4150;
    }
"""
# (Applied globally in main.py → app.setStyleSheet(DIALOG_STYLE))


# ============================================================
#  DXF EXPORT — Generate a robot‑centered DXF + preview
# ============================================================
def func_DXF(window, scene):
    """Export the loaded DXF into robot coordinates, generate a preview,
    validate workspace limits, and save export_robot.dxf."""

    doc = ezdxf.new()
    msp = doc.modelspace()
    robot_origin = QPointF(550, 450)

    # Remove previous preview if it exists
    if hasattr(window, "dxf_preview"):
        scene.removeItem(window.dxf_preview)

    window.dxf_preview = scene.createItemGroup([])
    preview_pen = QPen(Qt.GlobalColor.green)
    preview_pen.setWidth(1)

    # Ensure a DXF is loaded
    if not hasattr(window, "dxf_group"):
        QtWidgets.QMessageBox.warning(window, "Error", "No DXF loaded.")
        return

    # Convert each segment to robot coordinates
    for item in window.dxf_group.childItems():
        if isinstance(item, QGraphicsLineItem):

            # Extract scene coordinates
            line = item.line()
            p1 = item.mapToScene(line.p1())
            p2 = item.mapToScene(line.p2())

            # Convert to robot frame (Y inverted)
            p1_robot = p1 - robot_origin
            p2_robot = p2 - robot_origin

            # Write to DXF
            msp.add_line(
                (p1_robot.x(), -p1_robot.y()),
                (p2_robot.x(), -p2_robot.y())
            )

            # Draw preview
            x1, y1 = p1_robot.x(), p1_robot.y()
            x2, y2 = p2_robot.x(), p2_robot.y()

            preview_line = QGraphicsLineItem(
                550 + x1, 450 + y1,
                550 + x2, 450 + y2
            )
            preview_line.setPen(preview_pen)
            preview_line.setZValue(-10)
            preview_line.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable, False)
            preview_line.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, False)

            window.dxf_preview.addToGroup(preview_line)

    # Save DXF
    doc.saveas("export_robot.dxf")

    # Validate workspace
    if not check():
        print("Out of Bounds")
        scene.removeItem(window.dxf_preview)
        del window.dxf_preview
    else:
        print("DXF file generated")
        scene.removeItem(window.dxf_group)


# ============================================================
#  OPEN DXF FILE
# ============================================================
def open_file(window, scene):
    """Open a DXF file and load it into the scene."""

    # If a DXF is already loaded → ask confirmation
    if hasattr(window, "dxf_group"):
        box = QtWidgets.QMessageBox(window)
        box.setWindowTitle("DXF already loaded")
        box.setText("A DXF file is already loaded. Do you want to replace it?")
        box.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Yes |
                               QtWidgets.QMessageBox.StandardButton.No)

        if box.exec() == QtWidgets.QMessageBox.StandardButton.No:
            return

        close_file(window, scene)

    # File dialog
    dialog = QFileDialog(window)
    dialog.setWindowTitle("Open DXF File")
    dialog.setNameFilter("DXF Files (*.dxf)")

    if dialog.exec():
        filename = dialog.selectedFiles()[0]
        print("Selected file:", filename)
        window.dxf_group = load_dxf_into_scene(window, scene, window.graphicsView, filename)


# ============================================================
#  CLOSE DXF FILE
# ============================================================
def close_file(window, scene):
    """Remove the DXF group and preview from the scene."""

    if not hasattr(window, "dxf_group"):
        QtWidgets.QMessageBox.warning(window, "Error", "No DXF to close.")
        return

    # Remove DXF group
    if window.dxf_group.scene() is not None:
        scene.removeItem(window.dxf_group)
    del window.dxf_group

    # Remove preview if present
    if hasattr(window, "dxf_preview"):
        scene.removeItem(window.dxf_preview)
        del window.dxf_preview

    print("File closed")


# ============================================================
#  LOAD DXF INTO SCENE
# ============================================================
def load_dxf_into_scene(window, scene, view, filename):
    """Load a DXF file, convert entities into QGraphicsLineItem,
    group them, and center the result in the scene."""

    doc = ezdxf.readfile(filename)
    msp = doc.modelspace()
    pen = QPen(Qt.GlobalColor.black)
    pen.setWidth(1)
    count = 0

    # Create movable DXF group
    window.dxf_group = scene.createItemGroup([])

    for entity in msp:

        # ---------------- SPLINES ----------------
        if entity.dxftype() == "SPLINE":
            try:
                tool = entity.construction_tool()
                points = list(tool.approximate(200))
            except Exception:
                pts = entity.fit_points or entity.control_points
                points = [(p[0], p[1]) for p in pts]

            if len(points) < 2:
                continue

            for i in range(len(points) - 1):
                x1, y1 = points[i]
                x2, y2 = points[i + 1]

                line = QGraphicsLineItem(x1, -y1, x2, -y2)
                line.setPen(pen)
                window.dxf_group.addToGroup(line)
                count += 1

        # ---------------- LINES ----------------
        if entity.dxftype() == "LINE":
            x1, y1, _ = entity.dxf.start
            x2, y2, _ = entity.dxf.end

            line = QGraphicsLineItem(x1, -y1, x2, -y2)
            line.setPen(pen)
            window.dxf_group.addToGroup(line)
            count += 1

        # ---------------- POLYLINES ----------------
        if entity.dxftype() == "LWPOLYLINE":
            pts = entity.get_points()

            # Successive segments
            for i in range(len(pts) - 1):
                x1, y1 = pts[i][0], pts[i][1]
                x2, y2 = pts[i + 1][0], pts[i + 1][1]

                line = QGraphicsLineItem(x1, -y1, x2, -y2)
                line.setPen(pen)
                window.dxf_group.addToGroup(line)
                count += 1

            # Closed polyline
            if entity.closed:
                x1, y1 = pts[-1][0], pts[-1][1]
                x2, y2 = pts[0][0], pts[0][1]

                line = QGraphicsLineItem(x1, -y1, x2, -y2)
                line.setPen(pen)
                window.dxf_group.addToGroup(line)
                count += 1

    print("Segments drawn:", count)

    # Make group movable/selectable
    window.dxf_group.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, True)
    window.dxf_group.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable, True)

    # ---------------- Centering ----------------
    bbox = window.dxf_group.boundingRect()

    scene_center_x = 550
    scene_center_y = 200

    dxf_center_x = bbox.x() + bbox.width() / 2
    dxf_center_y = bbox.y() + bbox.height() / 2

    dx = scene_center_x - dxf_center_x
    dy = scene_center_y - dxf_center_y

    window.dxf_group.moveBy(dx, dy)

    return window.dxf_group


# ============================================================
#  WORKSPACE CHECK
# ============================================================
files = ["export_robot.dxf"]

def check():
    """Validate that all MOVE_TO commands remain inside the robot workspace."""

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

            # Workspace validation
            bad = 0
            for c in moves:
                r = (c["x"]**2 + c["y"]**2)**0.5
                if c["y"] < -10 or r > 419 or r < 85:
                    bad += 1

            if bad > 0:
                print(f"  WARNING: {bad}/{len(moves)} points OUTSIDE workspace")
                return False

            print("  OK: All points within workspace")
            return True

        except Exception as e:
            print(f"{fname}: ERROR: {e}")
