from PyQt6 import QtWidgets, uic
from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QPushButton, QGraphicsScene, QLineEdit, QGraphicsPixmapItem
from PyQt6.QtGui import QPixmap
from PyQt6.QtCore import  Qt, QObject, QEvent, Qt, QTimer
from collections import namedtuple

import sys
import math


# Other files to import
import dxf
import animation
from dxf_parser import DxfParser
import communication as comm

# ressources.qrc to compile  with pyrcc6
import ressources_rc

class Position:
    def __init__(self, x, y):
        self.x = x
        self.y = y

angular_speed = 1 # deg/click
linear_speed = 1 # mm/click
shoulder_angle = 0 # degrés
elbow_angle = 0 # degrés
tool_raised = True # true == levé / false == baissé

# --- Données de dimension des bras --- 
Arm = namedtuple("Arm", ["length", "width"])
bicep = Arm(222, 35)
forearm = Arm(216, 54)

# --- Données de position ---
Point = namedtuple("Position", ["x", "y"])
tool_pos = Position(forearm.length+bicep.length, 0) 
origin = Point(550, 450)

# --- Vérification des limites du bras ---
def limit(shoulder, elbow):
    # Angle max du 2e bras
    if elbow > 155 or elbow <- 155:
        dxf.add_text(window, "Max elbow angle reached")
        return False
    
    # Angle max du 1e bras
    if shoulder > 0 or shoulder < -180:
        dxf.add_text(window, "Max elbow angle reached")
        return False
    
    x = bicep.length*math.cos(shoulder*math.pi/180)+forearm.length*math.cos((shoulder+elbow)*math.pi/180)
    y = bicep.length*math.sin(shoulder*math.pi/180)+forearm.length*math.sin((shoulder+elbow)*math.pi/180)

    # Distance max
    if math.sqrt(pow(x,2)+pow(y,2))>bicep.length+forearm.length+0.1:
        dxf.add_text(window, "Out of bounds")
        return False
    
    # Limite du plan
    if y > 0:
        dxf.add_text(window, "Out of range in Y axis")
        print("test")
        return False
    
    # Actualisation de la position de l'effecteur
    else:
        x=round(x,2)
        y=round(y,2)
        # Affichage
        text = f"Position de l'effecteur: ({x}, {-y})"
        dxf.add_text(window, text )
        return True

class HelpDialog(QDialog):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Aide")
        self.setMinimumWidth(400)

        self.setStyleSheet("""
            QDialog {
                background-color: #111;
                color: #eee;
            }
            QLabel {
                color: #eee;
            }
            QPushButton {
                background-color: #333;
                color: #eee;
                border: 1px solid #555;
                padding: 6px;
                border-radius: 4px;
            }
            QPushButton:hover {
                background-color: #444;
            }
        """)

        layout = QVBoxLayout()

        help_text = """
        <h2>Aide de l'application</h2>
        <p>Voici quelques informations utiles :</p>
        <ul>
            <li>Utilisez le menu <b>Fichier</b> pour ouvrir ou sauvegarder.</li>
            <li>Le bouton <b>Exécuter</b> lance le traitement.</li>
            <li>Consultez la documentation pour plus de détails.</li>
        </ul>
        """

        label = QLabel(help_text)
        label.setWordWrap(True)

        close_btn = QPushButton("Fermer")
        close_btn.clicked.connect(self.close)

        layout.addWidget(label)
        layout.addWidget(close_btn)
        self.setLayout(layout)


def open_help():
    dlg = HelpDialog()
    dlg.exec()   # PyQt6 : exec() et non exec_()

# --- Changer les vitesse en mode manuel ---
def change_angular_speed():
    global angular_speed
    text = window.angSpeedEdit.text() # lecture du texte
    angular_speed = float(text) if text else 1

def change_linear_speed():
    global linear_speed
    text = window.linSpeedEdit.text() # lecture du texte
    linear_speed = float(text) if text else 1
# -------------------------------------------

# --- forcer float pour lire la valeur de vitesse -----------------------------
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

# --- Position "maison" ---
def go_home():
    global shoulder_angle
    global elbow_angle
    shoulder_angle =0
    elbow_angle = -154
    animator.setAngle(shoulder_angle)
    animator_elbow.setAngle(elbow_angle)
    # await client.send_command({"type": "HOME"})
    # await asyncio.sleep(0.3)
#-----------------------------


# Fonctions boutons -------------------------------------------
# --- Générer un dxf par rapport à l'origine ---
def func_DXF():
    dxf.func_DXF(window,scene)

