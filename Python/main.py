from PyQt6 import QtWidgets, uic
from PyQt6.QtWidgets import QGraphicsScene, QGraphicsRectItem, QGraphicsEllipseItem, QGraphicsLineItem, QGraphicsItem, QLineEdit, QFileDialog 
from PyQt6.QtCore import QObject, pyqtProperty, QPropertyAnimation, QRectF, Qt, QPointF
from PyQt6.QtGui import QPen 
import ezdxf
import os
import sys
from pathlib import Path
import readdxf

# --- Variables Globales ---
angular_speed = 1
linear_speed = 1
shoulder_angle = 0
elbow_angle = 0
tool_pos_x = 0
tool_pos_y = 0

# --- Qui print un DXF centré ---
def func_print():
    doc = ezdxf.new()
    msp = doc.modelspace()
    origin_bras = QPointF(400, 450)
    if hasattr(window, "dxf_group"):
        for item in window.dxf_group.childItems():
            # --- test
            #print("TYPE:", type(item))   # ← ajoute ceci ici
            #
            if isinstance(item, QGraphicsLineItem):
                line = item.line()
                p1 = item.mapToScene(line.p1())
                p2 = item.mapToScene(line.p2())

                # Convertir dans référentiel bras
                p1_robot = p1 - origin_bras
                p2_robot = p2 - origin_bras

                msp.add_line((p1_robot.x(), -p1_robot.y()), (p2_robot.x(), -p2_robot.y()))

        doc.saveas("export_robot.dxf")
    else:
        QtWidgets.QMessageBox.warning(window, "Erreur", "Aucun DXF à imprimer")
# --------------------------------------------


# --- Ouvertur d'un fichier ---
def open_file():
    # Si un DXF est déjà chargé, demander confirmation
    if hasattr(window, "dxf_group"):
        réponse = QtWidgets.QMessageBox.question(
            window,
            "DXF déjà chargé",
            "Un DXF est déjà chargé. Voulez-vous le remplacer ?",
            QtWidgets.QMessageBox.StandardButton.Yes | QtWidgets.QMessageBox.StandardButton.No
        )

        if réponse == QtWidgets.QMessageBox.StandardButton.No:
            return  # L'utilisateur annule

        # L'utilisateur accepte → supprimer l'ancien DXF
        scene.removeItem(window.dxf_group)
        del window.dxf_group

    # Ouvrir un nouveau fichier
    filename, _ = QFileDialog.getOpenFileName(
        None,
        "Ouvrir un fichier DXF",
        "",
        "Fichiers DXF (*.dxf)"
    )

    if filename:
        print("Fichier choisi :", filename)
        window.textEdit.setText(os.path.basename(filename))
        window.dxf_group = load_dxf_into_scene(scene, window.graphicsView, filename)
    # --- test ---
    #readdxf.print_all_as_lines(filename)
#-------------------------------------------------------------


# --- Fermer le fichier ouvert ---
def close_file():
    if hasattr(window, "dxf_group"):
        scene.removeItem(window.dxf_group)
        del window.dxf_group
    else:
        QtWidgets.QMessageBox.warning(window, "Erreur", "Aucun DXF à fermer.")
#----------------------------------


# --- Changer les vitesse en mode manuel. ---
def change_angular_speed():
    global angular_speed
    text = window.edit_angular_speed.text()
    angular_speed = float(text) if text else 1

def change_linear_speed():
    global linear_speed
    text = window.edit_linear_speed.text()
    linear_speed = float(text) if text else 1
# -------------------------------------------




# --- forcer float -----------------------------------------------------
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
# ------------------------------------------------------------------------


# --- Position "maison"
def go_home():
    global shoulder_angle
    global elbow_angle
    shoulder_angle =0
    elbow_angle = 170
    animator.setAngle(shoulder_angle)
    animator_elbow.setAngle(elbow_angle)



# --- Fonctions boutons -------------------------------------------

# linéaire
def move_forward():
    global tool_pos_y
    tool_pos_y += linear_speed

def move_backward():
    global tool_pos_y
    tool_pos_y -= linear_speed

def move_right():
    global tool_pos_x
    tool_pos_x += linear_speed

def move_left():
    global tool_pos_x
    tool_pos_x -= linear_speed

# Angulaire
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
#------------------------------------------------------------------------
    

