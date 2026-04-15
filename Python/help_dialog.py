from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QPushButton
from PyQt6.QtGui import QPixmap

from collections import namedtuple

import sys
import math

class HelpDialog(QDialog):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Help")
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
        <h2>Help Menu</h2>
        <p>Usefull informations :</p>
        <ul>
            <li>Use the <b>Connection & status</b> to open a connection menu.</li>
            <li>Use the  <b>Load DXF</b> button to import a drawing.</li>
            <ul>
                <li>Move the drawing within the red limits.</li>
            </ul>
            <li>Press on <b>Generate</b> once the drawing is where you desire.</li>
            <ul>
                <li>The drawing becomes green if it is in a valid position</li>
            </ul>
            <li>Use the <b>Delete</b> button to delete the drawing.</li>
            <li>Press on <b>Play</b> to start the printing process.</li>
            <li>Press on <b>Play</b> to start the printing process.</li>
            <li>Press on the <b>Stop</b> and cancel button to completely stop the print.</li>

            
            <li>Press on the <b>Control</b> button to open a control menu.</li>
            <ul>
                <li><b>Tool controls</b></li>
                <li><b>Linear Controls</b></li>
                <li><b>Angular conrols</b></li>
            </ul>
            <li>Press on the <b>Home</b> button to return the arm to its initial position.</li>
            <li>Press on the <b>M</b> button to manualy move the arm. Useful to calibrate the arm.</li>

        </ul>
        """

        label = QLabel(help_text)
        label.setWordWrap(True)

        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.close)

        layout.addWidget(label)
        layout.addWidget(close_btn)
        self.setLayout(layout)