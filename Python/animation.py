from PyQt6.QtWidgets import QGraphicsEllipseItem
from PyQt6.QtCore import QObject, pyqtProperty, Qt
from PyQt6.QtGui import QPen 

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



# --- Définition des dimension et des éléments dans l'affichage ---
def generate_scene(window, scene, bicep, forearm, origin, elbow, shoulder):
    # --- Définition de la zone d'affichage
    window.graphicsView.setScene(scene)
    window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
    scene.setSceneRect(0, 0, 800, 450)


    # --- Épaule ---  
    scene.addItem(shoulder)
    shoulder.setTransformOriginPoint(0, bicep.width/2) # position de lorigine sur l'objet
    shoulder.setPos(origin.x, origin.y) # déplacement

    # --- Coude ---
    elbow.setParentItem(shoulder)
    elbow.setTransformOriginPoint(0, forearm.width/2) # position de lorigine sur l'objet
    elbow.setPos(bicep.length, 0) # déplacement

    # --- Affichage porté max ---
    arc_max = QGraphicsEllipseItem( (origin.x-bicep.length-forearm.length), (origin.y-bicep.length-forearm.length+bicep.width/2), 
                                    2*(bicep.length+forearm.length), 2*(bicep.length+forearm.length)) #(pos x, pos y, taille en x, taille en y)
    scene.addItem(arc_max)
    arc_max.setStartAngle(0)
    arc_max.setSpanAngle(180 * 16) # angle désiré en 1/16 de degrés
    arc_max.setPen(QPen(Qt.GlobalColor.red, 2)) # dessiner en rouge
    arc_max.setZValue(1000) # Définir la hauteur(avant-plan/arrière-plan)

    # --- Affichage porté min ---
    arc_min = QGraphicsEllipseItem(329, 339, 142, 142) #(pos x, pos y, taille en x, taille en y)
    scene.addItem(arc_min)
    arc_min.setStartAngle(0)
    arc_min.setSpanAngle(180 * 16) # angle désiré en 1/16 de degrés
    arc_min.setPen(QPen(Qt.GlobalColor.red, 2)) # dessiner en rouge
    arc_min.setZValue(1000) # Définir la hauteur(avant-plan/arrière-plan)
