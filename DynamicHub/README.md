# DynamicHub (C++)

DynamicHub is a compiled SimTools replacement you can build and debug directly in VS Code. It reads motion telemetry from racing
titles, blends the data across four actuators, and streams CSV commands to an Arduino Mega that drives HBS86-controlled NEMA 34
steppers.

## Features
- C++17 executable with a small dependency footprint, ready for CMake + VS Code.
- Game interface modules for Assetto Corsa, Assetto Corsa Competizione (ACC), and rFactor 2 (UDP listeners that accept
  space-separated telemetry values: `surge sway heave roll pitch yaw`).
- Rig profile loader (INI-style) so you can tune travel limits, offsets, and axis mixes per actuator.
- Serial link that emits `tick,act1,act2,act3,act4` lines for the supplied Arduino Mega firmware.
- Optional HOME and SELFTEST commands sent straight to the controller at startup.

## Building
1. Configure the build directory:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```
2. Compile:
   ```bash
   cmake --build build
   ```
   The executable is emitted at `build/dynamic_hub`.

### VS Code hint
Use the CMake Tools extension or add a simple task that runs the two commands above. The project has no external library
requirements on Linux; Windows users can compile under WSL or replace the serial backend.

## Configuration
1. Copy the example rig profile:
   ```bash
   cp config/rig_profile.example.ini config/rig_profile.ini
   ```
2. Edit `config/rig_profile.ini` to set min/max heights, offsets, and axis mixes per actuator. Keys are grouped into
   `[actuatorN]` and `[blendN]` sections (1-4).

## Running
Start DynamicHub with your preferred game interface and serial port:
```bash
./build/dynamic_hub --game acc --port /dev/ttyUSB0 --baud 115200 --profile config/rig_profile.ini --home
```
- `--game` accepts `ac`, `acc`, or `rf2`.
- `--home` sends the homing command once at startup; `--selftest` triggers the firmware's test routine.
- Telemetry datagrams must contain six whitespace-separated numbers in the order `surge sway heave roll pitch yaw`.

## Arduino firmware
Flash `firmware/ArduinoMega/DynamicHubController.ino` to the Arduino Mega that drives your four HBS86 step/dir channels.
The sketch expects CSV payloads of `tick,act1,act2,act3,act4` (heights in millimeters). Send the plain-text commands `HOME` or
`SELFTEST` over the same serial link to trigger homing or a basic travel verification.
