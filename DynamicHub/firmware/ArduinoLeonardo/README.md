# Arduino Leonardo handbrake firmware (HX711 + load cell)

This folder contains a standalone Arduino sketch for using an Arduino Leonardo as a USB HID handbrake with an HX711 amplifier and load cell.

## Files
- `HandbrakeHX711.ino`: Calibratable handbrake firmware with EEPROM-backed scale factor storage.

## Key behavior
- Uses a single HX711 sample in the runtime loop for lower latency.
- Provides hold-low-on-boot calibration mode (`CAL_PIN`, default pin 4).
- Reports handbrake position on both HID **Z axis** and **Brake axis** (`0..1023`) for better game compatibility.
- Does **not** block on `while (!Serial)`, so HID starts even when Arduino Serial Monitor is closed.
- Drives two optional status LEDs:
  - `D5` (**Brake LED**) turns ON when handbrake value is above a small threshold.
  - `D6` (**Connection LED**) turns ON when USB is configured by the host (PC).

## Why Z + Brake compatibility mode?
Some games ignore vendor-specific brake-only HID layouts, while others bind best to brake/slider-style inputs.
This sketch exposes **both Z and Brake** from the same handbrake value and keeps X centered, which improves detection
in titles that expect at least one centered stick axis during device discovery.

## Changing the USB device name shown in Windows

If Windows shows **"Arduino Leonardo"** and you want **"Dynamic Handbrake"**, that name comes from the
**AVR core USB descriptor build flags** (`USB_PRODUCT`), not from the Joystick sketch code.

### Arduino IDE (quick approach)
1. Open your Arduino AVR core `boards.txt` file:
   - Typical path: `C:\Users\<you>\AppData\Local\Arduino15\packages\arduino\hardware\avr\<version>\boards.txt`
2. Find the Leonardo entries and change product string defines from:
   - `leonardo.build.usb_product="Arduino Leonardo"`
   - to `leonardo.build.usb_product="Dynamic Handbrake"`
3. Save, restart Arduino IDE, and re-upload firmware.
4. In Windows Device Manager, uninstall the old Leonardo device once (check "delete driver" if offered), unplug/replug.

### Better long-term approach (recommended)
Create a custom board variant (copy Leonardo section in `boards.txt` to e.g. `dynhandbrake.*`) and only change:
- `dynhandbrake.build.usb_product="Dynamic Handbrake"`
- optionally `dynhandbrake.build.usb_manufacturer="SimDynamics"`

This avoids editing upstream Leonardo defaults directly.

> Note: existing Windows HID entries may keep a cached friendly name until you remove old device instances.

## LED wiring note
Connect each LED (with resistor, e.g. 220Ω-1kΩ) from the pin to GND if you want ON=HIGH behavior:
- D5 -> resistor -> LED anode, LED cathode -> GND
- D6 -> resistor -> LED anode, LED cathode -> GND

`D6` indicates USB enumeration/configuration, not whether a specific game has currently bound the axis.
