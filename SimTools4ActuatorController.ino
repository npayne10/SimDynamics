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

// Mechanics
static const float LEAD_MM_PER_REV = 5.0f;
static const int   STEPS_PER_REV   = 200;
static const int   MICROSTEPS      = 16;
static const float STEPS_PER_MM    = (STEPS_PER_REV * MICROSTEPS) / LEAD_MM_PER_REV;

static const float STROKE_MM       = 150.0f;   // total travel
static const float MIN_POS_MM      = 0.0f;     // home at bottom limit
static const float MAX_POS_MM      = MIN_POS_MM + STROKE_MM;

// Motion settings
static const float MAX_SPEED_MM_S  = 60.0f;
static const float ACCEL_MM_S2     = 600.0f;
static const float HOME_SPEED_MM_S = 20.0f;    // approach speed towards bottom
static const float HOME_BACKOFF_MM = 3.0f;     // lift off switch after homing

// Serial
static const uint32_t SERIAL_BAUD = 115200;
static const bool SERIAL_ECHO = false;

// ----------------------------------------------------------------------------
static const float MAX_SPEED_STEPS_S = MAX_SPEED_MM_S * STEPS_PER_MM;
static const float ACCEL_STEPS_S2    = ACCEL_MM_S2 * STEPS_PER_MM;

AccelStepper steppers[AXES] = {
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[0], DIR_PIN[0]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[1], DIR_PIN[1]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[2], DIR_PIN[2]),
  AccelStepper(AccelStepper::DRIVER, STEP_PIN[3], DIR_PIN[3])
};

float targetMM[AXES] = { MIN_POS_MM, MIN_POS_MM, MIN_POS_MM, MIN_POS_MM };
long  targetSteps[AXES] = { 0, 0, 0, 0 };

bool minLimitState[AXES] = { false, false, false, false };
unsigned long lastLimitSample = 0;

String serialBuffer;

// ----------------------------------------------------------------------------
static inline float mmToSteps(float mm) { return mm * STEPS_PER_MM; }
static inline float stepsToMM(long steps) { return steps / STEPS_PER_MM; }

static inline float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
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
  float homeSpeedSteps = HOME_SPEED_MM_S * STEPS_PER_MM;
  steppers[axis].setMaxSpeed(homeSpeedSteps);
  steppers[axis].setAcceleration(ACCEL_STEPS_S2);
  steppers[axis].setSpeed(-homeSpeedSteps);

  // move downwards until the limit closes
  while (!minLimitState[axis]) {
    steppers[axis].runSpeed();
    sampleLimits();
  }

  // reached bottom -> set current position to zero
  steppers[axis].setCurrentPosition(lround(mmToSteps(MIN_POS_MM)));

  // lift up slightly to release switch
  long backoffSteps = lround(mmToSteps(HOME_BACKOFF_MM));
  steppers[axis].move(backoffSteps);
  while (steppers[axis].distanceToGo() != 0) {
    steppers[axis].run();
    sampleLimits();
  }

  float postHome = MIN_POS_MM + HOME_BACKOFF_MM;
  targetMM[axis] = postHome;
  targetSteps[axis] = lround(mmToSteps(postHome));
  steppers[axis].setMaxSpeed(MAX_SPEED_STEPS_S);
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

static float simtoolsValueToMM(int value) {
  value = constrain(value, 0, 255);
  float t = value / 255.0f;
  return MIN_POS_MM + t * (MAX_POS_MM - MIN_POS_MM);
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

      static uint8_t axis = 0;
      int axisValue = extractAxisValue(token);
      if (axisValue >= 0 && axis < AXES) {
        float mm = simtoolsValueToMM(axisValue);
        targetMM[axis] = clampFloat(mm, MIN_POS_MM, MAX_POS_MM);
        axis = (axis + 1) % AXES;
      }

      if (serialBuffer.length() > 64) {
        serialBuffer.remove(0, serialBuffer.length() - 16);
      }
    }
  }
}

static void updateTargets() {
  for (uint8_t i = 0; i < AXES; ++i) {
    float desired = clampFloat(targetMM[i], MIN_POS_MM, MAX_POS_MM);

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

// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {}

  for (uint8_t i = 0; i < AXES; ++i) {
    pinMode(STEP_PIN[i], OUTPUT);
    pinMode(DIR_PIN[i], OUTPUT);
    pinMode(MIN_LIMIT_PIN[i], INPUT_PULLUP);
  }

  enableDrivers(true);

  for (uint8_t i = 0; i < AXES; ++i) {
    steppers[i].setMaxSpeed(MAX_SPEED_STEPS_S);
    steppers[i].setAcceleration(ACCEL_STEPS_S2);
  }

  sampleLimits();
  homeAll();

  Serial.println(F("SimTools 4-Axis Controller ready."));
  Serial.println(F("Expecting packets: <A1><Axis1a>~<A2><Axis2a>~<A3><Axis3a>~<A4><Axis4a>~"));
}

void loop() {
  sampleLimits();
  parseSerial();
  updateTargets();

  for (uint8_t i = 0; i < AXES; ++i) {
    steppers[i].run();
  }
}
