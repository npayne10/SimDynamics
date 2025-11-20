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
