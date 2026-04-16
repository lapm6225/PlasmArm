# PlasmArm

A 3-DOF (X, Y, Z) SCARA robotic arm controlled by an ESP32 with real-time WebSocket interface. Designed for compact plasma cutting applications in small workshops.

![PlasmArm Robot](Mechanic/Complete%20Robot%20Assembly%20Drawing.png)

## Table of Contents

- [Overview](#overview)
- [Project Context](#project-context)
- [Electrical](#Electrical)
  - [Electrical Components](#Electrical-Components)
  - [Bill of Materials](#Bill-of-Materials)
  - [Wiring Diagram](#Wiring-Diagram)
- [Mechanical](#Mechanical)
  - [BoM & Drawings](#BoM-and-Drawings)
  - [Mechanical Project Files](#Mechanical-Project-Files)
  - [Assembly Instructions](#Assembly-Instructions)
- [Software](#Software)
  - [Software Installation](#software-installation)
    - [Prerequisites](#prerequisites)
    - [PlatformIO Setup](#platformio-setup)
    - [Python Client Setup](#python-client-setup)
  - [Configuration](#configuration)
  - [Building and Uploading](#building-and-uploading)
  - [Running the Robot](#running-the-robot)
  - [Architecture Overview](#architecture-overview)
    - [System Modules](#system-modules)
    - [FreeRTOS Task Structure](#freertos-task-structure)
    - [Kinematics](#kinematics)
    - [Trajectory Planning](#trajectory-planning)
  - [Web Interface](#web-interface)
  - [Testing](#testing)
  - [Licenses](#licenses)
  - [Troubleshooting](#troubleshooting)
  - [Project Structure](#project-structure)

---

## Overview

PlasmArm is a proof-of-concept for a compact, wall-mounted SCARA robot designed for plasma cutting in small workshops. The robot folds when inactive to save floor space.

### Key Features

- **3-DOF Architecture**: X, Y positioning (2 joints) + Z axis (tool actuation)
- **ESP32** microcontroller with FreeRTOS for real-time dual-core task scheduling
- **Dynamixel XM430** servomotors for shoulder and elbow joints
- **SG90 servo** with rack-and-pinion mechanism for Z-axis
- **Limit switch** for tool contact detection
- **WebSocket** interface for real-time control via web browser
- **DXF import** support for vector drawings
- **PyQt6** graphical user interface

### Current Status

This is a **proof-of-concept** version at reduced scale where the plasma torch and metal sheet are replaced by a pencil and paper for safe testing and validation.

---

## Project Context

### Motivation

PlasmArm emerged from the need for space-saving solutions in small machining workshops. Traditional CNC plasma tables require significant floor space. This project proposes a compact alternative - a wall or floor-mounted SCARA robot that folds when inactive to free up workspace.

### Development Method

Agile methodology with incremental feature delivery throughout the project.

---

## Electrical

### Electrical Components

- **ESP32** (esp32doit-devkit-v1 or compatible)
- USB cable for programming

### Motors & Actuators

| Motor | Model | Function |
|-------|-------|----------|
| Shoulder | Dynamixel XM430-W350 | Base joint (X-Y positioning) |
| Elbow | Dynamixel XM430-W350 | Second joint (X-Y positioning) |
| Z-Axis | SG90 (TowerPro) | Tool up/down actuation |
| End Effector | - | Pencil/pen holder (proof of concept) |

### Power Supply

- **12V DC** power supply (90W, 7.5A)
- 12V to 5V buck converter for ESP32 and SG90

### Electronics

- Level shifter (5V to 3.3V)
- Custom PCB (fit in the base)
- Limit switch (normally open) for Z-axis contact detection
- 10kΩ pull-up resistor for limit switch
- Diode (1N4148 or similar) for half-duplex UART

---

### Bill of Materials

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 Dev Kit | 1 | esp32doit-devkit-v1 |
| Dynamixel XM430-W350 | 2 | Shoulder and elbow joints |
| SG90 Servo | 1 | Z-axis actuation |
| 12V Power Supply | 1 | 90W, 7.5A |
| Buck Converter | 1 | 12V to 5V |
| Level Shifter | 1 | 5V to 3.3V |
| Limit Switch (NO) | 1 | Z-axis contact detection |
| 10kΩ Resistor | 1 | Pull-up for limit switch |
| 1N4148 Diode | 1 | Half-duplex UART |
| Electrolytic Capacitor | 1 | 10uF |
| Ceramic Capacitor | 3 | 10nF |

### ESP32 Pin Connections

| Function | GPIO Pin | Notes |
|----------|----------|-------|
| Dynamixel RX | 16 | Serial2 RX |
| Dynamixel TX | 17 | Serial2 TX (with diode for half-duplex) |
| SG90 Servo (Z-axis) | 26 | PWM output |
| Limit Switch | 32 | Input with 10kΩ pull-up |
| 5V Power | - | From buck converter |
| 3.3V Reference | - | From ESP32 |

### Wiring Diagram

![Wiring](Electric/Electrical%20Wiring%20Diagram.png)

---

### Connection Details

1. **Dynamixel Motors**: Daisy-chained, communicate via UART through level shifter to ESP32 pins 16 (RX) and 17 (TX)
2. **SG90**: PWM signal from GPIO 26 through level shifter
3. **Limit Switch**: GPIO 32 with 10kΩ pull-up to 3.3V
4. **Half-Duplex**: Diode on GPIO 17 prevents TX from blocking Dynamixel responses

---


### Mechanical 

### BoM and Drawings
![PlasmArm Robot Exploded](Mechanic/Complete%20Robot%20Exploded%20Assembly%20Drawing.png)
![End Effector Exploded](Mechanic/End%20Effector%20Exploded%20Assembly%20Drawing.png)

### Mechanical Project Files
- [STL Printing Files](Mechanic/STL%20Printing%20Files)
- [STEP Part Files](Mechanic/STEP%20Part%20Files)


### Assembly Instructions
- [Assembly Instructions](Mechanic/Assembly%20Instructions.md)

---
## Software

## Software Installation

### Prerequisites

#### Windows

1. **Python 3.8+** - Download from https://www.python.org/
2. **Git** - Download from https://git-scm.com/
3. **PlatformIO** - Install via pip:
   ```bash
   pip install platformio
   ```

#### Linux/macOS

```bash
# Install Python if not present
brew install python3  # macOS
sudo apt install python3 python3-pip  # Linux

# Install PlatformIO
pip3 install platformio
```

### PlatformIO Setup

1. Navigate to the ESP32 directory:
   ```bash
   cd PlasmArm/ESP32
   ```

2. Install dependencies:
   ```bash
   pio pkg install
   ```

### Python Client Setup

The Python client provides a GUI for importing DXF files and sending commands to the robot.

1. Navigate to the Python directory:
   ```bash
   cd PlasmArm/Python
   ```

2. Create a virtual environment (recommended):
   ```bash
   python -m venv venv
   ```

3. Activate the virtual environment:
   - Windows: `venv\Scripts\activate`
   - Linux/macOS: `source venv/bin/activate`

4. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```
   
   Required packages:
   - PyQt6 (GUI)
   - websocket-client
   - ezdxf (DXF parsing)

---

## Configuration

### WiFi Configuration

Edit `ESP32/src/Config.h`:

```cpp
// Access Point Mode (create your own network)
#define WIFI_AP_MODE true
#define WIFI_AP_SSID "PlasmArm_ESP32"
#define WIFI_AP_PASSWORD "12345678"

// OR Station Mode (connect to existing WiFi)
#define WIFI_AP_MODE false
#define WIFI_STA_SSID "Your_Network_Name"
#define WIFI_STA_PASSWORD "Your_Password"
```

### Robot Physical Parameters

```cpp
// Arm lengths in millimeters
#define ARM_LENGTH_1 220.05f  // First link (base to elbow)
#define ARM_LENGTH_2 217.65f  // Second link (elbow to end effector)

// Joint angle limits (degrees)
#define THETA1_MIN 0.0f
#define THETA1_MAX 180.0f
#define THETA2_MIN -150.0f
#define THETA2_MAX 150.0f
```

### Motor IDs (Dynamixel)

Default IDs (verify with Dynamixel Wizard):
- Shoulder: ID 1
- Elbow: ID 2

---

## Building and Uploading

### Build the Project

```bash
cd PlasmArm/ESP32
pio run
```

### Upload to ESP32

```bash
pio run -t upload
```

### Open Serial Monitor

```bash
pio device monitor
```

Serial monitor is at **115200 baud**.

---

## Running the Robot

### 1. Power On

Connect the 12V power supply and USB cable to the ESP32.

### 2. Connect to WiFi

If using AP mode:
- Connect to network `PlasmArm_ESP32`
- Password: `12345678`

If using Station mode:
- Robot will connect to your specified WiFi network
- Check serial monitor for IP address


### 3. Send Commands

Use the Python GUI to:
- Move the arm to specific coordinates (jog control)
- Import and draw DXF files
- Control the tool/end effector (pen up/down)

---

## Architecture Overview

### System Modules

The system is divided into three interconnected modules:

```
┌─────────────────────────────────────────────────────────────────┐
│                    PlasmArm Architecture                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────┐    ┌─────────────────────────────┐     │
│  │  User Interface     │    │   Embedded Control Unit     │     │
│  │  (PC - Python)      │    │   (ESP32)                   │     │
│  │                     │    │                             │     │
│  │  - DXF Import       │──▶│  - JSON Reception           │     │
│  │  - Trajectory Plan  │    │  - Inverse Kinematics       │    │
│  │  - Jog Controls     │    │  - Command Execution        │    │
│  │  - JSON Export      │    │  - Motor Communication      │    │
│  └─────────────────────┘    └──────────────┬──────────────┘    │
│                     Wi-Fi (WebSocket)     │                    │
│                                           │                    │
│                                           ▼                    │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    SCARA Arm (Mechanical)                │  │
│  │                                                          │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐   │  │
│  │  │ Dynamixel   │  │ Dynamixel   │  │ SG90 + Rack     │   │  │
│  │  │ (Shoulder)  │──│ (Elbow)     │  │ (Z-axis)        │   │  │
│  │  │  ID: 1      │  │  ID: 2      │  │                 │   │  │
│  │  └─────────────┘  └─────────────┘  └────────┬────────┘   │  │
│  │                                             │            │  │
│  │                                      ┌──────┴──────┐     │  │
│  │                                      │ Limit Switch│     │  │
│  │                                      │ (Contact)   │     │  │
│  │                                      └─────────────┘     │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

#### Module 1: User Interface (PC)

- **Technology**: Python with PyQt6
- **Functions**:
  - Import and parse DXF vector drawings
  - Generate trajectories centered on robot workspace
  - Manual jog controls
  - Export commands as JSON via Wi-Fi

#### Module 2: Embedded Control (ESP32)

- **Technology**: C++ with FreeRTOS
- **Functions**:
  - Receive JSON commands via WebSocket/Wi-Fi
  - Execute inverse kinematics calculations
  - Convert Cartesian coordinates to joint angles
  - High-frequency UART communication with Dynamixel motors

#### Module 3: SCARA Arm (Mechanical)

- **Technology**: 3D printed + Dynamixel servos
- **Components**:
  - Dynamixel XM430-W350 (Shoulder) - Time-Based Profile interpolation
  - Dynamixel XM430-W350 (Elbow) - Time-Based Profile interpolation
  - SG90 with rack-and-pinion for Z-axis
  - Limit switch for tool contact detection

### FreeRTOS Task Structure

The system uses two CPU cores:

```
Core 0: WebHandler (prio 1) + Trajectory Planner (prio 2)
Core 1: Motion Control (prio 3, 100 Hz loop)
```

| Task | Core | Priority | Function |
|------|------|----------|----------|
| WebHandler | 0 | 1 | Handle WebSocket commands from PC |
| Planner | 0 | 2 | Generate trajectory points from JSON |
| MotionControl | 1 | 3 | Execute motion at 100 Hz |

### Communication Flow

```
Web Client <--WebSocket--> WebHandler <--Command Queue--> Planner
                                                       |
                                                       v
                                            Trajectory Points
                                                       |
                                                       v
                                              Motion Queue
                                                       |
                                                       v
                                              MotionControl --> Motors
```

### Kinematics

The robot uses SCARA inverse kinematics:

#### Forward Kinematics
Convert joint angles (θ1, θ2) to Cartesian (x, y):
```
x = L1*cos(θ1) + L2*cos(θ1+θ2)
y = L1*sin(θ1) + L2*sin(θ1+θ2)
```

#### Inverse Kinematics
Convert Cartesian (x, y) to joint angles (θ1, θ2):
```
Using law of cosines and atan2
```

### Trajectory Planning

1. Receive target position and tool state from JSON
2. Generate linear path with waypoints
3. Apply trapezoidal velocity profile for smooth motion
4. Interpolate at 100 Hz for execution

---

## PyQt Interface Walktrough
![PlasmArm Robot](Software/GUI_walktrough.png)
The interface provides:

- **Manual Control**: Move arm to X, Y coordinates
- **DXF Upload**: Send drawing files to robot (via Python client)
- **Status Display**: Show current position and state
- **Tool Control**: Pen up/down commands

### WebSocket Commands

```json
// Move to position
{"type": "MOVE_TO", "x": 200, "y": 200, "speed": 100}

// Delay 
{"type": "DELAY", "ms": 500}

// Tool control
{"type": "TOOL", "state": "DOWN"}
{"type": "TOOL", "state": "UP"}

// Set home position
{"type": "SET_HOME"}
{"type": "SAVE_HOME"}
```

---

## Testing

The project includes built-in tests. Enable them in `ESP32/src/Config.h`:

```cpp
#define RUN_UNIT_TESTS true        // Automated kinematics/planner tests
#define RUN_VISUAL_TESTS true      // Detailed interpolation output
#define RUN_INTERACTIVE_TEST true  // Real hardware serial test
```

### Running Tests

1. Set test flags in `Config.h`
2. Build and upload: `pio run -t upload`
3. Open serial monitor at 115200 baud

See [ESP32/TEST_README.md](ESP32/TEST_README.md) for detailed testing instructions.

---

## Licenses

### Software

The Python code (DXF interpreter, GUI) and C++ firmware (ESP32) are distributed under the **GNU General Public License v3.0**.

This choice is primarily driven by the use of PyQt6, whose strong Copyleft license requires any project integrating it to adopt equivalent distribution conditions. GPLv3 ensures that future improvements by the community remain free and accessible to all.

### Hardware

3D printed models, CAD plans, and electrical schematics are distributed under the **CERN Open Hardware Licence - Weak (CERN-OHL-W)**.

---

## Troubleshooting

### Motors Not Moving

- Check power supply (12V, 3A minimum)
- Verify Dynamixel IDs are correct (default: 1 and 2)
- Check UART wiring (GPIO 16, 17)
- Use Dynamixel Wizard to verify motor communication

### Web Interface Not Loading

- Verify WiFi connection
- Check serial monitor for IP address

### Robot Not Reaching Target

- Check arm length parameters in Config.h
- Verify joint angle limits
- Ensure workspace boundaries are respected (half-circle, y > 0)

### Limit Switch Not Triggering

- Check wiring to GPIO 32
- Verify 10kΩ pull-up resistor is connected
- Test with multimeter

---

## Project Structure

```
PlasmArm/
├── ESP32/
│   ├── src/
│   │   ├── main.cpp           # Entry point, task creation
│   │   ├── Config.h           # All configuration constants
│   │   ├── core/
│   │   │   ├── Kinematics.cpp # Forward/Inverse kinematics
│   │   │   └── Planner.cpp    # Trajectory interpolation
│   │   ├── hardware/
│   │   │   ├── SG90.cpp       # Z-axis servo driver
│   │   │   └── Dynamixel2Arduino # Servo communication
│   │   ├── web/
│   │   │   └── WebServer.cpp  # WebSocket handler
│   │   └── test/              # Unit tests
│   ├── platformio.ini         # PlatformIO config
│   └── AGENTS.md              # Development notes
│
├── Python/
│   ├── main.py                # GUI application (PyQt6)
│   ├── robot_client.py       # WebSocket client
│   ├── dxf_parser.py         # DXF file parsing
│   └── requirements.txt      # Python dependencies
│
├── STL/                       # 3D print files (to be added)
│
├── Rapport final.pdf          # Technical report
│
├── LICENSE                    # GPLv3 (software)
│
├── CERN-OHL-W.txt            # Hardware license
│
└── README.md                  # This file
```