# --- Ouvrir un fichier ---
def open_file():
    dxf.open_file(window, scene)

# --- fermer un fichier ---
def close_file():
    dxf.close_file(window, scene)

# --- Monter l'outil ---
def tool_up():
    global tool_raised
    if tool_raised == True:
        dxf.add_text(window, "Tool already raised")
    else:
        tool_raised = True
        # await client.send_command({"type": "TOOL", "state": True, "z": 0.0})
        # await asyncio.sleep(0.3)
        dxf.add_text(window, "Tool raised")

# --- Descendre l'outil ---
def tool_down():
    global tool_raised
    if tool_raised == False:
        dxf.add_text(window, "Tool already lowered")
    else:
        tool_raised = False
        # await client.send_command({"type": "TOOL", "state": False, "z": 0.0})
        # await asyncio.sleep(0.3)
        dxf.add_text(window, "Tool lowered")

# --- mouvement linéaire selon la vitesse linéaire ---
def move_forward():
    global tool_pos
    y = tool_pos.y - linear_speed
    tool_pos.y = y
    print(tool_pos.x, -(tool_pos.y))
    text = f"Position de l'effecteur: ({tool_pos.x}, {-tool_pos.y})"
    dxf.add_text(window, text )
    
def move_backward():
    global tool_pos
    y = tool_pos.y + linear_speed
    tool_pos.y = y
    text = f"Position de l'effecteur: ({tool_pos.x}, {-tool_pos.y})"
    dxf.add_text(window, text )

def move_right():
    global tool_pos
    x=tool_pos.x + linear_speed
    tool_pos.x = x
    text = f"Position de l'effecteur: ({tool_pos.x}, {-tool_pos.y})"
    dxf.add_text(window, text )
def move_left():
    global tool_pos
    x=tool_pos.x - linear_speed
    tool_pos.x = x
    text = f"Position de l'effecteur: ({tool_pos.x}, {-tool_pos.y})"
    dxf.add_text(window, text )

# --- mouvement angulaire selon la vitesse angulaire---
def shoulder_clockwise():
    global shoulder_angle, angular_speed
    temp_angle = shoulder_angle + angular_speed
    if limit(temp_angle,elbow_angle):
        shoulder_angle= temp_angle
    animator.setAngle(shoulder_angle)

def shoulder_counterclockwise():
    global shoulder_angle, angular_speed
    temp_angle = shoulder_angle - angular_speed
    if limit(temp_angle,elbow_angle):
        shoulder_angle= temp_angle
    animator.setAngle(shoulder_angle)

def elbow_clockwise():
    global elbow_angle, angular_speed
    temp_angle = elbow_angle + angular_speed
    if limit(shoulder_angle,temp_angle):
        elbow_angle= temp_angle
    animator_elbow.setAngle(elbow_angle)

def elbow_counterclockwise():
    global elbow_angle, angular_speed
    temp_angle = elbow_angle - angular_speed
    if limit(shoulder_angle,temp_angle):
        elbow_angle= temp_angle
    animator_elbow.setAngle(elbow_angle)
#------------------------------------------------------------------------

# --- Imprimer ---
def func_print():
    if hasattr(window, "dxf_preview"):
        dxf.add_text(window, "Début de la découpe")
        cut=DxfParser("export_robot.dxf")
        command = cut.parse()
        cut.print_preview(command)
    # transmettre command  au esp32
# ----------------

# --- Arrêt ---
def func_stop():
    # await client.send_command({"type": "STOP"})
    # await asyncio.sleep(0.5)
    dxf.add_text(window, "Arrêt de la découpe")
# -------------







class AutoFitView(QObject):
    def __init__(self, window, view, scene):
        super().__init__()
        self.window = window
        self.view = view
        self.scene = scene
        self.defer = False

        QTimer.singleShot(0, self.refit)

    def schedule_refit(self):
        # On ne bloque jamais les resize
        if not self.defer:
            self.defer = True
            QTimer.singleShot(0, self._do_refit)

    def _do_refit(self):
        self.defer = False
        self.refit()

    def refit(self):
        rect = self.scene.itemsBoundingRect()
        rect = rect.adjusted(-40, -40, 40, 40)
        if not rect.isNull():
            self.view.fitInView(rect, Qt.AspectRatioMode.KeepAspectRatio)

    def eventFilter(self, obj, event):
        # Resize → refit immédiat (toujours)
        if obj is self.view and event.type() == QEvent.Type.Resize:
            self.refit()

        # Maximiser / Restaurer → refit différé
        if obj is self.window and event.type() == QEvent.Type.WindowStateChange:
            QTimer.singleShot(0, self.refit)

        return False



