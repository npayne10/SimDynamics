/*
  4-Actuator Motion Controller for HBS86 (STEP/DIR/ENA)
  Features:
    - SimTools 4-axis parsing: <A1><Axis1a>~<A2><Axis2a>~<A3><Axis3a>~<A4><Axis4a>~
    - Per-axis MIN/MAX limit switches
    - Homing to MIN with backoff + precise edge
    - Power-on initiation SELF-TEST (25% -> 75% -> post-home)
    - WATCHDOG: freeze/disable if SimTools packets stop

  Axis mapping:
    Axis1 -> FR (0)
    Axis2 -> RR (1)
    Axis3 -> FL (2)
    Axis4 -> RL (3)

  Author: ChatGPT (GPT-5 Thinking)
  Version: v1.4
*/

#include <AccelStepper.h>
#include <Arduino.h>

// ------------------- USER CONFIG -------------------
// Axis indices: 0=FR, 1=RR, 2=FL, 3=RL
const uint8_t STEP_PIN[4] = { 22, 24, 26, 28 };
const uint8_t DIR_PIN[4]  = { 23, 25, 27, 29 };
const uint8_t EN_PIN[4]   = { 30, 31, 32, 33 }; // 255 = unused

// Limit switch pins
const uint8_t MIN_SW_PIN[4] = { 34, 36, 38, 40 }; // Home side
const uint8_t MAX_SW_PIN[4] = { 35, 37, 39, 41 }; // Far end

// If mechanics move the wrong way, flip here
bool DIR_INVERT[4] = { false, false, true, true };

// Enable polarity
const bool ENABLE_ACTIVE_LOW = true;

// Limit switch logic
const bool LIMIT_ACTIVE_LOW = true;        // NC -> LOW when triggered with INPUT_PULLUP
const unsigned long LIMIT_DEBOUNCE_MS = 5; // ms

// Mechanics
const float LEAD_MM_PER_REV = 5.0f;
const int   MICROSTEPS      = 16;
const int   STEPS_PER_REV   = 200;
const float STEPS_PER_MM    = (STEPS_PER_REV * MICROSTEPS) / LEAD_MM_PER_REV;

// Travel limits (mm)
const float STROKE_MM       = 150.0f;
const float MIN_POS_MM      = 0.0f;
const float MAX_POS_MM      = MIN_POS_MM + STROKE_MM;

// Motion tuning (normal run)
const float MAX_SPEED_MM_S  = 80.0f;
const float ACCEL_MM_S2     = 800.0f;

// Homing tuning
const float HOME_FAST_MM_S  = 30.0f;
const float HOME_SLOW_MM_S  = 6.0f;
const float HOME_BACKOFF_MM = 3.0f;
const float POST_HOME_OFFSET_MM = 2.0f;

// Serial/SimTools
const uint32_t SERIAL_BAUD  = 115200;
const bool     ECHO_RX      = false;
const float    TARGET_SMOOTH_ALPHA = 0.25f; // 0..1 smoothing

// -------- Self-Test tuning --------
const float TST_PCT1 = 0.25f;
const float TST_PCT2 = 0.75f;
const float TST_SPEED_MM_S  = 40.0f;
const float TST_ACCEL_MM_S2 = 600.0f;
const float TST_TIMEOUT_FACTOR = 3.0f;
const unsigned long TST_TIMEOUT_FIXED_MS = 400;

// -------- Watchdog (NEW) --------
const unsigned long WATCHDOG_TIMEOUT_MS = 750; // trip if no packets for this long
const bool WD_DISABLE_EN_ON_TIMEOUT     = false; // true: drop EN when tripped

// ---------------------------------------------------
const float MAX_SPEED_STEPS_S = MAX_SPEED_MM_S * STEPS_PER_MM;
const float ACCEL_STEPS_S2    = ACCEL_MM_S2 * STEPS_PER_MM;

