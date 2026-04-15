from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QPushButton
from PyQt6.QtGui import QPixmap

from collections import namedtuple

import sys
import math

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