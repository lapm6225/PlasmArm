from PyQt6 import QtWidgets, uic
from PyQt6.QtWidgets import QGraphicsScene, QGraphicsRectItem, QLineEdit
from PyQt6.QtCore import  QRectF
import sys
import math
from collections import namedtuple
import dxf
import animation

# --- Variables Globales ---
class Position:
    def __init__(self, x, y):
        self.x = x
        self.y = y

angular_speed = 1 # deg/click
linear_speed = 1 # mm/click
shoulder_angle = 0 # degrés
elbow_angle = 0 # degrés
tool_raised = True # true == levé / false == baissé

Arm = namedtuple("Arm", ["length", "width"])
bicep = Arm(150, 20)
forearm = Arm(164.15, 20)

tool_pos = Position(forearm.length+bicep.length, 0) 

Position = namedtuple("Position", ["x", "y"])
origin = Position(400, 400)

# Vérification des limites du bras
def limit(shoulder, elbow):
    if elbow > 135 or elbow <- 135:
        dxf.add_text(window, "Max elbow angle reached")
        return False
    
    if shoulder > 34 or shoulder < -214:
        dxf.add_text(window, "Max elbow angle reached")
        return False
    
    x = bicep.length*math.cos(shoulder*math.pi/180)+forearm.length*math.cos((shoulder+elbow)*math.pi/180)
    y = bicep.length*math.sin(shoulder*math.pi/180)+forearm.length*math.sin((shoulder+elbow)*math.pi/180)

    if math.sqrt(pow(x,2)+pow(y,2))>314.3:
        dxf.add_text(window, "Out of bounds")
        return False
    
    if y > 0:
        dxf.add_text(window, "Out of range in Y axis")
        print("test")
        return False
    
    else:
        x=round(x,2)
        y=round(y,2)
        text = f"Position de l'effecteur: ({x}, {-y})"
        dxf.add_text(window, text )
        return True


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

# --- Position "maison" ---
def go_home():
    global shoulder_angle
    global elbow_angle
    shoulder_angle =0
    elbow_angle = -170
    animator.setAngle(shoulder_angle)
    animator_elbow.setAngle(elbow_angle)
#-----------------------------


# Fonctions boutons -------------------------------------------
# --- Générer un dxf par rapport à l'origine ---
def func_print():
    dxf.func_print(window)

# --- Ouvrir un fichier ---
def open_file():
    dxf.open_file(window, scene)

# --- fermer un fichier ---
def close_file():
    dxf.close_file(window, scene)

def tool_up():
    global tool_raised
    if tool_raised == True:
        dxf.add_text(window, "Tool already raised")
    else:
        tool_raised = True
        dxf.add_text(window, "Tool raised")

def tool_down():
    global tool_raised
    if tool_raised == False:
        dxf.add_text(window, "Tool already lowered")
    else:
        tool_raised = False
        dxf.add_text(window, "Tool lowered")

# --- mouvement linéaire ---
def move_forward():
    global tool_pos
    y = tool_pos.y - linear_speed
    tool_pos.y = y
    print(tool_pos.x, tool_pos.y)
    
def move_backward():
    global tool_pos
    y = tool_pos.y + linear_speed
    tool_pos.y = y
    print(tool_pos.x, tool_pos.y)

def move_right():
    global tool_pos
    x=tool_pos.x + linear_speed
    tool_pos.x = x
    print(tool_pos.x, tool_pos.y)

def move_left():
    global tool_pos
    x=tool_pos.x - linear_speed
    tool_pos.x = x
    print(tool_pos.x, tool_pos.y)

# --- mouvement angulaire ---
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
    

##########################
# MAIN
##########################
if __name__ == '__main__':
    
    app = QtWidgets.QApplication(sys.argv)

    window = QtWidgets.QMainWindow()
    uic.loadUi('plasmarm.ui', window)
    
    window.graphicsView.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)
    window.progressBar.setValue(0)

    # Connection aux éléments graphiques ---
    # --- Boutons principaux
    window.Button_DXF.clicked.connect(func_print)
    window.Button_Home.clicked.connect(go_home)
    #window.Button_Print.clicked.connect()
    #window.Button_Stop.clicked.connect()

    # --- Boutons Rotation ---
    window.Shoulder_Horaire.clicked.connect(shoulder_clockwise)
    window.Shoulder_Antihoraire.clicked.connect(shoulder_counterclockwise)
    window.Elbow_clockwise.clicked.connect(elbow_clockwise)
    window.Elbow_counterclockwise.clicked.connect(elbow_counterclockwise)

    # --- Boutons linéaire ---
    window.button_forward.clicked.connect(move_forward)
    window.button_backward.clicked.connect(move_backward)
    window.button_right.clicked.connect(move_right)
    window.button_left.clicked.connect(move_left)

    # --- Boutons Effecteur ---
    window.button_rise.clicked.connect(tool_up)
    window.button_descent.clicked.connect(tool_down)

    # --- Boutons Fichier ---
    window.actionLoad.triggered.connect(open_file)
    window.actionFermer.triggered.connect(close_file)

    # --- LineEdit pour vitesse ---
    window.edit_angular_speed.textChanged.connect(change_angular_speed)
    window.edit_linear_speed.textChanged.connect(change_linear_speed)
    # --- forcer l'entrée des lineEdit 
    enforce_float_only(window.edit_angular_speed)
    enforce_float_only(window.edit_linear_speed)
    
    # Génération de la scène --- 
    scene = QGraphicsScene()
    shoulder = QGraphicsRectItem(QRectF(0, 0, bicep.length, bicep.width))
    elbow = QGraphicsRectItem(QRectF(0, 0, forearm.length, forearm.width))
    animator = animation.AngleAnimator(shoulder)
    animator_elbow = animation.AngleAnimator(elbow)
    animation.generate_scene(window, scene, bicep, forearm, origin, elbow, shoulder)
   
    window.show()
    sys.exit(app.exec())