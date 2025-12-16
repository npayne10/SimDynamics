// DynamicHubController.ino
// Arduino Mega sketch to receive actuator commands from DynamicHub and
// drive four step/dir HBS86 drivers connected to NEMA 34 steppers.
// Features:
//   - Parses "tick,pos1,pos2,pos3,pos4" lines (integers in millimeters)
//   - Supports HOMING via limit switches and SELFTEST routines via serial commands
//   - Generates non-blocking step pulses with simple speed limiting

#include <Arduino.h>

// ---- User-adjustable constants ----
static const uint8_t NUM_ACTUATORS = 4;
static const float STEPS_PER_MM = 80.0f;          // Adjust for leadscrew pitch and microstepping
static const float MAX_SPEED_MM_S = 120.0f;       // Speed cap used for streamed targets
static const float HOME_SPEED_MM_S = 40.0f;       // Slower homing sweep
static const uint16_t STEP_PULSE_US = 4;          // Width of the step pulse
static const long SELF_TEST_TRAVEL_MM = 50;       // Distance used during self-test

struct ActuatorPins {
  uint8_t step;
  uint8_t dir;
  uint8_t enable;
  uint8_t limit; // Active LOW limit switch at home position
  bool invertDir;
};

struct ActuatorState {
  long targetSteps = 0;
  long currentSteps = 0;
  unsigned long lastStepMicros = 0;
  bool homed = false;
};

// Update these pin assignments to match your wiring.
static const ActuatorPins ACTUATOR_PINS[NUM_ACTUATORS] = {
  // step, dir, enable, limit, invertDir
  {2, 5, 8, 22, false},
  {3, 6, 9, 24, false},
  {4, 7, 10, 26, false},
  {12, 13, 11, 28, false},
};

ActuatorState actuators[NUM_ACTUATORS];
char serialLine[128];

// ---- Helpers ----
unsigned long speedToIntervalMicros(float speedMmS) {
  if (speedMmS <= 0.0f) return 0;
  float stepsPerSec = STEPS_PER_MM * speedMmS;
  if (stepsPerSec < 1.0f) stepsPerSec = 1.0f;
  return (unsigned long)(1000000.0f / stepsPerSec);
}

void pulseStep(uint8_t stepPin) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(STEP_PULSE_US);
  digitalWrite(stepPin, LOW);
}

void enableDrivers(bool enabled) {
  for (uint8_t i = 0; i < NUM_ACTUATORS; ++i) {
    digitalWrite(ACTUATOR_PINS[i].enable, enabled ? LOW : HIGH); // LOW enables for many drivers
  }
}

// ---- Motion primitives ----
void serviceActuator(uint8_t idx, unsigned long now) {
  ActuatorState &state = actuators[idx];
  const ActuatorPins &pins = ACTUATOR_PINS[idx];

  if (!state.homed) {
    return; // ignore streamed moves until homed
  }

  long delta = state.targetSteps - state.currentSteps;
  if (delta == 0) return;

  bool forward = delta > 0;
  bool directionHigh = forward ^ pins.invertDir;
  digitalWrite(pins.dir, directionHigh ? HIGH : LOW);

  unsigned long interval = speedToIntervalMicros(MAX_SPEED_MM_S);
  if (interval == 0) return;
  if (now - state.lastStepMicros < interval) return;

  // Safety: prevent driving further negative if limit is hit
  if (!forward && digitalRead(pins.limit) == LOW) {
    state.targetSteps = state.currentSteps; // clamp
    return;
  }

  pulseStep(pins.step);
  state.lastStepMicros = now;
  state.currentSteps += forward ? 1 : -1;
}

void driveUntilSettled() {
  bool moving;
  do {
    moving = false;
    unsigned long now = micros();
    for (uint8_t i = 0; i < NUM_ACTUATORS; ++i) {
      ActuatorState &state = actuators[i];
      long delta = state.targetSteps - state.currentSteps;
      if (delta != 0) {
        moving = true;
      }
      serviceActuator(i, now);
    }
  } while (moving);
}

