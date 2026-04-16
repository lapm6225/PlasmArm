from PyQt6.QtWidgets import QGraphicsEllipseItem
from PyQt6.QtCore import QObject, pyqtProperty, Qt
from PyQt6.QtGui import QPen


# ============================================================
#  AngleAnimator — Animates rotation of a QGraphicsItem
# ============================================================
class AngleAnimator(QObject):
    """
    A small helper class that exposes a Qt property 'angle'
    so it can be animated using QPropertyAnimation.

    It simply forwards the angle value to the QGraphicsItem's rotation.
    """

    def __init__(self, graphics_item):
        super().__init__()
        self.item = graphics_item
        self._angle = 0

    def getAngle(self):
        """Return the current angle value."""
        return self._angle

    def setAngle(self, value):
        """
        Update the internal angle and apply the rotation
        to the associated QGraphicsItem.
        """
        self._angle = value
        self.item.setRotation(value)

    # Expose angle as a Qt property so QPropertyAnimation can target it
    angle = pyqtProperty(float, getAngle, setAngle)


# ============================================================
#  generate_scene — Build the robot arm scene
# ============================================================
def generate_scene(window, scene, bicep, forearm, origin, elbow, shoulder):
    """
    Build and configure the graphical scene for the robot arm.

    This includes:
    - Setting up the QGraphicsView
    - Placing and scaling the shoulder and elbow pixmaps
    - Defining rotation origins
    - Drawing reach arcs (max, min, and side workspace)
    """

    # Attach the scene to the view
    window.graphicsView.setScene(scene)
    window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignCenter)
    scene.setSceneRect(0, 0, 900, 450)

    # ------------------------------------------------------------
    # 1. SHOULDER SEGMENT (first arm link)
    # ------------------------------------------------------------
    scene.addItem(shoulder)

    # Scale the shoulder image to match your robot proportions
    shoulder.setScale(1 / 7.2)

    # Set the rotation pivot (center of the shoulder joint)
    shoulder.setTransformOriginPoint(
        121,
        shoulder.pixmap().height() / 2
    )

    # Position the shoulder at the robot's origin
    shoulder.setPos(origin.x - 120, origin.y - 116)

    # ------------------------------------------------------------
    # 2. ELBOW SEGMENT (second arm link)
    # ------------------------------------------------------------
    elbow.setParentItem(shoulder)  # elbow rotates with the shoulder
    elbow.setScale(7.2 / 6.4)

    # Rotation pivot for the elbow joint
    elbow.setTransformOriginPoint(
        95,
        (elbow.pixmap().height() / 2) - 75
    )

    # Position relative to the shoulder image
    elbow.setPos(shoulder.pixmap().width() - 225, 30)

    # ------------------------------------------------------------
    # 3. WORKSPACE ARCS (visual reach boundaries)
    # ------------------------------------------------------------

    # Maximum reach radius (bicep + forearm)
    R = bicep.length + forearm.length

    # --- Max reach arc (outer boundary) ---
    arc_max = QGraphicsEllipseItem(
        origin.x - R,
        origin.y - R + bicep.width / 3.5,
        2 * R,
        2 * R
    )
    scene.addItem(arc_max)
    arc_max.setStartAngle(0)
    arc_max.setSpanAngle(180 * 16)  # Qt uses 1/16° units
    arc_max.setPen(QPen(Qt.GlobalColor.black, 2))
    arc_max.setZValue(1000)

    # --- Min reach arc (inner boundary) ---
    arc_min = QGraphicsEllipseItem(
        origin.x - 95,
        origin.y - 95 + 10,
        2 * 95,
        2 * 95
    )
    scene.addItem(arc_min)
    arc_min.setStartAngle(0)
    arc_min.setSpanAngle(180 * 16)
    arc_min.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_min.setZValue(1000)

    # --- Side workspace arc (custom constraint) ---
    arc_sw = QGraphicsEllipseItem(
        origin.x - R,
        origin.y - R / 2 + 10,
        2 * 214,
        2 * 214
    )
    scene.addItem(arc_sw)
    arc_sw.setStartAngle(0)
    arc_sw.setSpanAngle(180 * 16)
    arc_sw.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_sw.setZValue(1000)

    # Update scene rect to include all items
    scene.setSceneRect(scene.itemsBoundingRect())
