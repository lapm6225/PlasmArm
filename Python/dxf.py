from PyQt6 import QtWidgets
from PyQt6.QtWidgets import QGraphicsLineItem, QGraphicsItem, QFileDialog 
from PyQt6.QtCore import  Qt, QPointF
from PyQt6.QtGui import QPen 
import ezdxf
import os
import check_dxf
# --- Ajout de ligne dans le "debug" ---
def add_text(window,text):
    # window.textEdit.append(text)
    # text = window.textEdit.toPlainText()
    # lines = text.split("\n")
    # new_lines = lines[-20:]
    # trimmed_text = "\n".join(new_lines)
    # window.textEdit.setPlainText(trimmed_text)
    print(text)


# --- Qui génère un DXF centré ---
def func_DXF(window, scene):
    doc = ezdxf.new()
    msp = doc.modelspace()
    origin_bras = QPointF(550, 450)
    if hasattr(window, "dxf_preview"):
        scene.removeItem(window.dxf_preview)
    window.dxf_preview = scene.createItemGroup([])
    pen = QPen(Qt.GlobalColor.green)
    pen.setWidth(1)
    if hasattr(window, "dxf_group"):
        for item in window.dxf_group.childItems():
            if isinstance(item, QGraphicsLineItem):
                # --- Création des segments
                line = item.line()
                p1 = item.mapToScene(line.p1())
                p2 = item.mapToScene(line.p2())

                # Convertir dans référentiel bras
                p1_robot = p1 - origin_bras
                p2_robot = p2 - origin_bras               
                msp.add_line((p1_robot.x(), -p1_robot.y()), (p2_robot.x(), -p2_robot.y()))
                x1, y1 = p1_robot.x(), p1_robot.y()
                x2, y2 = p2_robot.x(), p2_robot.y()
                line = QGraphicsLineItem(550+x1, 450+y1, 550+x2, 450+y2)
                line.setPen(pen)
                line.setZValue(-10)  # derrière
                line.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable, False)
                line.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsMovable, False)
                window.dxf_preview.addToGroup(line)
        doc.saveas("export_robot.dxf")
        if check_dxf.check() == False:
            add_text(window, "Out of Bounds")
            scene.removeItem(window.dxf_preview)
            del window.dxf_preview
        else :
            add_text(window, "DXF file generated")
            scene.removeItem(window.dxf_group)        

    # --- Execption si aucun DXF n'est chargé ---
    else:
        QtWidgets.QMessageBox.warning(window, "Erreur", "Aucun DXF à imprimer")
# --------------------------------------------


# --- Ouvertur d'un fichier ---
def open_file(window, scene):

    # --- Style dark pour les boîtes de dialogue ---
    dialog_style = """
        QWidget {
            background-color: #20242c;
            color: #e6f1ff;
        }
        QLineEdit {
            background-color: #262b34;
            color: #e6f1ff;
            border: 1px solid #2f3540;
            border-radius: 4px;
            padding: 4px 6px;
        }
        QPushButton {
            background-color: #262b34;
            color: #e6f1ff;
            border: 1px solid #2f3540;
            border-radius: 4px;
            padding: 4px 10px;
        }
        QPushButton:hover {
            background-color: #2b303a;
            border-color: #3a4150;
        }
    """

    # --- Si un DXF est déjà chargé ---
    if hasattr(window, "dxf_group"):

        box = QtWidgets.QMessageBox(window)
        box.setWindowTitle("DXF file already loaded")
        box.setText("A DXF file is already loaded. Do-you want to replace it ?")
        box.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Yes |
                               QtWidgets.QMessageBox.StandardButton.No)
        box.setStyleSheet(dialog_style)

        réponse = box.exec()

        if réponse == QtWidgets.QMessageBox.StandardButton.No:
            return

        close_file(window, scene)

    # --- Ouvrir un fichier DXF ---
    dialog = QFileDialog(window)
    dialog.setWindowTitle("Ouvrir un fichier DXF")
    dialog.setNameFilter("Fichiers DXF (*.dxf)")
    dialog.setStyleSheet(dialog_style)

    if dialog.exec():
        filename = dialog.selectedFiles()[0]

        print("Fichier choisi :", filename)
        add_text(window, os.path.basename(filename))
        window.dxf_group = load_dxf_into_scene(window, scene, window.graphicsView, filename)
#-------------------------------------------------------------

# --- Fermer le fichier ---
def close_file(window,scene):
    # vérifier si un fichier est ouvert ou non
    if hasattr(window, "dxf_group"):
        if window.dxf_group.scene() is not None:
            scene.removeItem(window.dxf_group)
        del window.dxf_group
        if hasattr(window, "dxf_preview"):
            scene.removeItem(window.dxf_preview)
            del window.dxf_preview
        #window.textEdit.append("File closed")
        add_text(window, "File closed")
    # Cas sans fichier ouvert
    else:
        QtWidgets.QMessageBox.warning(window, "Erreur", "Aucun DXF à fermer.")
# --------------------------

# --- DXF loader ---
def load_dxf_into_scene(window, scene, view, filename):
    # Initialization
    doc = ezdxf.readfile(filename)
    msp = doc.modelspace()
    pen = QPen(Qt.GlobalColor.black)
    pen.setWidth(1)
    count = 0

    # --- Groupe DXF déplaçable ---
    window.dxf_group = scene.createItemGroup([])

    for entity in msp:
        #print("TYPE DXF:", entity.dxftype())
        if entity.dxftype() == "SPLINE":

            # Reconstruction propre des spline
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
        # Reconstruction de lignes
        if entity.dxftype() == "LINE":
            x1, y1, _ = entity.dxf.start
            x2, y2, _ = entity.dxf.end

            line = QGraphicsLineItem(x1, -y1, x2, -y2)
            line.setPen(pen)
            window.dxf_group.addToGroup(line)
            count += 1
        # Reconstruction de polyline
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

    # Point de centrage
    scene_center_x = 550
    scene_center_y = 200

    # Centre du DXF
    dxf_center_x = bbox.x() + bbox.width() / 2
    dxf_center_y = bbox.y() + bbox.height() / 2

    # Centrage du centre du DXF
    dx = scene_center_x - dxf_center_x
    dy = scene_center_y - dxf_center_y

    # Déplacement
    window.dxf_group.moveBy(dx, dy)

    return window.dxf_group
# --------------------------------------------
