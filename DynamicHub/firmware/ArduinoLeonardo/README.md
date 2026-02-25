# Arduino Leonardo handbrake firmware (HX711 + load cell)

This folder contains a standalone Arduino sketch for using an Arduino Leonardo as a USB HID handbrake with an HX711 amplifier and load cell.

## Files
- `HandbrakeHX711.ino`: Calibratable handbrake firmware with EEPROM-backed scale factor storage.

## Key behavior
- Uses a single HX711 sample in the runtime loop for lower latency.
- Provides hold-low-on-boot calibration mode (`CAL_PIN`, default pin 4).
- Reports handbrake position on joystick Z axis (`0..1023`).