# --- DXF loader ---
def load_dxf_into_scene(scene, view, filename):
    doc = ezdxf.readfile(filename)
    msp = doc.modelspace()

    pen = QPen(Qt.GlobalColor.black)
    pen.setWidth(1)

    count = 0

    # --- Groupe DXF déplaçable ---
    window.dxf_group = scene.createItemGroup([])

    for entity in msp:
        print("TYPE DXF:", entity.dxftype())
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

                line = QGraphicsLineItem(x1, -y1, x2, -y2)
                line.setPen(pen)
                window.dxf_group.addToGroup(line)
                count += 1
        if entity.dxftype() == "LINE":
            x1, y1, _ = entity.dxf.start
            x2, y2, _ = entity.dxf.end

            line = QGraphicsLineItem(x1, -y1, x2, -y2)
            line.setPen(pen)
            window.dxf_group.addToGroup(line)
            count += 1
        if entity.dxftype() == "LWPOLYLINE":
            pts = entity.get_points()  # liste de tuples (x, y, [bulge])

            # Segments successifs
            for i in range(len(pts) - 1):
                x1, y1 = pts[i][0], pts[i][1]
                x2, y2 = pts[i+1][0], pts[i+1][1]

                line = QGraphicsLineItem(x1, -y1, x2, -y2)
                line.setPen(pen)
                window.dxf_group.addToGroup(line)
                count += 1

            # Si la polyline est fermée
            if entity.closed:
                x1, y1 = pts[-1][0], pts[-1][1]
                x2, y2 = pts[0][0], pts[0][1]

                line = QGraphicsLineItem(x1, -y1, x2, -y2)
                line.setPen(pen)
                window.dxf_group.addToGroup(line)
                count += 1

    print("Segments drawn:", count)

    # --- Rendre le DXF déplaçable ---
    window.dxf_group.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, True)
    window.dxf_group.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable, True)

    # --- Centrage initial ---
    bbox = window.dxf_group.boundingRect()

    scene_center_x = 400
    scene_center_y = 300

    dxf_center_x = bbox.x() + bbox.width() / 2
    dxf_center_y = bbox.y() + bbox.height() / 2

    dx = scene_center_x - dxf_center_x
    dy = scene_center_y - dxf_center_y

    window.dxf_group.moveBy(dx, dy)

    return window.dxf_group



# --- Animation ---
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
    
    app = QtWidgets.QApplication(sys.argv)

    window = QtWidgets.QMainWindow()
    uic.loadUi('plasmarm.ui', window)
    
    window.graphicsView.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)


    window.progressBar.setValue(0)

    # --- Connection aux éléments graphiques ---
    window.Button_Print.clicked.connect(func_print)
    window.Button_Home.clicked.connect(go_home)

    window.Bras1_Horaire.clicked.connect(shoulder_clockwise)
    window.Bras1_Antihoraire.clicked.connect(shoulder_counterclockwise)
    window.Elbow_clockwise.clicked.connect(elbow_clockwise)
    window.Elbow_counterclockwise.clicked.connect(elbow_counterclockwise)


    window.button_forward.clicked.connect(move_forward)
    window.button_backward.clicked.connect(move_backward)
    window.button_right.clicked.connect(move_right)
    window.button_left.clicked.connect(move_left)
    # window.button_rise.clicked.connect(tool_up)
    # window.button_descent.clicked.connect(tool_down)

    window.actionLoad.triggered.connect(open_file)
    window.actionFermer.triggered.connect(close_file)

    window.edit_angular_speed.textChanged.connect(change_angular_speed)
    window.edit_linear_speed.textChanged.connect(change_linear_speed)

    enforce_float_only(window.edit_angular_speed)
    enforce_float_only(window.edit_linear_speed)
    

    # --- Scène ---
    scene = QGraphicsScene()
    window.graphicsView.setScene(scene)
    window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
    scene.setSceneRect(0, 0, 800, 500)

    # --- Épaule ---
    shoulder = QGraphicsRectItem(QRectF(0, 0, 150, 20))
    scene.addItem(shoulder)
    shoulder.setTransformOriginPoint(0, 10)
    shoulder.setPos(400, 450)

    # --- Coude ---
    elbow = QGraphicsRectItem(QRectF(0, 0, 148, 20))
    elbow.setParentItem(shoulder)
    elbow.setTransformOriginPoint(0, 10)
    elbow.setPos(140, 0)

    # --- Animateurs ---
    animator = AngleAnimator(shoulder)
    animator_elbow = AngleAnimator(elbow)

    # --- Affichage porté max ---
    arc_max = QGraphicsEllipseItem(112, 172, 576, 576)
    scene.addItem(arc_max)
    arc_max.setStartAngle(0)
    arc_max.setSpanAngle(180 * 16)
    arc_max.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_max.setZValue(1000)

    # --- Affichage porté min ---
    arc_min = QGraphicsEllipseItem(379, 439, 42, 42)
    scene.addItem(arc_min)
    arc_min.setStartAngle(0)
    arc_min.setSpanAngle(180 * 16)
    arc_min.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_min.setZValue(1000)

    window.show()
    sys.exit(app.exec())

    

