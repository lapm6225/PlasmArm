from PyQt6 import QtWidgets, uic, QtCore
from PyQt6.QtWidgets import QGraphicsScene, QLineEdit, QGraphicsPixmapItem, QGraphicsView
from PyQt6.QtGui import QPixmap, QIcon
from PyQt6.QtCore import Qt, QObject, QEvent, QTimer
from collections import namedtuple

import sys
import math

# External modules (robot communication, DXF handling, animation, help dialog)
import dxf_display
import animation
from dxf_parser import DxfParser
import communication as comm
import help_dialog

# Compiled Qt resources (icons, images)
import ressources_rc


# Simple class to store XY coordinates
class Position:
    def __init__(self, x, y):
        self.x = x
        self.y = y

# Global motion parameters
angular_speed = 1      # degrees per click
linear_speed = 1       # mm per click
shoulder_angle = 0     # degrees
elbow_angle = 0        # degrees
tool_raised = True     # True = tool lifted
click_move_enabled = False  # Enables click-to-move mode


# Arm geometry (length + width)
Arm = namedtuple("Arm", ["length", "width"])
bicep = Arm(216, 35)
forearm = Arm(214, 54)

# Robot origin and initial tool position
Point = namedtuple("Position", ["x", "y"])
tool_pos = Position(forearm.length + bicep.length, 0)
origin = Point(550, 450)


# Opens the help dialog window
def open_help():
    dlg = help_dialog.HelpDialog()
    dlg.exec()


# Reads angular speed from UI
def change_angular_speed():
    global angular_speed
    text = window.angSpeedEdit.text()
    angular_speed = float(text) if text else 1


# Reads linear speed from UI
def change_linear_speed():
    global linear_speed
    text = window.linSpeedEdit.text()
    linear_speed = float(text) if text else 1


# Restricts QLineEdit input to valid float characters
def enforce_float_only(line_edit: QLineEdit):
    def clean(text):
        allowed = "0123456789.-"
        cleaned = "".join(c for c in text if c in allowed)

        # Ensure only one decimal point
        if cleaned.count('.') > 1:
            parts = cleaned.split('.', 1)
            cleaned = parts[0] + '.' + parts[1].replace('.', '')

        # Ensure minus sign only at the beginning
        if cleaned.count('-') > 1:
            cleaned = cleaned.replace('-', '', cleaned.count('-') - 1)
        if '-' in cleaned and cleaned.index('-') != 0:
            cleaned = cleaned.replace('-', '')

        if cleaned != text:
            line_edit.setText(cleaned)

    line_edit.textChanged.connect(clean)


# Sends HOME command to robot
def go_home():
    worker.send_cmd({"type": "HOME"})


# DXF generation relative to robot origin
def func_DXF():
    dxf_display.func_DXF(window, scene)


# Opens a DXF file
def open_file():
    dxf_display.open_file(window, scene)


# Closes the currently loaded DXF
def close_file():
    dxf_display.close_file(window, scene)


# Raises the tool (pen up)
def tool_up():
    global tool_raised
    tool_raised = True
    worker.send_cmd({"type": "TOOL", "state": "UP"})
    print("Tool raised (requested)")


# Lowers the tool (pen down)
def tool_down():
    global tool_raised
    tool_raised = False
    worker.send_cmd({"type": "TOOL", "state": "DOWN"})
    print("Tool lowered (requested)")


# Sends a MOVE_TO command using the current tool_pos
def send_target_move():
    worker.send_cmd({
        "type": "MOVE_TO",
        "x": tool_pos.x,
        "y": -tool_pos.y,  # Y inverted for robot coordinates
        "speed": linear_speed
    })


# Linear motion helpers
def move_forward():
    tool_pos.y -= linear_speed
    send_target_move()

def move_backward():
    tool_pos.y += linear_speed
    send_target_move()

def move_right():
    tool_pos.x += linear_speed
    send_target_move()

def move_left():
    tool_pos.x -= linear_speed
    send_target_move()


# Starts the cutting process using parsed DXF commands
def func_print():
    if hasattr(window, "dxf_preview"):
        print("Starting cut")
        cut = DxfParser("export_robot.dxf")
        commands = cut.parse()
        cut.print_preview(commands)
        worker.stream_commands(commands)


# Emergency stop
def func_stop():
    worker.trigger_emergency_stop()
    print("Cut stopped (STOP command sent)")