AccelStepper steppers[4] = {
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[0], DIR_PIN[0]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[1], DIR_PIN[1]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[2], DIR_PIN[2]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[3], DIR_PIN[3])
};

volatile float target_mm[4]  = { MIN_POS_MM, MIN_POS_MM, MIN_POS_MM, MIN_POS_MM };
volatile long  target_steps[4];
String inBuf;

// Debounced limit state
bool minHit[4] = {false,false,false,false};
bool maxHit[4] = {false,false,false,false};
unsigned long lastSampleMs = 0;

// Watchdog state
unsigned long lastCmdMs = 0;
bool watchdogTripped = false;

// ------------------- Utils -------------------
template<typename T>
T clamp(T v, T lo, T hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

float mmToSteps(float mm) { return mm * STEPS_PER_MM; }
float stepsToMM(long st)  { return st / STEPS_PER_MM; }

float axisByteToMM(int v) {
  v = clamp(v, 0, 255);
  float t = v / 255.0f;
  return MIN_POS_MM + t * (MAX_POS_MM - MIN_POS_MM);
}

float smoothTarget(float current, float desired) {
  if (TARGET_SMOOTH_ALPHA <= 0.0f) return desired;
  if (TARGET_SMOOTH_ALPHA >= 1.0f) return current;
  return current + TARGET_SMOOTH_ALPHA * (desired - current);
}

bool rawLimitTriggered(uint8_t pin) {
  int r = digitalRead(pin);
  return LIMIT_ACTIVE_LOW ? (r == LOW) : (r == HIGH);
}

void sampleLimitsDebounced() {
  unsigned long now = millis();
  if (now - lastSampleMs < LIMIT_DEBOUNCE_MS) return;
  lastSampleMs = now;
  for (int i = 0; i < 4; ++i) {
    minHit[i] = rawLimitTriggered(MIN_SW_PIN[i]);
    maxHit[i] = rawLimitTriggered(MAX_SW_PIN[i]);
  }
}

void enforceHardLimits() {
  for (int i = 0; i < 4; ++i) {
    long cur = steppers[i].currentPosition();
    long tgt = steppers[i].targetPosition();
    float curMM = stepsToMM(cur);

    if (minHit[i]) {
      if (tgt < cur) steppers[i].stop();
      float clamped = max(curMM, MIN_POS_MM + POST_HOME_OFFSET_MM);
      target_mm[i] = max(target_mm[i], clamped);
      target_steps[i] = lroundf(mmToSteps(target_mm[i]));
      steppers[i].moveTo(target_steps[i]);
    }
    if (maxHit[i]) {
      if (tgt > cur) steppers[i].stop();
      float clamped = min(curMM, MAX_POS_MM);
      target_mm[i] = min(target_mm[i], clamped);
      target_steps[i] = lroundf(mmToSteps(target_mm[i]));
      steppers[i].moveTo(target_steps[i]);
    }
  }
}

void setEnabled(bool enable) {
  for (int i = 0; i < 4; ++i) {
    if (EN_PIN[i] == 255) continue;
    pinMode(EN_PIN[i], OUTPUT);
    digitalWrite(EN_PIN[i],
      (ENABLE_ACTIVE_LOW ? (enable ? LOW : HIGH) : (enable ? HIGH : LOW)));
  }
}

// ------------------- Watchdog (NEW) -------------------
void watchdogTrip() {
  if (watchdogTripped) return;
  watchdogTripped = true;

  // Freeze all axes at current position
  for (int i = 0; i < 4; ++i) {
    long cur = steppers[i].currentPosition();
    steppers[i].stop();                 // stop any ramp-in-progress
    steppers[i].setCurrentPosition(cur); // align internal state
    steppers[i].moveTo(cur);             // hold position
    target_steps[i] = cur;
    target_mm[i] = stepsToMM(cur);
  }

  if (WD_DISABLE_EN_ON_TIMEOUT) setEnabled(false);

  Serial.println(F("[WATCHDOG] No SimTools data. Motion frozen.")
                 );
  if (WD_DISABLE_EN_ON_TIMEOUT)
    Serial.println(F("[WATCHDOG] Drivers disabled (EN off)."));
}

void watchdogMaybeRecover() {
  if (!watchdogTripped) return;
  // Re-enable when a new valid packet was seen (handled in parser).
  setEnabled(true);
  watchdogTripped = false;
  Serial.println(F("[WATCHDOG] Data resumed. Control restored."));
}

void watchdogCheck() {
  unsigned long now = millis();
  if (!watchdogTripped && (now - lastCmdMs > WATCHDOG_TIMEOUT_MS)) {
    watchdogTrip();
  }
}

// ------------------- Setup helpers -------------------
void setupSteppers() {
  for (int i = 0; i < 4; ++i) {
    steppers[i].setPinsInverted(false, DIR_INVERT[i], false);
    steppers[i].setMaxSpeed(MAX_SPEED_STEPS_S);
    steppers[i].setAcceleration(ACCEL_STEPS_S2);
    long minSteps = lroundf(mmToSteps(MIN_POS_MM));
    steppers[i].setCurrentPosition(minSteps);
    target_steps[i] = minSteps;
    steppers[i].moveTo(target_steps[i]);
  }
}

void setupLimits() {
  for (int i = 0; i < 4; ++i) {
    pinMode(MIN_SW_PIN[i], INPUT_PULLUP);
    pinMode(MAX_SW_PIN[i], INPUT_PULLUP);
  }
  sampleLimitsDebounced();
}

// ------------------- Homing -------------------
bool homeOneAxis(uint8_t i) {
  Serial.print(F("Homing axis ")); Serial.println(i);

  float fastStepsPerSec = HOME_FAST_MM_S * STEPS_PER_MM;
  steppers[i].setMaxSpeed(fastStepsPerSec);
  steppers[i].setAcceleration(ACCEL_STEPS_S2);

  // Fast seek toward MIN
  steppers[i].setSpeed(-fastStepsPerSec);
  while (true) {
    sampleLimitsDebounced();
    if (minHit[i]) break;
    steppers[i].runSpeed();
  }

  // Back off
  long backoff = lroundf(mmToSteps(HOME_BACKOFF_MM));
  steppers[i].move(backoff);
  while (steppers[i].distanceToGo() != 0) {
    sampleLimitsDebounced();
    steppers[i].run();
  }

  // Slow approach
  float slowStepsPerSec = HOME_SLOW_MM_S * STEPS_PER_MM;
  steppers[i].setMaxSpeed(slowStepsPerSec);
  steppers[i].setSpeed(-slowStepsPerSec);
  while (true) {
    sampleLimitsDebounced();
    if (minHit[i]) break;
    steppers[i].runSpeed();
  }

  // Zero & move to offset
  steppers[i].setCurrentPosition(lroundf(mmToSteps(MIN_POS_MM)));
  long post = lroundf(mmToSteps(MIN_POS_MM + POST_HOME_OFFSET_MM));
  steppers[i].moveTo(post);
  while (steppers[i].distanceToGo() != 0) {
    sampleLimitsDebounced();
    steppers[i].run();
  }

  // Restore normal run speeds
  steppers[i].setMaxSpeed(MAX_SPEED_STEPS_S);
  steppers[i].setAcceleration(ACCEL_STEPS_S2);

  target_mm[i]    = MIN_POS_MM + POST_HOME_OFFSET_MM;
  target_steps[i] = lroundf(mmToSteps(target_mm[i]));

  Serial.print(F("Axis ")); Serial.print(i); Serial.println(F(" homed."));
  return true;
}

void homeAll() {
  Serial.println(F("Homing start..."));
  for (int i = 0; i < 4; ++i) {
    if (!homeOneAxis(i)) {
      Serial.print(F("Homing failed on axis ")); Serial.println(i);
    }
  }
  Serial.println(F("Homing complete."));
}

// --------------- Self-Test Helpers ---------------
unsigned long calcTimeoutMs(float fromMM, float toMM, float speedMMs) {
  float dist = fabs(toMM - fromMM);
  float tMs  = (dist / max(speedMMs, 1e-3f)) * 1000.0f;
  return (unsigned long)(tMs * TST_TIMEOUT_FACTOR) + TST_TIMEOUT_FIXED_MS;
}

bool moveAxisWithChecks(uint8_t i, float targetMM, float speedMMs, float accelMMs2) {
  targetMM = clamp(targetMM, MIN_POS_MM, MAX_POS_MM);

  float savedMax = steppers[i].maxSpeed();
  float savedAcc = steppers[i].acceleration();
  float spdSteps = speedMMs * STEPS_PER_MM;
  float accSteps = accelMMs2 * STEPS_PER_MM;
  steppers[i].setMaxSpeed(spdSteps);
  steppers[i].setAcceleration(accSteps);

  float startMM = stepsToMM(steppers[i].currentPosition());
  unsigned long timeoutMs = calcTimeoutMs(startMM, targetMM, speedMMs);
  unsigned long t0 = millis();

  long tgtSteps = lroundf(mmToSteps(targetMM));
  steppers[i].moveTo(tgtSteps);

  while (steppers[i].distanceToGo() != 0) {
    sampleLimitsDebounced();

    if (steppers[i].targetPosition() > steppers[i].currentPosition() && maxHit[i]) {
      Serial.print(F("[FAIL] Axis ")); Serial.print(i);
      Serial.println(F(" hit MAX unexpectedly."));
      steppers[i].stop();
      steppers[i].setMaxSpeed(savedMax);
      steppers[i].setAcceleration(savedAcc);
      return false;
    }
    if (steppers[i].targetPosition() < steppers[i].currentPosition() && minHit[i]) {
      Serial.print(F("[FAIL] Axis ")); Serial.print(i);
      Serial.println(F(" hit MIN unexpectedly."));
      steppers[i].stop();
      steppers[i].setMaxSpeed(savedMax);
      steppers[i].setAcceleration(savedAcc);
      return false;
    }

    if (millis() - t0 > timeoutMs) {
      Serial.print(F("[FAIL] Axis ")); Serial.print(i);
      Serial.println(F(" timed out."));
      steppers[i].stop();
      steppers[i].setMaxSpeed(savedMax);
      steppers[i].setAcceleration(savedAcc);
      return false;
    }
    steppers[i].run();
  }

  target_mm[i]    = targetMM;
  target_steps[i] = lroundf(mmToSteps(targetMM));

  steppers[i].setMaxSpeed(savedMax);
  steppers[i].setAcceleration(savedAcc);
  return true;
}

bool selfTestAxis(uint8_t i) {
  Serial.print(F("Self-test axis ")); Serial.println(i);

  bool ok = true;
  float posHome  = MIN_POS_MM + POST_HOME_OFFSET_MM;
  float pos25    = MIN_POS_MM + TST_PCT1 * STROKE_MM;
  float pos75    = MIN_POS_MM + TST_PCT2 * STROKE_MM;

  Serial.print(F("  -> 25% (")); Serial.print(pos25, 2); Serial.println(F(" mm)"));
  ok &= moveAxisWithChecks(i, pos25, TST_SPEED_MM_S, TST_ACCEL_MM_S2);

  if (ok) {
    Serial.print(F("  -> 75% (")); Serial.print(pos75, 2); Serial.println(F(" mm)"));
    ok &= moveAxisWithChecks(i, pos75, TST_SPEED_MM_S, TST_ACCEL_MM_S2);
  }

  if (ok) {
    Serial.print(F("  -> Post-home (")); Serial.print(posHome, 2); Serial.println(F(" mm)"));
    ok &= moveAxisWithChecks(i, posHome, TST_SPEED_MM_S, TST_ACCEL_MM_S2);
  }

  Serial.print(F("Axis ")); Serial.print(i);
  Serial.println(ok ? F(" PASS") : F(" FAIL"));
  return ok;
}

void selfTestAll() {
  Serial.println(F("=== Initiation Self-Test START ==="));
  bool allOk = true;
  for (int i = 0; i < 4; ++i) {
    bool ok = selfTestAxis(i);
    allOk &= ok;
  }
  Serial.println(allOk ? F("=== Self-Test COMPLETE: PASS ===")
                       : F("=== Self-Test COMPLETE: FAIL ==="));
}

// ------------------- Parser & Motion -------------------
int firstIntIn(const String& s) {
  int n = s.length(), i = 0;
  while (i < n && !isDigit(s[i])) i++;
  if (i >= n) return -1;
  long val = 0;
  while (i < n && isDigit(s[i])) { val = val*10 + (s[i]-'0'); i++; }
  return (int)val;
}

void noteFreshCommandSeen() {
  lastCmdMs = millis();
  if (watchdogTripped) watchdogMaybeRecover();
}

void parseIncoming() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (ECHO_RX) Serial.write(c);
    inBuf += c;

    int tildePos;
    while ((tildePos = inBuf.indexOf('~')) != -1) {
      String tok = inBuf.substring(0, tildePos);
      inBuf.remove(0, tildePos + 1);

      static uint8_t axisIx = 0; // cycles 0..3 each message
      int val = firstIntIn(tok);
      if (val >= 0) {
        noteFreshCommandSeen(); // <-- WATCHDOG heartbeat
        float mmDesired = axisByteToMM(clamp(val, 0, 255));
        mmDesired = clamp(mmDesired, MIN_POS_MM, MAX_POS_MM);
        target_mm[axisIx] = smoothTarget(target_mm[axisIx], mmDesired);
        axisIx = (axisIx + 1) & 0x03;
      }

      if (inBuf.length() > 64) inBuf.remove(0, inBuf.length() - 16);
    }
  }
}

