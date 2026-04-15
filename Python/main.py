from PyQt6 import QtWidgets, uic
from PyQt6.QtWidgets import QGraphicsScene, QLineEdit, QGraphicsPixmapItem
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
import help_dialog

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
bicep = Arm(220, 35)
forearm = Arm(220, 54)

# --- Données de position ---
Point = namedtuple("Position", ["x", "y"])
tool_pos = Position(forearm.length+bicep.length, 0) 
origin = Point(550, 450)

# --- Verify that the commands are in the available range ---
def limit(shoulder, elbow):
    # Max angle of the 2e arm
    if elbow > 150 or elbow <- 150:
        dxf.add_text(window, "Max elbow angle reached")
        return False
    
    # Max angle of the 1st arm
    if shoulder > 0 or shoulder < -180:
        dxf.add_text(window, "Max elbow angle reached")
        return False
    
    x = bicep.length*math.cos(shoulder*math.pi/180)+forearm.length*math.cos((shoulder+elbow)*math.pi/180)
    y = bicep.length*math.sin(shoulder*math.pi/180)+forearm.length*math.sin((shoulder+elbow)*math.pi/180)

    # Max range of the arm
    if math.sqrt(pow(x,2)+pow(y,2))>bicep.length+forearm.length+0.1:
        dxf.add_text(window, "Out of bounds")
        return False
    
    # Limitation of the workspace (demi circle)
    if y > 0:
        dxf.add_text(window, "Out of range in Y axis")
        print("test")
        return False
    
    # Actualise the tool position
    else:
        x=round(x,2)
        y=round(y,2)
        # Debug prints
        text = f"Position de l'effecteur: ({x}, {-y})"
        dxf.add_text(window, text )
        return True


# Open a help window with instruction on how to use the program
def open_help():
    dlg = help_dialog.HelpDialog()
    dlg.exec()

# --- Change Speeds ---
def change_angular_speed():
    global angular_speed
    text = window.angSpeedEdit.text() # lecture du texte
    angular_speed = float(text) if text else 1

def change_linear_speed():
    global linear_speed
    text = window.linSpeedEdit.text() # lecture du texte
    linear_speed = float(text) if text else 1
# -------------------------------------------

# --- Force floats -----------------------------
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
    #animator.setAngle(shoulder_angle)
    #animator_elbow.setAngle(elbow_angle)
    worker.send_cmd({"type": "HOME"})
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
    tool_raised = True
    worker.send_cmd({"type": "TOOL", "state": "UP"})
    dxf.add_text(window, "Tool raised (action demandée)")

# --- Descendre l'outil ---
def tool_down():
    global tool_raised
    tool_raised = False
    worker.send_cmd({"type": "TOOL", "state": "DOWN"})
    dxf.add_text(window, "Tool lowered (action demandée)")

def send_target_move():
    worker.send_cmd({
        "type": "MOVE_TO",
        "x": tool_pos.x,
        "y": -tool_pos.y,
        "speed": linear_speed
    })

# --- mouvement linéaire selon la vitesse linéaire ---
def move_forward():
    global tool_pos
    y = tool_pos.y - linear_speed
    tool_pos.y = y
    print(tool_pos.x, -(tool_pos.y))
    send_target_move()
    
def move_backward():
    global tool_pos
    y = tool_pos.y + linear_speed
    tool_pos.y = y
    send_target_move()

def move_right():
    global tool_pos
    x=tool_pos.x + linear_speed
    tool_pos.x = x
    send_target_move()

def move_left():
    global tool_pos
    x=tool_pos.x - linear_speed
    tool_pos.x = x
    send_target_move()

# --- mouvement angulaire selon la vitesse angulaire---
def shoulder_clockwise():
    global shoulder_angle, angular_speed
    temp_angle = shoulder_angle + angular_speed
    if limit(temp_angle,elbow_angle):
        shoulder_angle= temp_angle
    #animator.setAngle(-shoulder_angle)

def shoulder_counterclockwise():
    global shoulder_angle, angular_speed
    temp_angle = shoulder_angle - angular_speed
    if limit(temp_angle,elbow_angle):
        shoulder_angle= temp_angle
    #animator.setAngle(-shoulder_angle)

def elbow_clockwise():
    global elbow_angle, angular_speed
    temp_angle = elbow_angle + angular_speed
    if limit(shoulder_angle,temp_angle):
        elbow_angle= temp_angle
    #animator_elbow.setAngle(-elbow_angle)

def elbow_counterclockwise():
    global elbow_angle, angular_speed
    temp_angle = elbow_angle - angular_speed
    if limit(shoulder_angle,temp_angle):
        elbow_angle= temp_angle
    #animator_elbow.setAngle(-elbow_angle)
#------------------------------------------------------------------------

# --- Imprimer ---
def func_print():
    if hasattr(window, "dxf_preview"):
        dxf.add_text(window, "Début de la découpe")
        cut=DxfParser("export_robot.dxf")
        commands = cut.parse()
        cut.print_preview(commands)
        worker.stream_commands(commands)
    
# ----------------

# --- Arrêt ---
def func_stop():
    worker.trigger_emergency_stop()
    dxf.add_text(window, "Arrêt de la découpe (STOP command envoyé)")
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

def connect():
    ip = window.ipEdit.text().strip()
    print("IP entered:", ip)
    dxf.add_text(window, f"Connexion à {ip} demandée...")
    worker.connect_robot(ip)

def disconnect():
    worker.disconnect_robot()
    dxf.add_text(window, "Déconnexion demandée...")
    print("disconnect")

