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
# DynamicHub

DynamicHub is a SimTools replacement focused on running from VS Code. It reads motion telemetry from supported racing titles, translates that data into actuator commands, and sends those commands over a serial link to an Arduino Mega driving four HBS86-controlled actuators.

## Highlights
- Modular game interface projects for Assetto Corsa, Assetto Corsa Competizione (ACC), and rFactor 2.
- Configurable actuator layout (heave-only or multi-axis blending), per-axis travel limits, speed caps, and hardware offsets.
- Serial transport designed for an Arduino Mega that relays target positions to four HBS86 drivers.
- CLI-oriented tooling so the project is easy to run and debug inside VS Code terminals.

## Project layout
```
DynamicHub/
├── README.md
├── pyproject.toml           # Editable install + tooling configuration
└── src/dynamic_hub
    ├── __init__.py
    ├── config.py            # Actuator + profile configuration models
    ├── serial_link.py       # Serial transport to the Arduino Mega
    ├── telemetry_router.py  # Maps game telemetry into actuator targets
    ├── main.py              # Entry point wiring everything together
    ├── ui/
    │   └── cli.py           # CLI for setup, calibration, and runtime control
    └── game_interfaces/
        ├── __init__.py
        ├── base.py          # Shared interface contract
        ├── assetto_corsa.py
        ├── assetto_corsa_competizione.py
        └── rfactor2.py
```

## Usage
Install the project in editable mode and run the CLI:

```bash
python -m venv .venv
source .venv/bin/activate
pip install -e .
dynamichub --help
```

### Configure actuators
Calibrate hardware offsets and speed limits:
```bash
dynamichub actuators configure --port /dev/ttyUSB0 --max-speed 12000 --height 520
```

### Run with a game interface
Launch the router so it listens to a specific game feed and streams commands to the Arduino Mega:
```bash
dynamichub run --game acc --port /dev/ttyUSB0 --baud 115200
```

## Notes
- Game telemetry readers are structured but keep implementation light; replace the stubbed parsers with the data you prefer (UDP, shared memory, or plugins).
- The serial protocol uses a simple newline-delimited CSV: `tick,act1,act2,act3,act4`. Adjust `SerialLink.format_payload` if your firmware expects a different framing.
- All configuration is stored in `dynamic_hub.config.ProfileConfig` objects and can be saved/loaded as JSON for reuse.
