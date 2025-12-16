# SimDynamics
This is for the documents and code for the SimDynamics 3DOF Actuator Simulator and related software.

## Wiring
- `docs/wiring-diagram.pdf` shows the Arduino Mega 2560 wiring to four HBS86H drivers and their limit switches.
- Regenerate the PDF by running `python docs/wiring_diagram.py`; the script writes PDF primitives directly so it has no external dependencies.

## Sketch layout
- The primary Arduino sketch is `SimTools4ActuatorController.ino`. Keep only this `.ino` in the project root when compiling so the IDE does not merge multiple sketches together.
- Legacy/experimental sketches have been moved under `examples/`; for instance, `examples/DOF3_actuator_selftest/` contains the original standalone self-test sketch.
- If you see redefinition errors (e.g., `redefinition of 'const float LEAD_MM_PER_REV'`), double-check that only one copy of the controller `.ino` is present in the folder—remove or move aside any backups or alternate sketch revisions before compiling.

## Actuator limits
- The sketch enforces an 85 mm maximum physical stroke for each actuator; car profile stroke requests above this are clamped to protect hardware travel.
