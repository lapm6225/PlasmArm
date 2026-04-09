from PyQt6.QtWidgets import QGraphicsEllipseItem, QGraphicsPixmapItem
from PyQt6.QtCore import QObject, pyqtProperty, Qt
from PyQt6.QtGui import QPen, QPixmap
 

# --- animateur qui dessine les bras ---
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


def generate_scene(window, scene, bicep, forearm, origin, elbow, shoulder):
    window.graphicsView.setScene(scene)
    window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignCenter)
    scene.setSceneRect(0, 0, 900, 450)

    # --- Segment 1 : épaule ---
    scene.addItem(shoulder)
    shoulder.setScale(1/7.2)
    shoulder.setTransformOriginPoint(121, shoulder.pixmap().height() / 2)
    shoulder.setPos(origin.x-120, origin.y-116)

    # --- Segment 2 : coude ---
    elbow.setParentItem(shoulder)
    elbow.setScale(7.2/6.4)
    elbow.setTransformOriginPoint(95, (elbow.pixmap().height() / 2)-75)
    elbow.setPos(shoulder.pixmap().width()-225, 30)


    # --- Arcs de portée (inchangés) ---
    R = bicep.length + forearm.length

    arc_max = QGraphicsEllipseItem(
        origin.x - R,
        origin.y - R + bicep.width/3.5,
        2*R,
        2*R
    )
    scene.addItem(arc_max)
    arc_max.setStartAngle(0)
    arc_max.setSpanAngle(180 * 16)
    arc_max.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_max.setZValue(1000)

    arc_min = QGraphicsEllipseItem(origin.x-95, origin.y-95+10, 2*95, 2*95)
    scene.addItem(arc_min)
    arc_min.setStartAngle(0)
    arc_min.setSpanAngle(180 * 16)
    arc_min.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_min.setZValue(1000)
    scene.setSceneRect(scene.itemsBoundingRect())

# # --- Définition des dimension et des éléments dans l'affichage ---
# def generate_scene(window, scene, bicep, forearm, origin, elbow, shoulder):
#     # --- Définition de la zone d'affichage
#     window.graphicsView.setScene(scene)
#     window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
#     scene.setSceneRect(0, 0, 800, 450)


#     # --- Épaule ---  
#     scene.addItem(shoulder)
#     shoulder.setTransformOriginPoint(0, bicep.width/2) # position de lorigine sur l'objet
#     shoulder.setPos(origin.x, origin.y) # déplacement

#     # --- Coude ---
#     elbow.setParentItem(shoulder)
#     elbow.setTransformOriginPoint(0, forearm.width/2) # position de lorigine sur l'objet
#     elbow.setPos(bicep.length, 0) # déplacement

#     # --- Affichage porté max ---
#     arc_max = QGraphicsEllipseItem( (origin.x-bicep.length-forearm.length), (origin.y-bicep.length-forearm.length+bicep.width/2), 
#                                     2*(bicep.length+forearm.length), 2*(bicep.length+forearm.length)) #(pos x, pos y, taille en x, taille en y)
#     scene.addItem(arc_max)
#     arc_max.setStartAngle(0)
#     arc_max.setSpanAngle(180 * 16) # angle désiré en 1/16 de degrés
#     arc_max.setPen(QPen(Qt.GlobalColor.red, 2)) # dessiner en rouge
#     arc_max.setZValue(1000) # Définir la hauteur(avant-plan/arrière-plan)

#     # --- Affichage porté min ---
#     arc_min = QGraphicsEllipseItem(329, 339, 142, 142) #(pos x, pos y, taille en x, taille en y)
#     scene.addItem(arc_min)
#     arc_min.setStartAngle(0)
#     arc_min.setSpanAngle(180 * 16) # angle désiré en 1/16 de degrés
#     arc_min.setPen(QPen(Qt.GlobalColor.red, 2)) # dessiner en rouge
#     arc_min.setZValue(1000) # Définir la hauteur(avant-plan/arrière-plan)