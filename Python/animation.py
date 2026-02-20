from PyQt6 import QtWidgets, uic
from PyQt6.QtWidgets import QGraphicsScene, QGraphicsRectItem, QGraphicsEllipseItem, QGraphicsLineItem, QGraphicsItem, QLineEdit, QFileDialog 
from PyQt6.QtCore import QObject, pyqtProperty, QPropertyAnimation, QRectF, Qt, QPointF
from PyQt6.QtGui import QPen 
import ezdxf
import os
import sys
import math
from collections import namedtuple
from pathlib import Path
import dxf

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
    window.graphicsView.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignTop)
    scene.setSceneRect(0, 0, 800, 450)

    # --- Épaule ---
    
    scene.addItem(shoulder)
    shoulder.setTransformOriginPoint(0, bicep.width/2)
    shoulder.setPos(origin.x, origin.y)

    # --- Coude ---
    
    elbow.setParentItem(shoulder)
    elbow.setTransformOriginPoint(0, forearm.width/2)
    elbow.setPos(bicep.length, 0)

    # --- Affichage porté max ---
    arc_max = QGraphicsEllipseItem( (origin.x-bicep.length-forearm.length), (origin.y-bicep.length-forearm.length+bicep.width/2), 
                                    2*(bicep.length+forearm.length), 2*(bicep.length+forearm.length))
    scene.addItem(arc_max)
    arc_max.setStartAngle(0)
    arc_max.setSpanAngle(180 * 16)
    arc_max.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_max.setZValue(1000)

    # --- Affichage porté min ---
    arc_min = QGraphicsEllipseItem(280, 290, 240, 240)
    scene.addItem(arc_min)
    arc_min.setStartAngle(0)
    arc_min.setSpanAngle(180 * 16)
    arc_min.setPen(QPen(Qt.GlobalColor.red, 2))
    arc_min.setZValue(1000)