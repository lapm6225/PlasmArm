from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QPushButton
from PyQt6.QtGui import QPixmap
from PyQt6.QtCore import Qt
from PyQt6 import QtWidgets




class HelpDialog(QDialog):
    def __init__(self):
        super().__init__()

        # --- Window configuration ---
        self.setWindowTitle("Help")                     # Title of the dialog window
        self.setMinimumWidth(400)                       # Prevents the dialog from being too narrow

        # Prevents the dialog from expanding vertically beyond its content
        self.setSizePolicy(QtWidgets.QSizePolicy.Policy.Fixed,
                           QtWidgets.QSizePolicy.Policy.Fixed)
        self.adjustSize()                               # Adjusts size to fit content
        self.setFixedHeight(self.height())              # Locks the height after adjustment

        # --- Dark theme styling for the dialog and widgets ---
        self.setStyleSheet("""
            QDialog {
                background-color: #111;                 /* Dark background */
                color: #eee;                            /* Light text */
            }
            QLabel {
                color: #eee;                            /* Ensures label text is readable */
            }
            QPushButton {
                background-color: #333;                 /* Dark button background */
                color: #eee;                            /* Light button text */
                border: 1px solid #555;                 /* Subtle border */
                padding: 6px;
                border-radius: 4px;                     /* Rounded corners */
            }
            QPushButton:hover {
                background-color: #444;                 /* Slightly lighter on hover */
            }
        """)

        # --- Main vertical layout ---
        layout = QVBoxLayout()
        layout.setAlignment(Qt.AlignmentFlag.AlignTop)   # Ensures content stays at the top

        # --- HTML help text displayed inside a QLabel ---
        # Note: QLabel supports a subset of HTML (Qt Rich Text)
        help_text = """
        <h2>Help Menu</h2>
        <p>Useful information:</p>

        <ul>
            <li>Use the <b>Connection & Status</b> button to open the connection panel.</li>

            <li>
                Use the <b>Load DXF</b> button to import a drawing.
                <ul>
                    <li>Move the drawing within the red boundaries.</li>
                </ul>
            </li>

            <li>
                Press <b>Generate</b> once the drawing is positioned as desired.
                <ul>
                    <li>The drawing turns green when it is in a valid position.</li>
                </ul>
            </li>

            <li>Use the <b>Delete</b> button to remove the loaded drawing.</li>

            <li>Press <b>Play</b> to start the cutting process.</li>
            <li>Press <b>Pause</b> to temporarily pause the cutting process.</li>
            <li>Press the <b>Stop</b> button to completely stop and cancel the cut.</li>

            <li>
                Press the <b>Control</b> button to open the control panel.
                <ul>
                    <li><b>Tool Controls</b></li>
                    <li><b>Linear Controls</b></li>
                    <li><b>Angular Controls</b></li>
                </ul>
            </li>

            <li>When the <b>Pen</b> button is toggled, click inside the red area to send the tool to that position.</li>
            <li>Press the <b>Home</b> button to return the arm to its initial position.</li>
            <li>Press the <b>M</b> button to manually move the arm. Useful for calibration.</li>
            <li>Press the <b>Anchor</b> button to define the calibration point while the arm is at its home position. See the GitHub for proper orientation.</li>
        </ul>
        """

        # QLabel used to render the HTML help text
        label = QLabel(help_text)
        label.setWordWrap(True)                         # Allows text to wrap inside the dialog

        # --- Close button ---
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.close)           # Closes the dialog when clicked

        # --- Add widgets to layout ---
        layout.addWidget(label)
        layout.addWidget(close_btn)

        # Apply layout to the dialog
        self.setLayout(layout)
