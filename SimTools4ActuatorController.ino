#include <AccelStepper.h>
#include <Arduino.h>

// ---------------------------- USER CONFIG ----------------------------------
static const uint8_t AXES = 4;

// STEP / DIR / ENABLE pins for each HBS86H driver (NEMA34 motor)
static const uint8_t STEP_PIN[AXES] = { 22, 24, 26, 28 };
static const uint8_t DIR_PIN [AXES] = { 23, 25, 27, 29 };
static const uint8_t EN_PIN  [AXES] = { 30, 31, 32, 33 }; // set to 255 if unused

// Bottom (minimum) limit switch pins, wired NC -> GND when pressed.
static const uint8_t MIN_LIMIT_PIN[AXES] = { 34, 36, 38, 40 };
static const bool LIMIT_ACTIVE_LOW = true;  // LOW when switch is pressed
static const unsigned long LIMIT_DEBOUNCE_MS = 5;

// Car profile selection (hardware switches are active LOW)
static const uint8_t CAR_TYPE_PIN_F1   = 42;
static const uint8_t CAR_TYPE_PIN_GT3  = 43;
static const uint8_t CAR_TYPE_PIN_RALLY= 44;

struct CarProfile {
  const char *name;
  float strokeMM;
  float homePercent;   // 0.0 - 1.0 of stroke
  float stepsPerRevScale; // multiplier applied to base driver setting
};

// Base pulses-per-revolution setting on the HBS86H driver (adjust to match dip-switch configuration).
static float HBS86H_STEPS_PER_REV = 2000.0f;
static const float ACTUATOR_MAX_STROKE_MM = 85.0f; // physical travel limit of each actuator

// Adjust the stroke and stiffness scalars below to tune each vehicle profile.
static CarProfile PROFILE_F1   = { "F1 Car",   80.0f, 0.50f, 1.20f }; // slightly stiffer (more steps per mm)
static CarProfile PROFILE_GT3  = { "GT3 Car", 200.0f, 0.50f, 1.00f }; // baseline driver steps
static CarProfile PROFILE_RALLY= { "Rally Car",220.0f, 0.50f, 0.80f }; // softer (fewer steps per mm)

static const float LEAD_MM_PER_REV = 5.0f;
static const int   MICROSTEPS      = 1; // already included in the driver step configuration

static CarProfile *currentProfile = &PROFILE_GT3;

static float strokeMM       = PROFILE_GT3.strokeMM;
static float homePercent    = PROFILE_GT3.homePercent;
static float minPosMM       = 0.0f;     // home at bottom limit
static float maxPosMM       = minPosMM + strokeMM;
static float homePositionMM = minPosMM + (strokeMM * homePercent);

static float stepsPerRev    = HBS86H_STEPS_PER_REV * PROFILE_GT3.stepsPerRevScale;
static float stepsPerMM     = (stepsPerRev * MICROSTEPS) / LEAD_MM_PER_REV;

// Motion settings
static const float MAX_SPEED_MM_S  = 60.0f;
static const float ACCEL_MM_S2     = 600.0f;
static const float HOME_SPEED_MM_S = 20.0f;    // approach speed towards bottom
static const float HOME_BACKOFF_MM = 3.0f;     // lift off switch after homing

// Self-test motion envelope
static const float SELFTEST_BASE_FACTOR  = 0.5f;
static const float SELFTEST_ROLL_FACTOR  = 0.25f;
static const float SELFTEST_PITCH_FACTOR = 0.25f;
static const float SELFTEST_HEAVE_FACTOR = 0.20f;
static const unsigned long SELFTEST_PAUSE_MS = 500;

// Serial
static const uint32_t SERIAL_BAUD = 115200;
static const bool SERIAL_ECHO = false;

// Communication watchdog (SimTools inactivity timeout)
static const unsigned long COMM_WATCHDOG_TIMEOUT_MS = 60UL * 1000UL;

// ----------------------------------------------------------------------------
static float maxSpeedStepsPerSec = MAX_SPEED_MM_S * stepsPerMM;
static float accelStepsPerSec2    = ACCEL_MM_S2 * stepsPerMM;