void updateMoves() {
  for (int i = 0; i < 4; ++i) {
    float curMM = stepsToMM(steppers[i].currentPosition());
    if (minHit[i] && target_mm[i] < curMM) target_mm[i] = curMM;
    if (maxHit[i] && target_mm[i] > curMM) target_mm[i] = curMM;

    target_mm[i] = clamp(target_mm[i], MIN_POS_MM, MAX_POS_MM);
    long steps = lroundf(mmToSteps(target_mm[i]));
    if (steps != target_steps[i]) {
      target_steps[i] = steps;
      steppers[i].moveTo(target_steps[i]);
    }
  }
}

// ------------------- SETUP / LOOP -------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {}

  for (int i = 0; i < 4; ++i) {
    pinMode(STEP_PIN[i], OUTPUT);
    pinMode(DIR_PIN[i], OUTPUT);
    if (EN_PIN[i] != 255) pinMode(EN_PIN[i], OUTPUT);
  }
  setupLimits();

  setEnabled(true);
  setupSteppers();

  Serial.println(F("MotionCtrl4x v1.4 (limits+homing+self-test+watchdog) @115200"));
  Serial.println(F("Expecting: <A1><Axis1a>~<A2><Axis2a>~<A3><Axis3a>~<A4><Axis4a>~"));
  Serial.println(F("Mapping: Axis1->FR, Axis2->RR, Axis3->FL, Axis4->RL"));
  Serial.println(F("Homing... keep actuators clear."));

  homeAll();

  // Delay watchdog trip during self-test:
  lastCmdMs = millis();

  // ---- Initiation Self-Test ----
  selfTestAll();

  Serial.println(F("Ready for SimTools control."));
}

void loop() {
  sampleLimitsDebounced();
  enforceHardLimits();

  parseIncoming();

  // Watchdog check AFTER parse, before motion update
  watchdogCheck();

  updateMoves();

  for (int i = 0; i < 4; ++i) steppers[i].run();
}