# Automatically fits the scene inside the QGraphicsView
class AutoFitView(QObject):
    def __init__(self, window, view, scene):
        super().__init__()
        self.window = window
        self.view = view
        self.scene = scene
        self.defer = False

        # Initial fit after UI loads
        QTimer.singleShot(0, self.refit)

    def schedule_refit(self):
        # Avoid repeated refits during resize
        if not self.defer:
            self.defer = True
            QTimer.singleShot(0, self._do_refit)

    def _do_refit(self):
        self.defer = False
        self.refit()

    def refit(self):
        # Expands bounding rect slightly for padding
        rect = self.scene.itemsBoundingRect().adjusted(-40, -40, 40, 40)
        if not rect.isNull():
            self.view.fitInView(rect, Qt.AspectRatioMode.KeepAspectRatio)

    def eventFilter(self, obj, event):
        # Immediate refit on resize
        if obj is self.view and event.type() == QEvent.Type.Resize:
            self.refit()

        # Delayed refit on maximize/restore
        if obj is self.window and event.type() == QEvent.Type.WindowStateChange:
            QTimer.singleShot(0, self.refit)

        return False


# Custom QGraphicsView that supports click-to-move
class ClickableGraphicsView(QGraphicsView):
    def mousePressEvent(self, event):
        global click_move_enabled, tool_pos

        # Convert click position to scene coordinates
        view_pos = event.position()
        scene_pos = self.mapToScene(int(view_pos.x()), int(view_pos.y()))

        centered_x = scene_pos.x() - origin.x
        centered_y = scene_pos.y() - origin.y - 10

        print("Centered coords:", centered_x, centered_y)

        # If click-to-move enabled → send robot move command
        if click_move_enabled:
            tool_pos.x = centered_x
            tool_pos.y = centered_y
            send_target_move()
            print(f"Moving to ({centered_x:.1f}, {centered_y:.1f})")

        super().mousePressEvent(event)


# Connects to robot using IP from UI
def connect():
    ip = window.ipEdit.text().strip()
    print("IP entered:", ip)
    print(f"Connecting to {ip}...")
    worker.connect_robot(ip)


# Disconnects from robot
def disconnect():
    worker.disconnect_robot()
    print("Disconnect requested...")
    window.connectPanelBtn.setChecked(False)
    window.connectPanelBtn.setIcon(QIcon(":/icons/icons/wifi-off.svg"))


# Toggles click-to-move mode
def toggle_click_move():
    global click_move_enabled
    click_move_enabled = not click_move_enabled
    state = "enabled" if click_move_enabled else "disabled"
    print(f"Click move {state}")


# Callback when robot connection succeeds or fails
def on_connected(success):
    if success:
        window.connectPanelBtn.setChecked(True)
        window.connectPanelBtn.setIcon(QIcon(":/icons/icons/wifi.svg"))
        print("Connected to ESP32 successfully!")
    else:
        window.connectPanelBtn.setChecked(False)
        window.connectPanelBtn.setIcon(QIcon(":/icons/icons/wifi-off.svg"))
        print("Connection error.")


# Callback when robot sends status updates
def on_status_received(data):
    global tool_raised

    # Update digital twin angles
    if 'theta1' in data:
        animator.setAngle(-data['theta1'])
    if 'theta2' in data:
        animator_elbow.setAngle(-data['theta2'])

    # Update real effector coordinates
    if 'x' in data and 'y' in data:
        x_reel = data['x']
        y_reel = data['y']
        text = f"Real effector: ({x_reel:.1f}, {y_reel:.1f})"

        # If robot is idle, sync local target to avoid jumps
        if not data.get('isMoving', False):
            tool_pos.x = x_reel
            tool_pos.y = -y_reel

        print(text)

    # Update tool state
    if 'tool' in data:
        tool_raised = not data['tool']


# Error callback from robot worker
def on_error(err_msg):
    print(f"⚠️ WS Error: {err_msg}")


# Toggles visibility of connection panel
def toggle_connection():
    connection_panel.setHidden(not connection_panel.isHidden())
    auto_fit.schedule_refit()


# Toggles visibility of control panel
def toggle_control():
    control_panel.setHidden(not control_panel.isHidden())
    auto_fit.schedule_refit()


# Sends a predefined manual command sequence
def manual():
    cmd = [
        {"type": "SET_HOME"},
        {"type": "DELAY", "ms": 5000},
        {"type": "SAVE_HOME"},
        {"type": "HOME"},
    ]
    worker.stream_commands(cmd)


