from PyQt6 import QtWidgets, uic
from PyQt6.QtWidgets import QGraphicsScene, QGraphicsRectItem, QGraphicsLineItem, QGraphicsItem
from PyQt6.QtCore import QObject, pyqtProperty, QPropertyAnimation, QRectF, Qt
from PyQt6.QtGui import QPen
import ezdxf
import os
from pathlib import Path

x = 0

def func_print():
    global x
    print("Starting print")
    if x < 100:
        x += 10
    else:
        x = 0
    window.progressBar.setValue(x)

def open_folder():
    desktop = Path.home() / "Desktop"
    os.startfile(desktop)

# --- Fonctions angles ---
shoulder_angle = 0
elbow_angle = 0

def shoulder_clockwise():
    global shoulder_angle
    shoulder_angle += 5
    animator.setAngle(shoulder_angle)

def shoulder_counterclockwise():
    global shoulder_angle
    shoulder_angle -= 5
    animator.setAngle(shoulder_angle)

def elbow_clockwise():
    global elbow_angle
    elbow_angle += 5
    animator_elbow.setAngle(elbow_angle)

def elbow_counterclockwise():
    global elbow_angle
    elbow_angle -= 5
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
    uic.loadUi('testhmi.ui', window)
    

    window.progressBar.setValue(0)
    window.Button_Print.clicked.connect(open_folder)

    window.Bras1_Horaire.clicked.connect(shoulder_clockwise)
    window.Bras1_Antihoraire.clicked.connect(shoulder_counterclockwise)

    window.Elbow_clockwise.clicked.connect(elbow_clockwise)
    window.Elbow_counterclockwise.clicked.connect(elbow_counterclockwise)

    # --- Scène ---
    scene = QGraphicsScene()
    window.graphicsView.setScene(scene)
    window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
    scene.setSceneRect(0, 0, 2000, 2000)

    # --- Charger un DXF ---
    dxf_group = load_dxf_into_scene(scene, window.graphicsView, "plan.dxf")
    

    # --- Bras : épaule ---
    shoulder = QGraphicsRectItem(QRectF(0, 0, 200, 20))
    scene.addItem(shoulder)
    shoulder.setTransformOriginPoint(0, 10)
    shoulder.setPos(350, 450)

    window.graphicsView.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)


    # --- Bras : coude ---
    elbow = QGraphicsRectItem(QRectF(0, 0, 200, 20))
    elbow.setParentItem(shoulder)
    elbow.setTransformOriginPoint(0, 10)
    elbow.setPos(190, 0)

    # --- Animateurs ---
    animator = AngleAnimator(shoulder)
    animator_elbow = AngleAnimator(elbow)

    window.show()
    sys.exit(app.exec())





# from PyQt6 import QtWidgets, uic
# from PyQt6.QtWidgets import QGraphicsScene, QGraphicsRectItem
# from PyQt6.QtCore import QObject, pyqtProperty, QPropertyAnimation, QRectF
# import ezdxf
# import os
# from pathlib import Path

# x = 0
# # --- Fonction de test ---
# def func_print():
#     global x
#     print("Starting print")
#     if x < 100:
#         x += 10
#     else:
#         x = 0
#     window.progressBar.setValue(x)

# def open_folder():
#     desktop = Path.home() / "Desktop"
#     os.startfile(desktop)

# # --- Fonction angle épaule ---
# shoulder_angle = 0

# def shoulder_clockwise():
#     global shoulder_angle
#     shoulder_angle += 5
#     animator.setAngle(shoulder_angle)

# def shoulder_counterclockwise():
#     global shoulder_angle
#     shoulder_angle -= 5
#     animator.setAngle(shoulder_angle)

# # --- Fonction angle coude ---
# elbow_angle = 0

# def elbow_clockwise():
#     global elbow_angle
#     elbow_angle += 5
#     animator_elbow.setAngle(elbow_angle)

# def elbow_counterclockwise():
#     global elbow_angle
#     elbow_angle -= 5
#     animator_elbow.setAngle(elbow_angle)



# # ---------------------------------------------------------
# #  WRAPPER POUR ANIMER UN QGraphicsItem EN PyQt6
# # ---------------------------------------------------------
# class AngleAnimator(QObject):
#     def __init__(self, graphics_item):
#         super().__init__()
#         self.item = graphics_item
#         self._angle = 0

#     def getAngle(self):
#         return self._angle

#     def setAngle(self, value):
#         self._angle = value
#         self.item.setRotation(value)

#     angle = pyqtProperty(float, getAngle, setAngle)


# # ---------------------------------------------------------
# #  PROGRAMME PRINCIPAL
# # ---------------------------------------------------------
# if __name__ == '__main__':
#     import sys
    
#     app = QtWidgets.QApplication(sys.argv)

#     window = QtWidgets.QMainWindow()
#     uic.loadUi('testhmi.ui', window)

#     window.progressBar.setValue(0)
#     window.Button_Print.clicked.connect(open_folder)

#     window.Bras1_Horaire.clicked.connect(shoulder_clockwise)
#     window.Bras1_Antihoraire.clicked.connect(shoulder_counterclockwise)

#     window.Elbow_clockwise.clicked.connect(elbow_clockwise)
#     window.Elbow_counterclockwise.clicked.connect(elbow_counterclockwise)

#     # --- Création de la scène graphique ---
#     scene = QGraphicsScene()
#     window.graphicsView.setScene(scene)
#     scene.setSceneRect(0, 0, 700, 500)


#     # --- Création du épaule ---
#     shoulder = QGraphicsRectItem(QRectF(0, 0, 200, 20))
#     scene.addItem(shoulder)
#     shoulder.setTransformOriginPoint(0, 10)
#     shoulder.setPos(350,450)

#     # --- Création du coupe ---
#     elbow = QGraphicsRectItem(QRectF(0, 0, 200, 20))
#     elbow.setParentItem(shoulder)
#     elbow.setTransformOriginPoint(0, 10)
#     elbow.setPos(190,0)
    

#     # --- Objet animateur ---
#     animator = AngleAnimator(shoulder)
#     animator_elbow = AngleAnimator(elbow)

#     # --- Animation ---
#     anim = QPropertyAnimation(animator, b"angle")
#     # anim.setStartValue(-90)
#     # anim.setEndValue(0)
#     # anim.setDuration(1500)
#     # anim.setLoopCount(2)
#     # anim.start()

#     window.show()
#     sys.exit(app.exec())