AccelStepper steppers[AXES] = {
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[0], DIR_PIN[0]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[1], DIR_PIN[1]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[2], DIR_PIN[2]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[3], DIR_PIN[3])
};

float targetMM[AXES];
long  targetSteps[AXES];

bool minLimitState[AXES] = { false, false, false, false };
unsigned long lastLimitSample = 0;

String serialBuffer;

unsigned long lastCommandReceived = 0;
bool watchdogReturnActive = false;
uint8_t rollingAxisCursor = 0;

// ----------------------------------------------------------------------------
static inline float mmToSteps(float mm) { return mm * stepsPerMM; }
static inline float stepsToMM(long steps) { return steps / stepsPerMM; }

static inline float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static float clampToStroke(float mm) {
  return clampFloat(mm, minPosMM, maxPosMM);
}

static void updateDerivedMotionParameters() {
  if (homePercent < 0.0f) homePercent = 0.0f;
  if (homePercent > 1.0f) homePercent = 1.0f;
  stepsPerMM = (stepsPerRev * MICROSTEPS) / LEAD_MM_PER_REV;
  maxPosMM = minPosMM + strokeMM;
  homePositionMM = minPosMM + (strokeMM * homePercent);
  maxSpeedStepsPerSec = MAX_SPEED_MM_S * stepsPerMM;
  accelStepsPerSec2 = ACCEL_MM_S2 * stepsPerMM;
}

static void applyCarProfile(CarProfile *profile) {
  if (!profile) {
    return;
  }
  currentProfile = profile;
  strokeMM = min(profile->strokeMM, ACTUATOR_MAX_STROKE_MM);
  homePercent = profile->homePercent;
  stepsPerRev = HBS86H_STEPS_PER_REV * profile->stepsPerRevScale;
  updateDerivedMotionParameters();
}

static void printCurrentProfile() {
  if (!currentProfile) return;
  Serial.print(F("Selected profile: "));
  Serial.println(currentProfile->name);
  Serial.print(F("  Stroke (mm): "));
  Serial.println(strokeMM, 1);
  if (currentProfile->strokeMM > ACTUATOR_MAX_STROKE_MM) {
    Serial.print(F("  (Hardware-limited from requested "));
    Serial.print(currentProfile->strokeMM, 1);
    Serial.println(F(" mm)"));
  }
  Serial.print(F("  Home position (mm): "));
  Serial.println(homePositionMM, 1);
  Serial.print(F("  Steps/rev: "));
  Serial.println(stepsPerRev);
  Serial.print(F("  Steps/mm: "));
  Serial.println(stepsPerMM, 3);
}

static CarProfile* detectCarProfileFromSwitches() {
  if (digitalRead(CAR_TYPE_PIN_F1) == LOW) {
    return &PROFILE_F1;
  }
  if (digitalRead(CAR_TYPE_PIN_GT3) == LOW) {
    return &PROFILE_GT3;
  }
  if (digitalRead(CAR_TYPE_PIN_RALLY) == LOW) {
    return &PROFILE_RALLY;
  }
  // Default profile when no switch is asserted.
  return &PROFILE_GT3;
}

static void configureCarProfileFromSwitches() {
  pinMode(CAR_TYPE_PIN_F1, INPUT_PULLUP);
  pinMode(CAR_TYPE_PIN_GT3, INPUT_PULLUP);
  pinMode(CAR_TYPE_PIN_RALLY, INPUT_PULLUP);

  // allow signals to settle
  delay(5);

  CarProfile *selected = detectCarProfileFromSwitches();
  applyCarProfile(selected);
  printCurrentProfile();
}

static float selfTestBaseMM() {
  return clampToStroke(minPosMM + (strokeMM * SELFTEST_BASE_FACTOR));
}

static float selfTestRollMM() {
  return strokeMM * SELFTEST_ROLL_FACTOR;
}

static float selfTestPitchMM() {
  return strokeMM * SELFTEST_PITCH_FACTOR;
}

static float selfTestHeaveMM() {
  return strokeMM * SELFTEST_HEAVE_FACTOR;
}