// ---- Homing and self-test ----
void homeActuator(uint8_t idx) {
  ActuatorState &state = actuators[idx];
  const ActuatorPins &pins = ACTUATOR_PINS[idx];

  digitalWrite(pins.dir, pins.invertDir ? HIGH : LOW); // move toward switch
  unsigned long interval = speedToIntervalMicros(HOME_SPEED_MM_S);
  if (interval == 0) interval = 2000;

  while (digitalRead(pins.limit) != LOW) {
    unsigned long now = micros();
    if (now - state.lastStepMicros >= interval) {
      pulseStep(pins.step);
      state.lastStepMicros = now;
      if (state.currentSteps > -500000) { // basic runaway guard
        state.currentSteps--;
      }
    }
  }

  state.currentSteps = 0;
  state.targetSteps = 0;
  state.homed = true;
}

void homeAllActuators() {
  Serial.println(F("HOMING"));
  for (uint8_t i = 0; i < NUM_ACTUATORS; ++i) {
    homeActuator(i);
  }
  Serial.println(F("HOMED"));
}

void runSelfTest() {
  Serial.println(F("SELFTEST_START"));
  if (!actuators[0].homed || !actuators[1].homed || !actuators[2].homed || !actuators[3].homed) {
    homeAllActuators();
  }

  long testSteps = (long)(SELF_TEST_TRAVEL_MM * STEPS_PER_MM);
  long sequence[] = {testSteps, -testSteps, 0};

  for (unsigned int phase = 0; phase < (sizeof(sequence) / sizeof(sequence[0])); ++phase) {
    long offset = sequence[phase];
    for (uint8_t i = 0; i < NUM_ACTUATORS; ++i) {
      actuators[i].targetSteps = offset;
    }
    driveUntilSettled();
    delay(500);
  }

  Serial.println(F("SELFTEST_DONE"));
}

// ---- Command parsing ----
void parseStreamCommand(char *line) {
  // Expected: tick,pos1,pos2,pos3,pos4
  const uint8_t expectedFields = NUM_ACTUATORS + 1;
  long values[expectedFields];
  uint8_t parsed = 0;

  char *token = strtok(line, ",");
  while (token != nullptr && parsed < expectedFields) {
    values[parsed++] = atol(token);
    token = strtok(nullptr, ",");
  }

  if (parsed != expectedFields) {
    Serial.println(F("ERR_BAD_FIELDS"));
    return;
  }

  for (uint8_t i = 0; i < NUM_ACTUATORS; ++i) {
    actuators[i].targetSteps = (long)(values[i + 1] * STEPS_PER_MM);
  }
}

void processSerialLine(char *line) {
  if (strcmp(line, "HOME") == 0) {
    homeAllActuators();
    return;
  }
  if (strcmp(line, "SELFTEST") == 0) {
    runSelfTest();
    return;
  }
  parseStreamCommand(line);
}

void readSerial() {
  if (!Serial.available()) return;
  size_t len = Serial.readBytesUntil('\n', serialLine, sizeof(serialLine) - 1);
  if (len == 0 || len >= sizeof(serialLine)) return;
  serialLine[len] = '\0';

  // trim carriage returns
  while (len > 0 && (serialLine[len - 1] == '\r' || serialLine[len - 1] == '\n')) {
    serialLine[len - 1] = '\0';
    len--;
  }

  if (len == 0) return;
  processSerialLine(serialLine);
}

// ---- Arduino lifecycle ----
void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < NUM_ACTUATORS; ++i) {
    pinMode(ACTUATOR_PINS[i].step, OUTPUT);
    pinMode(ACTUATOR_PINS[i].dir, OUTPUT);
    pinMode(ACTUATOR_PINS[i].enable, OUTPUT);
    pinMode(ACTUATOR_PINS[i].limit, INPUT_PULLUP);
    actuators[i].currentSteps = 0;
    actuators[i].targetSteps = 0;
  }

  enableDrivers(true);
  Serial.println(F("READY"));
}

void loop() {
  readSerial();

  unsigned long now = micros();
  for (uint8_t i = 0; i < NUM_ACTUATORS; ++i) {
    serviceActuator(i, now);
  }
}

