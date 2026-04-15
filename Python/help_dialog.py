from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QPushButton
from PyQt6.QtGui import QPixmap
from PyQt6.QtCore import  Qt
from PyQt6 import QtWidgets
from collections import namedtuple

import sys
import math

class HelpDialog(QDialog):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Help")
        self.setMinimumWidth(400)
        self.setSizePolicy(QtWidgets.QSizePolicy.Policy.Fixed,
                   QtWidgets.QSizePolicy.Policy.Fixed)
        self.adjustSize()
        self.setFixedHeight(self.height())

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
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)



        help_text = """
        <h2>Help Menu</h2>
    <p>Useful information:</p>
    <ul>
        <li>Use the <b>Connection & Status</b> button to open the connection panel.</li>

        <li>Use the <b>Load DXF</b> button to import a drawing.</li>
        <ul>
            <li>Move the drawing within the red boundaries.</li>
        </ul>
        <li>Press <b>Generate</b> once the drawing is positioned as desired.</li>
        <ul>
            <li>The drawing turns green when it is in a valid position.</li>
        </ul>
        <li>Use the <b>Delete</b> button to remove the loaded drawing.</li>

        <li>Press <b>Play</b> to start the cutting process.</li>
        <li>Press <b>Pause</b> to temporarily pause the cutting process.</li>
        <li>Press the <b>Stop</b> button to completely stop and cancel the cut.</li>

        <li>Press the <b>Control</b> button to open the control panel.</li>
        <ul>
            <li><b>Tool Controls</b></li>
            <li><b>Linear Controls</b></li>
            <li><b>Angular Controls</b></li>
        </ul>
        <li>When the <b>Pen</b> button is toggled, click inside the red area to send the tool to that position.</li>
        <li>Press the <b>Home</b> button to return the arm to its initial position.</li>
        <li>Press the <b>M</b> button to manually move the arm. Useful for calibration.</li>
        <li>Press the <b>Anchor</b> button to define the calibration point while the arm is at its home position. See the GitHub for proper orientation.</li>
    </ul>

        """

        label = QLabel(help_text)
        label.setWordWrap(True)

        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.close)

        layout.addWidget(label)
        layout.addWidget(close_btn)
        self.setLayout(layout)