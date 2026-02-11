from PyQt6 import QtWidgets, uic
from PyQt6.QtWidgets import QGraphicsScene, QGraphicsRectItem, QGraphicsEllipseItem, QGraphicsLineItem, QGraphicsItem, QLineEdit, QFileDialog 
from PyQt6.QtCore import QObject, pyqtProperty, QPropertyAnimation, QRectF, Qt, QPointF
from PyQt6.QtGui import QPen 
import ezdxf
import os
from pathlib import Path

angular_speed = 1



# --- Qui print un DXF centré ---
def func_print():
    doc = ezdxf.new()
    msp = doc.modelspace()
    origin_bras = QPointF(400, 450)

    for item in window.dxf_group.childItems():
        if isinstance(item, QGraphicsLineItem):
            line = item.line()
            p1 = item.mapToScene(line.p1())
            p2 = item.mapToScene(line.p2())

            # Convertir dans référentiel bras
            p1_robot = p1 - origin_bras
            p2_robot = p2 - origin_bras

            msp.add_line((p1_robot.x(), -p1_robot.y()), (p2_robot.x(), -p2_robot.y()))

    doc.saveas("export_robot.dxf")




def open_file():
    filename, _ = QFileDialog.getOpenFileName(
        None,
        "Ouvrir un fichier DXF",
        "",
        "Fichiers DXF (*.dxf)"
    )

    if filename:
        print("Fichier choisi :", filename)
        window.dxf_group=load_dxf_into_scene(scene, window.graphicsView,filename)

def close_file():
    scene.removeItem(window.dxf_group)


def change_angular_speed():
    global angular_speed
    text = window.edit_angular_speed.text()
    angular_speed = float(text) if text else 1







# --- forcer float ---
def enforce_float_only(line_edit: QLineEdit):
    def clean(text):
        allowed = "0123456789.-"
        cleaned = "".join(c for c in text if c in allowed)

        # Only one dot
        if cleaned.count('.') > 1:
            parts = cleaned.split('.', 1)
            cleaned = parts[0] + '.' + parts[1].replace('.', '')

        # Only one minus, and only at the beginning
        if cleaned.count('-') > 1:
            cleaned = cleaned.replace('-', '', cleaned.count('-') - 1)
        if '-' in cleaned and cleaned.index('-') != 0:
            cleaned = cleaned.replace('-', '')

        if cleaned != text:
            line_edit.setText(cleaned)

    line_edit.textChanged.connect(clean)

# --- Fonctions boutons angles ---
shoulder_angle = 0
elbow_angle = 0

def shoulder_clockwise():
    global shoulder_angle
    global angular_speed
    shoulder_angle += angular_speed
    animator.setAngle(shoulder_angle)

def shoulder_counterclockwise():
    global shoulder_angle
    global angular_speed
    shoulder_angle -= angular_speed
    animator.setAngle(shoulder_angle)

def elbow_clockwise():
    global elbow_angle
    global angular_speed
    elbow_angle += angular_speed
    animator_elbow.setAngle(elbow_angle)

def elbow_counterclockwise():
    global elbow_angle
    global angular_speed
    elbow_angle -= angular_speed
    animator_elbow.setAngle(elbow_angle)
    

# --- DXF loader ---
def load_dxf_into_scene(scene, view, filename):
    doc = ezdxf.readfile(filename)
    msp = doc.modelspace()

    pen = QPen(Qt.GlobalColor.black)
    pen.setWidth(1)

    SCALE = 15
    count = 0

    # --- Groupe DXF déplaçable ---
    dxf_group = scene.createItemGroup([])

    for entity in msp:
        if entity.dxftype() == "SPLINE":

            # Reconstruction propre de la spline
            try:
                tool = entity.construction_tool()
                points = list(tool.approximate(200))
            except Exception:
                pts = entity.fit_points or entity.control_points
                points = [(p[0], p[1]) for p in pts]

            if len(points) < 2:
                continue

            # Création des segments
            for i in range(len(points) - 1):
                x1, y1 = points[i][0], points[i][1]
                x2, y2 = points[i+1][0], points[i+1][1]

                line = QGraphicsLineItem(x1*SCALE, -y1*SCALE, x2*SCALE, -y2*SCALE)
                line.setPen(pen)
                dxf_group.addToGroup(line)
                count += 1

    print("Segments drawn:", count)

    # --- Rendre le DXF déplaçable ---
    dxf_group.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, True)
    dxf_group.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable, True)

    # --- Centrage initial ---
    bbox = dxf_group.boundingRect()

    scene_center_x = 400
    scene_center_y = 250

    dxf_center_x = bbox.x() + bbox.width() / 2
    dxf_center_y = bbox.y() + bbox.height() / 2

    dx = scene_center_x - dxf_center_x
    dy = scene_center_y - dxf_center_y

    dxf_group.moveBy(dx, dy)

    return dxf_group



# --- Wrapper animation ---
class AngleAnimator(QObject):
    def __init__(self, graphics_item):
        super().__init__()
        self.item = graphics_item
        self._angle = 0

    def getAngle(self):
        return self._angle

    def setAngle(self, value):
        self._angle = value
        self.item.setRotation(value)

    angle = pyqtProperty(float, getAngle, setAngle)

# --- Programme principal ---
if __name__ == '__main__':
    import sys
    
    app = QtWidgets.QApplication(sys.argv)

    window = QtWidgets.QMainWindow()
    uic.loadUi('plasmarm.ui', window)
    

    window.progressBar.setValue(0)
    window.Button_Print.clicked.connect(func_print)

    window.Bras1_Horaire.clicked.connect(shoulder_clockwise)
    window.Bras1_Antihoraire.clicked.connect(shoulder_counterclockwise)

    window.Elbow_clockwise.clicked.connect(elbow_clockwise)
    window.Elbow_counterclockwise.clicked.connect(elbow_counterclockwise)

    window.actionLoad.triggered.connect(open_file)
    window.actionFermer.triggered.connect(close_file)

    window.edit_angular_speed.textChanged.connect(change_angular_speed)



    enforce_float_only(window.edit_angular_speed)
    

    # --- Scène ---
    scene = QGraphicsScene()
    window.graphicsView.setScene(scene)
    window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
    scene.setSceneRect(0, 0, 800, 500)

    # --- Charger un DXF ---
    #dxf_group = load_dxf_into_scene(scene, window.graphicsView, "plan.dxf")
    

    # --- Bras : épaule ---
    shoulder = QGraphicsRectItem(QRectF(0, 0, 150, 20))
    scene.addItem(shoulder)
    shoulder.setTransformOriginPoint(0, 10)
    shoulder.setPos(400, 450)

    window.graphicsView.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)


    # --- Bras : coude ---
    elbow = QGraphicsRectItem(QRectF(0, 0, 148, 20))
    elbow.setParentItem(shoulder)
    elbow.setTransformOriginPoint(0, 10)
    elbow.setPos(140, 0)

    # --- Animateurs ---
    animator = AngleAnimator(shoulder)
    animator_elbow = AngleAnimator(elbow)

    # --- afficher porté  ---
    arc_max = QGraphicsEllipseItem(112, 172, 576, 576)
    scene.addItem(arc_max)
    arc_max.setStartAngle(0)
    arc_max.setSpanAngle(180 * 16)
    arc_max.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_max.setZValue(1000)

    arc_min = QGraphicsEllipseItem(379, 439, 42, 42)
    scene.addItem(arc_min)
    arc_min.setStartAngle(0)
    arc_min.setSpanAngle(180 * 16)
    arc_min.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_min.setZValue(1000)



    window.show()
    sys.exit(app.exec())


