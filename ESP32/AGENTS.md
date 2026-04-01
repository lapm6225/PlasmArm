# AGENTS.md

Instructions for AI agents working on this codebase.

## Project Overview

PlasmArm - ESP32-based 2-DOF SCARA robotic arm controller with WebSocket UI.
- Platform: ESP32 (esp32doit-devkit-v1)
- Framework: Arduino via PlatformIO
- Real-time: FreeRTOS dual-core tasks (100 Hz motion loop)

## Build & Upload

```bash
pio run                    # Build
pio run -t upload          # Build + upload
pio run -t uploadfs        # Upload filesystem
pio device monitor         # Serial monitor (115200 baud)
```

## Testing

Tests run ON the ESP32 (not host-side). Set flags in `src/Config.h`:

```cpp
#define RUN_UNIT_TESTS true        // Automated kinematics/planner tests
#define RUN_VISUAL_TESTS true      // Detailed interpolation output
#define RUN_INTERACTIVE_TEST true  // Real hardware serial test
```

Then `pio run -t upload` and open serial monitor.

## Linting & Formatting

```bash
clang-format -i src/**/*.cpp src/**/*.h    # Format code
cpplint --recursive src/                    # Lint (Google style)
"C:/Program Files/Cppcheck/cppcheck.exe" --enable=warning --suppress=missingIncludeSystem src/  # Static analysis
pio run                                     # Verify build still works
```

## Code Style

- 4-space indent, no tabs
- Opening brace on same line
- camelCase for methods, PascalCase for classes
- Doxygen `/** */` comments on public APIs
- No `using namespace` in headers

## Architecture

```
Core 0: WebHandler (prio 1) + Trajectory Planner (prio 2)
Core 1: Motion Control (prio 3, 100 Hz loop)
```

Communication via FreeRTOS queues: `commandQueue` (30), `motionQueue` (1000).

## Key Files

| File | Purpose |
|---|---|
| `src/Config.h` | All constants, WiFi creds, test flags |
| `src/core/Kinematics.cpp` | Forward/Inverse kinematics |
| `src/core/Planner.cpp` | Trajectory interpolation |
| `src/hardware/IMotor.h` | Motor interface (polymorphism) |
| `src/web/WebServer.cpp` | WebSocket command handler |

## Libraries

- ESPAsyncWebServer ^3.6.0
- ArduinoJson ^6.21.3
- Dynamixel2Arduino ^0.8.1
- ESP32Servo ^3.0.5

## ESP32 Gotchas (from LobeHub ESP32 skill)

### GPIO Restrictions
- Strapping pins affect boot mode: GPIO0, GPIO2, GPIO12, GPIO15
- GPIO6-11 connected to flash — never use, crashes immediately
- GPIO34-39 input only — no output, no pullup/pulldown
- ADC2 unusable with WiFi active — use ADC1 (GPIOs 32-39) when WiFi enabled
- **GPIO12 is used as MOTOR2_DIR_PIN** — ensure motor driver is LOW at boot

### FreeRTOS
- Default stack too small for printf/WiFi — use 4096+ for complex tasks
- Task watchdog triggers at 5s default — call `vTaskDelay()` or feed watchdog
- Use `xTaskCreatePinnedToCore()` for core affinity — WiFi on core 0, your code on core 1
- `delay()` yields to scheduler — `vTaskDelay(pdMS_TO_TICKS(ms))` in tasks

### WiFi
- Call `WiFi.mode()` before `WiFi.begin()` — mode affects behavior
- Event-driven with `WiFi.onEvent()` more reliable than polling `WiFi.status()`

### Memory
- Heap fragments over time — preallocate buffers, avoid repeated malloc/free
- String concatenation fragments heap — use `reserve()` or char arrays
- `ESP.getFreeHeap()` for monitoring — log periodically in long-running apps

### Peripherals
- No native `analogWrite()` — use LEDC: `ledcSetup()`, `ledcAttachPin()`, `ledcWrite()`
- UART0 is Serial/USB — use UART1/2 for external devices (Dynamixel uses Serial2)