# Enables dragging the window by clicking the custom title bar
class TitleBarDrag(QObject):
    def __init__(self, window, header):
        super().__init__()
        self.window = window
        self.header = header

        header.installEventFilter(self)
        for child in header.findChildren(QObject):
            child.installEventFilter(self)

    def eventFilter(self, obj, event):
        # Start window drag when clicking header or its children
        if obj is self.header or obj.parent() is self.header:
            if event.type() == QEvent.Type.MouseButtonPress:
                if event.button() == Qt.MouseButton.LeftButton:
                    if self.window.windowHandle():
                        self.window.windowHandle().startSystemMove()
                return True
        return False


# Maximizes or restores the window
def toggle_maximize():
    if window.isMaximized():
        window.showNormal()
    else:
        window.showMaximized()


# Connects all UI buttons to their respective functions
def connect_buttons():
    window.connectPanelBtn.clicked.connect(toggle_connection)
    window.downloadBtn.clicked.connect(open_file)
    window.deleteBtn.clicked.connect(close_file)
    window.dxfBtn.clicked.connect(func_DXF)
    window.startBtn.clicked.connect(func_print)
    window.stopBtn.clicked.connect(func_stop)
    window.helpBtn.clicked.connect(open_help)

    window.connectBtn.clicked.connect(connect)
    window.disconnectBtn.clicked.connect(disconnect)

    window.upBtn.clicked.connect(move_forward)
    window.downBtn.clicked.connect(move_backward)
    window.rightBtn.clicked.connect(move_right)
    window.leftBtn.clicked.connect(move_left)

    window.toolUpBtn.clicked.connect(tool_up)
    window.toolDownBtn.clicked.connect(tool_down)

    window.closeBtn.clicked.connect(window.close)
    window.minBtn.clicked.connect(window.showMinimized)
    window.maxBtn.clicked.connect(toggle_maximize)

    window.clickMoveBtn.clicked.connect(toggle_click_move)
    window.jogBtn.clicked.connect(toggle_control)
    window.homeBtn.clicked.connect(go_home)
    window.manualBtn.clicked.connect(manual)


# Connects QLineEdit fields to speed update functions
def connect_line_edits():
    window.linSpeedEdit.textChanged.connect(change_linear_speed)


# -------------------------
# Main application startup
# -------------------------
if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)

    window = QtWidgets.QMainWindow()
    uic.loadUi('plasmarm_v2.ui', window)

    # Frameless window for custom title bar
    window.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.Window)

    # Setup connection button icon
    window.connectPanelBtn.setCheckable(True)
    window.connectPanelBtn.setIconSize(QtCore.QSize(24, 24))
    window.connectPanelBtn.setIcon(QIcon(":/icons/icons/wifi-off.svg"))

    # Replace default QGraphicsView with clickable subclass
    old_view = window.graphicsView
    click_view = ClickableGraphicsView(old_view.parent())
    click_view.setObjectName(old_view.objectName())

    layout = old_view.parent().layout()
    if layout is not None:
        layout.replaceWidget(old_view, click_view)

    old_view.deleteLater()
    window.graphicsView = click_view

    # Scene setup for robot animation
    scene = QGraphicsScene()
    shoulder_img = QPixmap("bras1.png")
    elbow_img = QPixmap("bras2.png")
    shoulder = QGraphicsPixmapItem(shoulder_img)
    elbow = QGraphicsPixmapItem(elbow_img)

    animator = animation.AngleAnimator(shoulder)
    animator_elbow = animation.AngleAnimator(elbow)

    animation.generate_scene(window, scene, bicep, forearm, origin, elbow, shoulder)

    # Auto-fit behavior
    auto_fit = AutoFitView(window, window.graphicsView, scene)
    window.graphicsView.installEventFilter(auto_fit)
    window.installEventFilter(auto_fit)

    # Panels hidden by default
    connection_panel = window.ConnectionPanel
    connection_panel.setHidden(True)

    control_panel = window.ControlContainer
    control_panel.setHidden(True)

    # Start robot worker thread
    worker = comm.RobotWorker()
    worker.connected_signal.connect(on_connected)
    worker.status_received_signal.connect(on_status_received)
    worker.error_signal.connect(on_error)
    worker.start()

    # UI initialization
    window.graphicsView.setDragMode(QtWidgets.QGraphicsView.DragMode.NoDrag)
    enforce_float_only(window.linSpeedEdit)

    titlebar_drag = TitleBarDrag(window, window.headerContainer)

    connect_buttons()
    connect_line_edits()

    window.show()
    exit_code = app.exec()

    # Clean shutdown
    worker.stop()
    worker.quit()
    worker.wait()
    sys.exit(exit_code)