static bool rawLimitTriggered(uint8_t pin) {
  int level = digitalRead(pin);
  return LIMIT_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
}

static void sampleLimits() {
  unsigned long now = millis();
  if (now - lastLimitSample < LIMIT_DEBOUNCE_MS) return;
  lastLimitSample = now;
  for (uint8_t i = 0; i < AXES; ++i) {
    minLimitState[i] = rawLimitTriggered(MIN_LIMIT_PIN[i]);
  }
}

static void enableDrivers(bool enable) {
  for (uint8_t i = 0; i < AXES; ++i) {
    if (EN_PIN[i] == 255) continue;
    pinMode(EN_PIN[i], OUTPUT);
    digitalWrite(EN_PIN[i], enable ? LOW : HIGH); // HBS86H ENA- active LOW
  }
}

// ----------------------------------------------------------------------------
static void homeAxis(uint8_t axis) {
  Serial.print(F("Homing axis ")); Serial.println(axis);

  // configure for homing speed towards negative direction
  float homeSpeedSteps = HOME_SPEED_MM_S * stepsPerMM;
  steppers[axis].setMaxSpeed(homeSpeedSteps);
  steppers[axis].setAcceleration(accelStepsPerSec2);
  steppers[axis].setSpeed(-homeSpeedSteps);

  // move downwards until the limit closes
  while (!minLimitState[axis]) {
    steppers[axis].runSpeed();
    sampleLimits();
  }

  // reached bottom -> set current position to zero
  steppers[axis].setCurrentPosition(lround(mmToSteps(minPosMM)));

  // lift up slightly to release switch
  long backoffSteps = lround(mmToSteps(HOME_BACKOFF_MM));
  steppers[axis].move(backoffSteps);
  while (steppers[axis].distanceToGo() != 0) {
    steppers[axis].run();
    sampleLimits();
  }

  float postHome = minPosMM + HOME_BACKOFF_MM;
  targetMM[axis] = postHome;
  targetSteps[axis] = lround(mmToSteps(postHome));
  steppers[axis].setMaxSpeed(maxSpeedStepsPerSec);
  steppers[axis].moveTo(targetSteps[axis]);
}

static void homeAll() {
  Serial.println(F("--- Homing start ---"));
  for (uint8_t i = 0; i < AXES; ++i) {
    homeAxis(i);
  }
  Serial.println(F("--- Homing complete ---"));
}

// ----------------------------------------------------------------------------
static int extractAxisValue(const String &token) {
  int n = token.length();
  int bestStart = -1;
  int bestLen = 0;

  for (int i = 0; i < n; ++i) {
    if (isDigit(token[i])) {
      int start = i;
      while (i < n && isDigit(token[i])) {
        ++i;
      }
      int len = i - start;
      if (len >= bestLen) {
        bestLen = len;
        bestStart = start;
      }
    }
  }

  if (bestStart >= 0) {
    return token.substring(bestStart, bestStart + bestLen).toInt();
  }
  return -1;
}

static bool extractAxisIndexAndValue(const String &token, uint8_t &axisIndex, int &axisValue) {
  axisIndex = 255;
  axisValue = -1;

  int prefixStart = token.indexOf("<A");
  if (prefixStart < 0) {
    return false;
  }

  int idStart = prefixStart + 2;
  int len = token.length();
  int idEnd = idStart;
  while (idEnd < len && isDigit(token[idEnd])) {
    ++idEnd;
  }
  if (idEnd == idStart) {
    return false;
  }

  long axisNumber = token.substring(idStart, idEnd).toInt();
  if (axisNumber < 1 || axisNumber > AXES) {
    return false;
  }

  int closeBracket = token.indexOf('>', idEnd);
  int valueStart = (closeBracket >= 0) ? (closeBracket + 1) : idEnd;
  String remainder = token.substring(valueStart);
  int value = extractAxisValue(remainder);
  if (value < 0) {
    return false;
  }

  axisIndex = static_cast<uint8_t>(axisNumber - 1);
  axisValue = value;
  return true;
}

