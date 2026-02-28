#include <HX711.h>
#include <Joystick.h>
#include <EEPROM.h>
#include <math.h>


// USB product name note:
// On ATmega32u4 boards (Leonardo), the name shown in Windows comes from the
// core build flag USB_PRODUCT (boards.txt), not this sketch source file.

// ---------------- Pins ----------------
static const uint8_t HX_DOUT = 3;   // HX711 DT
static const uint8_t HX_SCK  = 2;   // HX711 SCK
static const uint8_t CAL_PIN = 4;   // Hold LOW at boot to enter calibration mode

HX711 scale;

// --------------- Tuning ----------------
float MAX_FORCE  = 20.0f;   // full brake force (kg-equivalent)
float ALPHA      = 0.35f;   // higher = faster response (try 0.25..0.45)
float DEADZONE   = 0.10f;   // kg-equivalent
float GAMMA      = 1.2f;    // reduce for more immediate bite (try 1.0..1.3)

float filteredForce = 0.0f;

// --------------- Joystick --------------
Joystick_ Joystick(
  JOYSTICK_DEFAULT_REPORT_ID,
  JOYSTICK_TYPE_JOYSTICK,
  0, 0,
  false, false, true,   // X,Y,Z
  false, false, false,
  false, false, false,
  false, false
);

// --------------- EEPROM layout ----------
struct CalData {
  float calFactor;
  uint32_t magic;
};
static const uint32_t CAL_MAGIC = 0xC0A1BEEF;

float CAL_FACTOR = -7050.0f;

static float clamp01(float v) {
  return (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
}

void saveCal(float calFactor) {
  CalData d{calFactor, CAL_MAGIC};
  EEPROM.put(0, d);
}

bool loadCal(float &calFactorOut) {
  CalData d;
  EEPROM.get(0, d);
  if (d.magic != CAL_MAGIC) return false;
  if (!isfinite(d.calFactor) || fabs(d.calFactor) < 1.0f) return false;
  calFactorOut = d.calFactor;
  return true;
}

String readLine() {
  String s;
  while (true) {
    while (Serial.available()) {
      char c = static_cast<char>(Serial.read());
      if (c == '\r') continue;
      if (c == '\n') return s;
      s += c;
    }
  }
}

// Faster calibration capture (still averages, but only in calibration mode)
void calibrationMode() {
  Serial.println("\n=== CALIBRATION MODE ===");
  Serial.println("1) Ensure NO load is applied. Press ENTER to tare...");
  readLine();

  scale.set_scale(1.0f);
  scale.tare(20);
  long offset = scale.get_offset();
  Serial.print("Tare done. Offset = ");
  Serial.println(offset);

  Serial.println("\n2) Enter known calibration mass (kg) (e.g. 10 or 20), then ENTER:");
  float knownKg = readLine().toFloat();
  if (knownKg <= 0.0f) {
    Serial.println("Invalid mass. Exiting calibration.");
    return;
  }

  Serial.println("\n3) Apply that load NOW (steady), then press ENTER to capture...");
  readLine();

  long rawAvg = scale.read_average(15); // only here we average
  long net = rawAvg - offset;

  Serial.print("RawAvg = ");
  Serial.println(rawAvg);
  Serial.print("Net(raw-offset) = ");
  Serial.println(net);

  if (net == 0) {
    Serial.println("Net is 0 -> no signal change. Check wiring / load cell type.");
    return;
  }

  float newScale = static_cast<float>(net) / knownKg;

  Serial.print("\nComputed SCALE factor = ");
  Serial.println(newScale, 6);

  Serial.println("Save this factor to EEPROM? Type Y then ENTER:");
  String ans = readLine();
  if (ans.length() && (ans[0] == 'Y' || ans[0] == 'y')) {
    saveCal(newScale);
    Serial.println("Saved to EEPROM.");
  } else {
    Serial.println("Not saved.");
  }

  Serial.println("=== EXIT CALIBRATION MODE ===\n");
}

void setup() {
  pinMode(CAL_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("\nHandbrake Load Cell + HID (FAST) (Leonardo)");
  Serial.println("Cols: raw\tkg\tfiltered\taxis");

  scale.begin(HX_DOUT, HX_SCK);

  float eCal;
  if (loadCal(eCal)) {
    CAL_FACTOR = eCal;
    Serial.print("Loaded CAL_FACTOR from EEPROM: ");
    Serial.println(CAL_FACTOR, 6);
  } else {
    Serial.print("Using default CAL_FACTOR: ");
    Serial.println(CAL_FACTOR, 6);
  }

  if (digitalRead(CAL_PIN) == LOW) {
    calibrationMode();
    if (loadCal(eCal)) CAL_FACTOR = eCal;
  }

  scale.set_scale(CAL_FACTOR);
  scale.tare(20);

  Joystick.setZAxisRange(0, 1023);
  Joystick.begin();
}

void loop() {
  // ---- FAST: ONE READ ONLY ----
  long raw = scale.read(); // single sample (blocks only until one sample is ready)
  float kg = (static_cast<float>(raw) - scale.get_offset()) / scale.get_scale();

  if (kg < DEADZONE) kg = 0.0f;

  filteredForce += ALPHA * (kg - filteredForce);

  float norm = (MAX_FORCE > 0.0f) ? (filteredForce / MAX_FORCE) : 0.0f;
  norm = clamp01(norm);
  if (GAMMA != 1.0f) norm = pow(norm, GAMMA);

  int z = static_cast<int>(norm * 1023.0f + 0.5f);
  Joystick.setZAxis(z);

  // Throttle serial so printing doesn't slow the loop
  static uint32_t lastPrintMs = 0;
  uint32_t now = millis();
  if (now - lastPrintMs >= 20) { // 50 Hz print
    lastPrintMs = now;
    Serial.print(raw);
    Serial.print('\t');
    Serial.print(kg, 2);
    Serial.print('\t');
    Serial.print(filteredForce, 2);
    Serial.print('\t');
    Serial.println(z);
  }
}