def toggle_connection():
    connection_panel.setHidden(not connection_panel.isHidden())
    auto_fit.schedule_refit()


def toggle_control():
    control_panel.setHidden(not control_panel.isHidden())
    auto_fit.schedule_refit()



class TitleBarDrag(QObject):
    def __init__(self, window, header):
        super().__init__()
        self.window = window
        self.header = header

        header.installEventFilter(self)
        for child in header.findChildren(QObject):
            child.installEventFilter(self)

    def eventFilter(self, obj, event):
        # Drag depuis header ou ses enfants
        if obj is self.header or obj.parent() is self.header:

            # Début du drag
            if event.type() == QEvent.Type.MouseButtonPress:
                if event.button() == Qt.MouseButton.LeftButton:
                    if self.window.windowHandle():
                        self.window.windowHandle().startSystemMove()
                return True

        return False

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)

    window = QtWidgets.QMainWindow()
    uic.loadUi('plasmarm_v2.ui', window)
    window.setWindowFlags(
    Qt.WindowType.FramelessWindowHint |
    Qt.WindowType.Window
)
    # Génération de la scène pour l'affichage--- 
    scene = QGraphicsScene()
    shoulder_img = QPixmap("bras1.png")      # image du segment 1
    elbow_img = QPixmap("bras2.png")       # image du segment 2
    shoulder = QGraphicsPixmapItem(shoulder_img)
    elbow = QGraphicsPixmapItem(elbow_img)
    animator = animation.AngleAnimator(shoulder)
    animator_elbow = animation.AngleAnimator(elbow)
    animation.generate_scene(window, scene, bicep, forearm, origin, elbow, shoulder)

    auto_fit = AutoFitView(window, window.graphicsView, scene)
    window.graphicsView.installEventFilter(auto_fit)
    window.installEventFilter(auto_fit)

    # Connection Panel Open and Close
    connection_panel = window.ConnectionPanel
    connection_panel.setHidden(True)
    connection_panel.setMinimumWidth(connection_panel.width())
    window.connectPanelBtn.clicked.connect(toggle_connection)

    # Control Panel Open and Close
    control_panel = window.ControlContainer
    control_panel.setHidden(True)
    #control_panel.setMinimumHeight(connection_panel.width())
    window.jogBtn.clicked.connect(toggle_control)

    window.helpBtn.clicked.connect(open_help)



    client = comm.RobotWSClient()
    
    # initialisation des éléments de l'interface
    window.graphicsView.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)
    window.progressBar.setValue(0)

    # Connection aux éléments graphiques ---
    # --- Boutons principaux
    window.dxfBtn.clicked.connect(func_DXF)
    window.homeBtn.clicked.connect(go_home)
    window.startBtn.clicked.connect(func_print)
    window.pauseBtn.clicked.connect(func_stop)

    # --- Boutons Rotation ---
    window.A1cBtn.clicked.connect(shoulder_clockwise)
    window.A1ccBtn.clicked.connect(shoulder_counterclockwise)
    window.A2cBtn.clicked.connect(elbow_clockwise)
    window.A2ccBtn.clicked.connect(elbow_counterclockwise)

    # --- Boutons linéaire ---
    window.upBtn.clicked.connect(move_forward)
    window.downBtn.clicked.connect(move_backward)
    window.rightBtn.clicked.connect(move_right)
    window.leftBtn.clicked.connect(move_left)

    # --- Boutons Effecteur ---
    window.toolUpBtn.clicked.connect(tool_up)
    window.toolDownBtn.clicked.connect(tool_down)

    # --- Boutons Fichier ---
    window.downloadBtn.clicked.connect(open_file)
    window.deleteBtn.clicked.connect(close_file)

    # --- LineEdit pour vitesse ---
    window.angSpeedEdit.textChanged.connect(change_angular_speed)
    window.linSpeedEdit.textChanged.connect(change_linear_speed)
    # --- forcer l'entrée des lineEdit 
    enforce_float_only(window.angSpeedEdit)
    enforce_float_only(window.linSpeedEdit)
    
    
    window.closeBtn.clicked.connect(window.close)
    window.minBtn.clicked.connect(window.showMinimized)

    def toggle_maximize():
        if window.isMaximized():
            window.showNormal()
        else:
            window.showMaximized()

    window.maxBtn.clicked.connect(toggle_maximize)
    titlebar_drag = TitleBarDrag(window, window.headerContainer)



    window.show()
    sys.exit(app.exec())