static float simtoolsValueToMM(int value) {
  value = constrain(value, 0, 255);
  float t = value / 255.0f;
  return minPosMM + t * (maxPosMM - minPosMM);
}

static void setAllTargetsTo(float mm) {
  float desired = clampToStroke(mm);
  long steps = lround(mmToSteps(desired));
  for (uint8_t i = 0; i < AXES; ++i) {
    targetMM[i] = desired;
    targetSteps[i] = steps;
    steppers[i].moveTo(steps);
  }
}

static void triggerWatchdogReturn() {
  Serial.print(F("Watchdog timeout: returning actuators to home (mm): "));
  Serial.println(homePositionMM, 1);
  setAllTargetsTo(homePositionMM);
  watchdogReturnActive = true;
}

static void parseSerial() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (SERIAL_ECHO) Serial.write(c);
    serialBuffer += c;

    int tildeIndex;
    while ((tildeIndex = serialBuffer.indexOf('~')) != -1) {
      String token = serialBuffer.substring(0, tildeIndex);
      serialBuffer.remove(0, tildeIndex + 1);
      token.trim();
      if (token.length() == 0) {
        continue;
      }

      uint8_t axisIndex = 255;
      int axisValue = -1;
      bool parsed = extractAxisIndexAndValue(token, axisIndex, axisValue);
      if (!parsed) {
        axisValue = extractAxisValue(token);
        if (axisValue >= 0) {
          axisIndex = rollingAxisCursor;
          rollingAxisCursor = (rollingAxisCursor + 1) % AXES;
        }
      }

      if (axisValue >= 0 && axisIndex < AXES) {
        float mm = simtoolsValueToMM(axisValue);
        targetMM[axisIndex] = clampFloat(mm, minPosMM, maxPosMM);
        rollingAxisCursor = (axisIndex + 1) % AXES;
        lastCommandReceived = millis();
        watchdogReturnActive = false;
      }

      if (serialBuffer.length() > 64) {
        serialBuffer.remove(0, serialBuffer.length() - 16);
      }
    }
  }
}

static void updateTargets() {
  for (uint8_t i = 0; i < AXES; ++i) {
    float desired = clampFloat(targetMM[i], minPosMM, maxPosMM);

    // If bottom limit is active, do not allow further downward motion.
    if (minLimitState[i]) {
      float current = stepsToMM(steppers[i].currentPosition());
      if (desired < current) {
        desired = current;
        targetMM[i] = current;
      }
    }

    long steps = lround(mmToSteps(desired));
    if (steps != targetSteps[i]) {
      targetSteps[i] = steps;
      steppers[i].moveTo(steps);
    }
  }
}

static void moveToPositionsBlocking(const float mmTargets[AXES]) {
  for (uint8_t i = 0; i < AXES; ++i) {
    float desired = clampToStroke(mmTargets[i]);
    targetMM[i] = desired;
    long steps = lround(mmToSteps(desired));
    targetSteps[i] = steps;
    steppers[i].moveTo(steps);
  }

  bool anyMoving;
  do {
    anyMoving = false;
    sampleLimits();
    updateTargets();
    for (uint8_t i = 0; i < AXES; ++i) {
      if (steppers[i].distanceToGo() != 0) {
        anyMoving = true;
      }
      steppers[i].run();
    }
  } while (anyMoving);
}

static void pauseForSettle() {
  unsigned long start = millis();
  while (millis() - start < SELFTEST_PAUSE_MS) {
    sampleLimits();
    updateTargets();
    for (uint8_t i = 0; i < AXES; ++i) {
      steppers[i].run();
    }
  }
}

static void performLimitTest() {
  Serial.println(F("Self-test: limit sweep to top."));
  float topTargets[AXES];
  for (uint8_t i = 0; i < AXES; ++i) {
    topTargets[i] = maxPosMM;
  }
  moveToPositionsBlocking(topTargets);
  pauseForSettle();

  Serial.println(F("Self-test: limit sweep to bottom."));
  float bottomTargets[AXES];
  for (uint8_t i = 0; i < AXES; ++i) {
    bottomTargets[i] = minPosMM + HOME_BACKOFF_MM;
  }
  moveToPositionsBlocking(bottomTargets);
  pauseForSettle();
}

