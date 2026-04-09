# PlasmArm

A 2-DOF SCARA robotic arm controlled by an ESP32 with real-time WebSocket interface.

![PlasmArm Robot](path/to/robot_image.jpg)

## Table of Contents

- [Overview](#overview)
- [Hardware Requirements](#hardware-requirements)
- [Mechanical Assembly](#mechanical-assembly)
  - [3D Printed Parts](#3d-printed-parts)
  - [Bill of Materials](#bill-of-materials)
- [Electrical Wiring](#electrical-wiring)
- [Software Installation](#software-installation)
  - [Prerequisites](#prerequisites)
  - [PlatformIO Setup](#platformio-setup)
  - [Python Client Setup](#python-client-setup)
- [Configuration](#configuration)
- [Building and Uploading](#building-and-uploading)
- [Running the Robot](#running-the-robot)
- [Architecture Overview](#architecture-overview)
  - [FreeRTOS Task Structure](#freertos-task-structure)
  - [Kinematics](#kinematics)
  - [Trajectory Planning](#trajectory-planning)
- [Web Interface](#web-interface)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Overview

PlasmArm is a 2-Degree-of-Freedom SCARA (Selective Compliance Articulated Robot Arm) robotic arm project. It uses:

- **ESP32** (esp32doit-devkit-v1) as the main controller
- **FreeRTOS** for real-time dual-core task scheduling
- **Stepper motors** for precise joint control
- **WebSocket** interface for real-time control via web browser
- **PlatformIO** as the development framework

### Key Features

- Inverse and forward kinematics
- Trapezoidal velocity profile interpolation
- 100 Hz motion control loop
- Web-based control interface
- Supports both stepper motors and servo motors

---

## Hardware Requirements

### Microcontroller

- ESP32 (esp32doit-devkit-v1 or compatible)
- USB cable for programming and power

### Motors

- 2x Stepper motors (NEMA17 or similar)
  - 200 steps per revolution (1.8° per step)
- OR 2x Servo motors (SG90 or compatible)

### Motor Drivers

- 2x Stepper motor drivers (A4988, DRV8825, or TMC2208)
- Ensure GPIO12 has pulldown resistor (see ESP32 warning below)

### Power Supply

- 12V DC power supply (for stepper motors)
- 5V regulator for ESP32 (or USB power)

---

## Mechanical Assembly

### 3D Printed Parts

[TO BE ADDED: List of STL files and print settings]

<!--
Print all STL files from the `STL/` folder with recommended settings:
- Layer height: 0.2mm
- Infill: 20%
- Supports: As needed
-->

### Bill of Materials

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 Dev Kit | 1 | esp32doit-devkit-v1 |
| Stepper Motor (NEMA17) | 2 | 200 steps/rev |
| Stepper Driver (A4988/DRV8825) | 2 | |
| 12V Power Supply | 1 | 2A minimum |
| 608 Bearings | 4 | For smooth rotation |
| M3 Screws | 20+ | Various lengths |
| M3 Heat Inserts | 10+ | For plastic parts |
| GT2 Belt (optional) | 1 | For timing |

### Assembly Instructions

[TO BE ADDED: Step-by-step assembly guide with images]

<!--
1. Print all required parts
2. Install heat inserts into 3D printed parts
3. Assemble base joint with first stepper motor
4. Attach first arm segment
5. Assemble elbow joint with second stepper motor
6. Attach second arm segment
7. Install end effector (tool servo)
8. Mount ESP32 and motor drivers
-->

---

## Electrical Wiring

### ESP32 Pin Connections

| Function | GPIO Pin | Notes |
|----------|----------|-------|
| Motor 1 Step | 19 | Base joint |
| Motor 1 Dir | 18 | |
| Motor 1 Enable | 27 | |
| Motor 2 Step | 14 | Elbow joint |
| Motor 2 Dir | 12 | ⚠️ Strapping pin - ensure pulldown |
| Motor 2 Enable | 13 | |
| Tool Servo | 26 | End effector |
| Tool Switch | 32 | Pressure switch input |
| Dynamixel RX | 16 | Serial2 |
| Dynamixel TX | 17 | Serial2 |

### Wiring Diagram

[TO BE ADDED: Electrical schematic]

<!--
```
ESP32        Stepper Driver 1    Stepper Driver 2
GPIO19 ----> STEP               (Motor 1 - Base)
GPIO18 ----> DIR
GPIO27 ----> ENABLE
GPIO14 ----> STEP               (Motor 2 - Elbow)
GPIO12 ----> DIR               ⚠️ Watch boot voltage!
GPIO13 ----> ENABLE

12V Power ----> Motors ----> Drivers ----> GND
```

⚠️ **ESP32 GPIO12 Warning**: GPIO12 is a strapping pin that controls flash voltage. 
Ensure your motor driver doesn't hold GPIO12 HIGH during boot, or the ESP32 won't start.
Add a pulldown resistor (10kΩ) on GPIO12 if unsure.
-->

---

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

The Python client provides a GUI for sending DXF files to the robot.

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
   - PyQt5 (GUI)
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

### Motor Configuration

```cpp
// Stepper motor parameters
#define STEPS_PER_REVISION 200   // 1.8° per step
#define MICROSTEPS 16            // Microstepping factor
```

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

### Upload Filesystem (for web assets)

```bash
pio run -t uploadfs
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

### 3. Access Web Interface

Open a browser and navigate to:
```
http://192.168.4.1  (AP mode)
http://<IP_ADDRESS> (Station mode)
```

### 4. Send Commands

Use the web interface to:
- Move the arm to specific coordinates
- Draw DXF files
- Control the tool/end effector

---

## Architecture Overview

### FreeRTOS Task Structure

The system uses two cores:

```
Core 0: WebHandler (prio 1) + Trajectory Planner (prio 2)
Core 1: Motion Control (prio 3, 100 Hz loop)
```

| Task | Core | Priority | Function |
|------|------|----------|----------|
| WebHandler | 0 | 1 | Handle WebSocket commands |
| Planner | 0 | 2 | Generate trajectory points |
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

The robot uses a SCARA kinematics model:

- **Forward Kinematics**: Convert joint angles (θ1, θ2) to Cartesian (x, y)
  ```
  x = L1*cos(θ1) + L2*cos(θ1+θ2)
  y = L1*sin(θ1) + L2*sin(θ1+θ2)
  ```

- **Inverse Kinematics**: Convert Cartesian (x, y) to joint angles (θ1, θ2)
  ```
  Using law of cosines and atan2
  ```

### Trajectory Planning

1. Receive target position from command
2. Generate path with linear interpolation
3. Apply trapezoidal velocity profile
4. Interpolate at 100 Hz for smooth motion

---

## Web Interface

The web interface provides:

- **Manual Control**: Move arm to X, Y coordinates
- **DXF Upload**: Send drawing files to robot
- **Status Display**: Show current position and state
- **Settings**: Configure speed, acceleration

### WebSocket Commands

```json
// Move to position
{"cmd": "move", "x": 300, "y": 100, "speed": 50}

// Draw DXF
{"cmd": "draw", "points": [[x,y], [x,y], ...]}

// Tool control
{"cmd": "tool", "action": "down"}
{"cmd": "tool", "action": "up"}

// Home position
{"cmd": "home"}
```

---

## Testing

The project includes built-in tests. Enable them in `Config.h`:

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

## Troubleshooting

### ESP32 Won't Boot

- **Cause**: GPIO12 held HIGH during boot
- **Fix**: Add 10kΩ pulldown resistor on GPIO12, or ensure motor driver is OFF at boot

### Motors Not Moving

- Check power supply (12V, 2A minimum)
- Verify step/dir pin connections
- Check enable pin is pulled LOW

### Web Interface Not Loading

- Verify WiFi connection
- Check serial monitor for IP address
- Ensure `pio run -t uploadfs` was run

### Robot Not Reaching Target

- Check arm length parameters in Config.h
- Verify joint angle limits
- Ensure workspace boundaries are respected

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
│   │   │   ├── StepperMotor.cpp
│   │   │   └── SG90.cpp       # Servo motor driver
│   │   ├── web/
│   │   │   └── WebServer.cpp  # WebSocket handler
│   │   └── test/              # Unit tests
│   ├── data/
│   │   └── index.html         # Web interface
│   ├── platformio.ini         # PlatformIO config
│   └── AGENTS.md              # Development notes
│
├── Python/
│   ├── main.py                # GUI application
│   ├── robot_client.py       # WebSocket client
│   └── dxf_parser.py         # DXF file parsing
│
├── STL/                       # 3D print files (to be added)
│
└── README.md                  # This file
```

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Acknowledgments

Based on projects from:
- [GRO400H25-marcus](https://github.com/UdeS-GRO/GRO400H25-marcus)
- [S4H2023-POLUS](https://github.com/UdeS-GRO/S4H2023-POLUS)