# Callbacks QThread
def on_connected(success):
    if success:
        dxf.add_text(window, "Connecté à l'ESP32 avec succès !")
    else:
        dxf.add_text(window, "Erreur lors de la connexion.")

def on_status_received(data):
    global tool_raised
    
    # 1. Mise à jour de la visualisation du jumeau numérique (les angles réels)
    if 'theta1' in data:
        animator.setAngle(-data['theta1'])
    if 'theta2' in data:
        animator_elbow.setAngle(-data['theta2'])

    # 2. Mise à jour du retour utilisateur des coordonnées
    if 'x' in data and 'y' in data:
        # Quand le bras n'est pas en mouvement (ou si on veut un affichage temps réel)
        x_reel = data['x']
        y_reel = data['y']
        text = f"Effecteur réel : ({x_reel:.1f}, {y_reel:.1f})"
        if 'isMoving' in data and data['isMoving']:
            text += " [En mouvement...]"
        elif not data.get('isMoving', False):
            # Recalibrer la cible locale si inactif pour ne pas "sauter" un pas au clic suivant
            tool_pos.x = x_reel
            tool_pos.y = -y_reel
        dxf.add_text(window, text)

    # 3. État de l'outil
    if 'tool' in data:
        tool_raised = not data['tool']

def on_error(err_msg):
    dxf.add_text(window, f"⚠️ Erreur WS : {err_msg}")

def toggle_connection():
    connection_panel.setHidden(not connection_panel.isHidden())
    auto_fit.schedule_refit()


def toggle_control():
    control_panel.setHidden(not control_panel.isHidden())
    auto_fit.schedule_refit()

def manual():
    worker.send_cmd({"type": "SET_HOME"})

def calibration():
    worker.send_cmd({"type": "SAVE_HOME"})
    worker.send_cmd({"type": "HOME"})

    


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

def toggle_maximize():
        if window.isMaximized():
            window.showNormal()
        else:
            window.showMaximized()


def connect_buttons():
    # Left container buttons
    window.connectPanelBtn.clicked.connect(toggle_connection) # Opens Connection menu

    window.downloadBtn.clicked.connect(open_file) # Load a dxf file
    window.deleteBtn.clicked.connect(close_file) # delete a loaded dxf file
    window.dxfBtn.clicked.connect(func_DXF) # Generate a dxf file centered with the base of the robot

    window.startBtn.clicked.connect(func_print) # Start or resume the print
    window.pauseBtn.clicked.connect(func_stop) # pause the print
    window.stopBtn.clicked.connect(func_stop) # stop and cancel the print

    window.helpBtn.clicked.connect(open_help) # Opens a help dialog

    # Connect Menu buttons
    window.connectBtn.clicked.connect(connect)
    window.disconnectBtn.clicked.connect(disconnect)

    # Controls menu buttons
    # --- Angular motion buttons
    window.A1cBtn.clicked.connect(shoulder_clockwise)
    window.A1ccBtn.clicked.connect(shoulder_counterclockwise)
    window.A2cBtn.clicked.connect(elbow_clockwise)
    window.A2ccBtn.clicked.connect(elbow_counterclockwise)

     # --- Linear motion buttons
    window.upBtn.clicked.connect(move_forward)
    window.downBtn.clicked.connect(move_backward)
    window.rightBtn.clicked.connect(move_right)
    window.leftBtn.clicked.connect(move_left)

    # --- Tools controls
    window.toolUpBtn.clicked.connect(tool_up)
    window.toolDownBtn.clicked.connect(tool_down)

    # Window buttons
    window.closeBtn.clicked.connect(window.close)
    window.minBtn.clicked.connect(window.showMinimized)
    window.maxBtn.clicked.connect(toggle_maximize)

    # Right Menu buttons
    window.jogBtn.clicked.connect(toggle_control)
    window.homeBtn.clicked.connect(go_home)
    window.manualBtn.clicked.connect(manual)
    window.calibrateBtn.clicked.connect(calibration)


def connect_line_edits():
     # ------- speed controls ---
    window.angSpeedEdit.textChanged.connect(change_angular_speed)
    window.linSpeedEdit.textChanged.connect(change_linear_speed)

### --------------------------------------------------------------
### Main
### --------------------------------------------------------------
if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)

    window = QtWidgets.QMainWindow()
    uic.loadUi('plasmarm_v2.ui', window)
    window.setWindowFlags(Qt.WindowType.FramelessWindowHint |Qt.WindowType.Window
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

    # Control Panel Open and Close
    control_panel = window.ControlContainer
    control_panel.setHidden(True)

    # Lancement du Worker PyQt
    worker = comm.RobotWorker()
    worker.connected_signal.connect(on_connected)
    worker.status_received_signal.connect(on_status_received)
    worker.error_signal.connect(on_error)
    worker.start()  # Démarre la boucle asyncio en fond

    # initialisation des éléments de l'interface
    window.graphicsView.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)
    window.progressBar.setValue(0)
    
    # --- forcer l'entrée des lineEdit 
    enforce_float_only(window.angSpeedEdit)
    enforce_float_only(window.linSpeedEdit)
    
    titlebar_drag = TitleBarDrag(window, window.headerContainer)

    # Initialize graphic items
    connect_buttons()
    connect_line_edits()

    window.show()
    exit_code = app.exec()
    
    # Fermeture propre
    worker.stop()
    worker.quit()
    worker.wait()
    sys.exit(exit_code)