static void performRollTest() {
  Serial.println(F("Self-test: roll motion."));
  float base = selfTestBaseMM();
  float roll = selfTestRollMM();
  float rollUp[AXES] = {
    clampToStroke(base + roll),
    clampToStroke(base - roll),
    clampToStroke(base + roll),
    clampToStroke(base - roll)
  };
  moveToPositionsBlocking(rollUp);
  pauseForSettle();

  float rollDown[AXES] = {
    clampToStroke(base - roll),
    clampToStroke(base + roll),
    clampToStroke(base - roll),
    clampToStroke(base + roll)
  };
  moveToPositionsBlocking(rollDown);
  pauseForSettle();
}

static void performPitchTest() {
  Serial.println(F("Self-test: pitch motion."));
  float base = selfTestBaseMM();
  float pitch = selfTestPitchMM();
  float pitchForward[AXES] = {
    clampToStroke(base + pitch),
    clampToStroke(base + pitch),
    clampToStroke(base - pitch),
    clampToStroke(base - pitch)
  };
  moveToPositionsBlocking(pitchForward);
  pauseForSettle();

  float pitchBackward[AXES] = {
    clampToStroke(base - pitch),
    clampToStroke(base - pitch),
    clampToStroke(base + pitch),
    clampToStroke(base + pitch)
  };
  moveToPositionsBlocking(pitchBackward);
  pauseForSettle();
}

static void performHeaveTest() {
  Serial.println(F("Self-test: heave motion."));
  float base = selfTestBaseMM();
  float heave = selfTestHeaveMM();
  float heaveUp[AXES];
  float heaveDown[AXES];
  for (uint8_t i = 0; i < AXES; ++i) {
    heaveUp[i] = clampToStroke(base + heave);
    heaveDown[i] = clampToStroke(base - heave);
  }
  moveToPositionsBlocking(heaveUp);
  pauseForSettle();
  moveToPositionsBlocking(heaveDown);
  pauseForSettle();
}

static void runSelfTest() {
  Serial.println(F("--- Actuator self-test start ---"));
  performLimitTest();
  performRollTest();
  performPitchTest();
  performHeaveTest();

  float neutral[AXES];
  float base = selfTestBaseMM();
  for (uint8_t i = 0; i < AXES; ++i) {
    neutral[i] = clampToStroke(base);
  }
  moveToPositionsBlocking(neutral);
  pauseForSettle();
  Serial.println(F("--- Actuator self-test complete ---"));
}

// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {}

  configureCarProfileFromSwitches();

  for (uint8_t i = 0; i < AXES; ++i) {
    pinMode(STEP_PIN[i], OUTPUT);
    pinMode(DIR_PIN[i], OUTPUT);
    pinMode(MIN_LIMIT_PIN[i], INPUT_PULLUP);
  }

  enableDrivers(true);

  for (uint8_t i = 0; i < AXES; ++i) {
    steppers[i].setMaxSpeed(maxSpeedStepsPerSec);
    steppers[i].setAcceleration(accelStepsPerSec2);
  }

  setAllTargetsTo(homePositionMM);

  sampleLimits();
  homeAll();

  float homeTargets[AXES];
  for (uint8_t i = 0; i < AXES; ++i) {
    homeTargets[i] = clampToStroke(homePositionMM);
  }
  moveToPositionsBlocking(homeTargets);
  runSelfTest();

  Serial.println(F("SimTools 4-Axis Controller ready."));
  Serial.println(F("Expecting packets: <A1><Axis1a>~<A2><Axis2a>~<A3><Axis3a>~<A4><Axis4a>~"));

  lastCommandReceived = millis();
}

void loop() {
  sampleLimits();
  parseSerial();
  updateTargets();

  for (uint8_t i = 0; i < AXES; ++i) {
    steppers[i].run();
  }

  if (!watchdogReturnActive) {
    unsigned long now = millis();
    if (now - lastCommandReceived >= COMM_WATCHDOG_TIMEOUT_MS) {
      triggerWatchdogReturn();
    }
  }
}
