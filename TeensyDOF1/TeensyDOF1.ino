// 3DOF by Andrey Zhuravlev
// e-mail: v.azhure@gmail.com
// version from 2025-06-21
// Teensy 4.1 single-board adaptation

//#define INVERTED_DIR

//#define DEBUG
// Board: Teensy 4.1 (single board)
// Upload: USB
#include <stdio.h>

// Uncomment this line if you using SF1610
#define SFU1610 // Used only in SLAVE devices

// This sketch runs everything on a single Teensy 4.1 (no I2C master/slave split).
// It preserves the serial command protocol so SimHub can control the actuator.

// Maximal number of linear actuators
#define MAX_LINEAR_ACTUATORS 1
// Number of linear actuators
#define LINEAR_ACTUATORS MAX_LINEAR_ACTUATORS

// Used in serial responses for state (kept for compatibility with existing tools)
#define DEVICE_ADDR 10

template<class T>
const T& clamp(const T& x, const T& a, const T& b) {
  if (x < a) {
    return a;
  } else if (b < x) {
    return b;
  } else
    return x;
}

#define SERIAL_BAUD_RATE 115200

// override Serial Port buffer size
#define SERIAL_TX_BUFFER_SIZE 512
#define SERIAL_RX_BUFFER_SIZE 512

#define LED_PIN LED_BUILTIN

enum MODE : uint8_t { UNKNOWN,
                      CONNECTED,
                      DISABLED,
                      HOMEING,
                      PARKING,
                      READY,
                      ALARM
};

enum COMMAND : uint8_t { CMD_HOME,
                         CMD_MOVE,
                         CMD_SET_SPEED,
                         CMD_DISABLE,
                         CMD_ENABLE,
                         CMD_GET_STATE,
                         CMD_CLEAR_ALARM,
                         CMD_PARK,
                         SET_ALARM,
                         CMD_SET_SLOW_SPEED,
                         CMD_SET_ACCEL,
                         CMD_MOVE_SH
};

enum FLAGS : uint8_t { NONE = 0,
                       STATE_ON_LIMIT_SWITCH = 1,
                       STATE_HOMED = 1 << 1,
};

struct STATE {
  MODE mode;
  FLAGS flags;
  uint8_t speedMMperSEC;
  int32_t currentpos;
  int32_t targetpos;
  int32_t min;
  int32_t max;
};

const int STATE_LEN = sizeof(STATE);

struct PCCMD {
  uint8_t header = 0;
  uint8_t len;  // len
  COMMAND cmd;
  uint8_t reserved;
  int32_t data[MAX_LINEAR_ACTUATORS];
} pccmd;

// FOR SimHub
struct PCCMD_SH {
  uint8_t header = 0;
  uint8_t len;  // len
  COMMAND cmd;
  uint8_t reserved;
  uint16_t data[MAX_LINEAR_ACTUATORS];
  uint16_t data2[MAX_LINEAR_ACTUATORS];  // EMPTY
};

PCCMD_SH& pccmd_sh = *(PCCMD_SH*)&pccmd;

const int RAW_DATA_LEN = sizeof(PCCMD);

volatile MODE mode;

#ifdef INVERTED_DIR
const uint32_t dirPin = 4;
const uint32_t stepPin = 5;
#else
const uint32_t dirPin = 5;
const uint32_t stepPin = 4;
#endif
const uint32_t limiterPinNO = 6;
const uint8_t limiterPinNC = 7;

#define ANALOG_INPUT_MAX 4095

uint32_t accel = 10000;
#define STEPS_CONTROL_DIST STEPS_PER_REVOLUTIONS / 4  // Distance in steps

#ifdef SFU1610
const float MM_PER_REV = 10.0f;                                // distance in mm per revolution
const float MAX_REVOLUTIONS = 8.75;                            // maximum revolutions
const int32_t STEPS_PER_REVOLUTIONS = 2000;                    // Steps per revolution
const int32_t SAFE_DIST_IN_STEPS = STEPS_PER_REVOLUTIONS / 4;  // Safe traveling distance in steps
#define MAX_SPEED_MM_SEC 240  // maximum speed mm/sec
#else
const float MM_PER_REV = 5.0f;                                 // distance in mm per revolution
const float MAX_REVOLUTIONS = 17.5;                            // maximum revolutions
const int32_t STEPS_PER_REVOLUTIONS = 1000;                    // Steps per revolution
const int32_t SAFE_DIST_IN_STEPS = STEPS_PER_REVOLUTIONS / 2;  // Safe traveling distance in steps
#define MAX_SPEED_MM_SEC 120  // maximum speed mm/sec
#endif

const int32_t RANGE = (int32_t)(MAX_REVOLUTIONS * STEPS_PER_REVOLUTIONS);  // Maximum traveling distance, steps
const int32_t MIN_POS = SAFE_DIST_IN_STEPS;                                // minimal controlled position, steps
const int32_t MAX_POS = RANGE - SAFE_DIST_IN_STEPS;                        // maximal controlled position, steps
const uint8_t HOME_DIRECTION = HIGH;

#define MIN_PULSE_DELAY 10  // Minimal pulse interval, us

#define MIN_REVERSE_DELAY 6  // Delay between DIR and STEP signal on direction change, us

#define MMPERSEC2DELAY(mmps) 1000000 / (STEPS_PER_REVOLUTIONS * mmps / MM_PER_REV)

#define MIN_SPEED_MM_SEC 10
#define SLOW_SPEED_MM_SEC 10
#define DEFAULT_SPEED_MM_SEC 90

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

const int HOMEING_PULSE_DELAY = MAX(MIN_PULSE_DELAY, (int)MMPERSEC2DELAY(MIN_SPEED_MM_SEC) - MIN_PULSE_DELAY);     // us
const int FAST_PULSE_DELAY = MAX(MIN_PULSE_DELAY, (int)MMPERSEC2DELAY(DEFAULT_SPEED_MM_SEC) - MIN_PULSE_DELAY);    // us
const int SLOW_PULSE_DELAY = MAX(FAST_PULSE_DELAY * 2, (int)MMPERSEC2DELAY(SLOW_SPEED_MM_SEC) - MIN_PULSE_DELAY);  // us

volatile int iFastPulseDelay = FAST_PULSE_DELAY;
volatile int iSlowPulseDelay = SLOW_PULSE_DELAY;
int iFastPulseDelayMM = MAX_SPEED_MM_SEC;

volatile int32_t targetPos = (MIN_POS + MAX_POS) / 2;
uint8_t limitSwitchState = HIGH;
volatile bool bHomed = false;
int32_t currentPos = 0;
uint8_t _oldDir = LOW;  // previous DIR state

volatile bool LimitChanged = true;
uint32_t last_step_time = 0;
double pulse = iSlowPulseDelay;

inline void Step(uint8_t dir, int delay) {
  if (limitSwitchState == HIGH && dir == HOME_DIRECTION)
    return;  // ON LIMIT SWITCH

  digitalWrite(dirPin, dir);
  if (_oldDir != dir) {
    last_step_time = 0;
    delayMicroseconds(MIN_REVERSE_DELAY);
    _oldDir = dir;
  }

  double delta = MIN(MAX((micros() - last_step_time), 1), iSlowPulseDelay);

  double acc_pulse = pulse - (double)accel * delta / 1000000.0;
  pulse = acc_pulse > delay ? acc_pulse : delay;

  last_step_time = micros();
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(MIN_PULSE_DELAY);
  digitalWrite(stepPin, LOW);
  delayMicroseconds((uint32_t)pulse);
  currentPos += dir == HIGH ? 1 : -1;
}

// Limit switch event
void OnLimitSwitch() {
  LimitChanged = true;
}

void ApplyCommand(COMMAND cmd, uint32_t data) {
  switch (cmd) {
    case COMMAND::CMD_HOME:
      if (mode != MODE::HOMEING) {
        mode = MODE::HOMEING;
        currentPos = 0;
        targetPos = RANGE * 1.2;
        bHomed = false;
      }
      break;
    case COMMAND::CMD_PARK:
      if (bHomed) {
        mode = MODE::PARKING;
      }
      break;
    case COMMAND::CMD_MOVE:
      if (mode == MODE::READY)
        targetPos = clamp(data, (uint32_t)MIN_POS, (uint32_t)MAX_POS);
      break;
    case COMMAND::CMD_CLEAR_ALARM:
    case COMMAND::CMD_ENABLE:
      {
        LimitChanged = true;
        if (bHomed)
          mode = MODE::READY;
        else
          mode = MODE::CONNECTED;
      }
      break;
    case COMMAND::CMD_DISABLE:
      mode = MODE::DISABLED;
      break;
    case COMMAND::CMD_SET_SPEED:
      iFastPulseDelayMM = data = clamp((int)data, MIN_SPEED_MM_SEC, MAX_SPEED_MM_SEC);
      iFastPulseDelay = MAX(MIN_PULSE_DELAY, (int)MMPERSEC2DELAY(data) - MIN_PULSE_DELAY);  // us
      break;
    case COMMAND::CMD_SET_SLOW_SPEED:
      iSlowPulseDelay = MAX(iFastPulseDelay, (int)MMPERSEC2DELAY(data) - MIN_PULSE_DELAY);  // us
      break;
    case COMMAND::CMD_SET_ACCEL:
      accel = data;
      break;
    case COMMAND::SET_ALARM:
      bHomed = false;
      mode = MODE::ALARM;
      break;
    default:
      break;
  }
}

// Initializtion
void setup() {
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(limiterPinNO, INPUT);
  pinMode(limiterPinNC, INPUT);
  pinMode(LED_PIN, OUTPUT);
  targetPos = (MIN_POS + MAX_POS) / 2;

  limitSwitchState = digitalRead(limiterPinNO);

  attachInterrupt(digitalPinToInterrupt(limiterPinNO), OnLimitSwitch, CHANGE);

  mode = MODE::UNKNOWN;
  LimitChanged = true;
  currentPos = 0;

  Serial.begin(SERIAL_BAUD_RATE);
  while (!Serial)
    ;  // whait connected
}

int inc = -1;

#define CMD_ID 0
// serial input buffer
uint8_t buf[RAW_DATA_LEN * 2];
int offset = 0;
volatile bool _bDataPresent = false;
unsigned long _lasttime;

void serialEvent() {
  int data_cnt = min(Serial.available(), RAW_DATA_LEN);
  if (data_cnt < 2)
    return;
  for (int t = 0; t < data_cnt; ++t) {
    int byte = Serial.read();
    if (offset > 0) {
      buf[offset++] = byte;
      if (offset == RAW_DATA_LEN) {
        memcpy(&pccmd, buf, RAW_DATA_LEN);
        _bDataPresent = true;
        _lasttime = millis();
        offset = 0;
      }
    } else {
      if (byte == CMD_ID) {
        if (Serial.peek() == RAW_DATA_LEN) {
          buf[offset++] = CMD_ID;
        }
      }
    }
  }
}

STATE BuildState() {
  if (mode == MODE::UNKNOWN)
    mode = MODE::CONNECTED;
  STATE state = { mode, (FLAGS)((limitSwitchState == HIGH ? FLAGS::STATE_ON_LIMIT_SWITCH : 0) | (bHomed ? FLAGS::STATE_HOMED : 0)), (uint8_t)iFastPulseDelayMM, currentPos, targetPos, MIN_POS, MAX_POS };
  return state;
}

// Main function
void loop() {
  if (LimitChanged) {
    LimitChanged = false;
    limitSwitchState = digitalRead(limiterPinNO);
    digitalWrite(LED_PIN, !limitSwitchState);
  }

  switch (mode) {
    case MODE::HOMEING:
      {
        if (limitSwitchState == HIGH)  // SITTING ON SWITH
        {
          for (int32_t t = 0; t < SAFE_DIST_IN_STEPS; t++) {
            Step(!HOME_DIRECTION, HOMEING_PULSE_DELAY);
          }

          currentPos = MAX_POS;
          targetPos = (MIN_POS + MAX_POS) / 2;
          // Moving to the center postion at homing speed
          while (targetPos != currentPos)
            Step(targetPos > currentPos ? HIGH : LOW, HOMEING_PULSE_DELAY);

          mode = MODE::READY;
          bHomed = true;

          break;
        }

        digitalWrite(LED_PIN, (millis() % 500) > 250 ? HIGH : LOW);

        if (abs(currentPos) >= RANGE * 1.2)  // SWITCH NOT FOUND
        {
          mode = MODE::ALARM;
          bHomed = false;
          break;
        }
        Step(HOME_DIRECTION, HOMEING_PULSE_DELAY);
      }
      break;
    case MODE::PARKING:
      {
        targetPos = MIN_POS;
        while (targetPos != currentPos)
          Step(targetPos > currentPos ? HIGH : LOW, HOMEING_PULSE_DELAY);
        mode = MODE::READY;
      }
      break;
    case MODE::READY:
      {
        if (targetPos != currentPos) {
          long dist = constrain(abs(targetPos - currentPos), 0, STEPS_CONTROL_DIST);
          int delay = map(dist, 0, STEPS_CONTROL_DIST, iSlowPulseDelay, iFastPulseDelay);
          Step(targetPos > currentPos ? HIGH : LOW, delay);
        }
      }
      break;
    case MODE::ALARM:
      digitalWrite(LED_PIN, (millis() % 250) > 125 ? HIGH : LOW);
      break;
    default: break;
  }

  if (Serial.available())
    serialEvent();

  if (_bDataPresent) {
    digitalWrite(LED_PIN, LOW);  // Turn LED on
    switch (pccmd.cmd) {
      case COMMAND::CMD_HOME:
      case COMMAND::CMD_ENABLE:
      case COMMAND::CMD_DISABLE:
      case COMMAND::CMD_CLEAR_ALARM:
        if (pccmd.data[0] == 1)
          ApplyCommand(pccmd.cmd, 0);
        break;
      case COMMAND::CMD_MOVE_SH:
        {
          uint16_t val = pccmd_sh.data[0];
          val = (val >> 8) | (val << 8);
          uint32_t target = map(val, 0, 65535, MIN_POS, MAX_POS);
          ApplyCommand(COMMAND::CMD_MOVE, target);
        }
        break;
      case COMMAND::CMD_PARK:
      case COMMAND::CMD_MOVE:
      case COMMAND::CMD_SET_SPEED:
      case COMMAND::CMD_SET_SLOW_SPEED:
      case COMMAND::CMD_SET_ACCEL:
        ApplyCommand(pccmd.cmd, pccmd.data[0]);
        break;
      case COMMAND::CMD_GET_STATE:
        {
          STATE state = BuildState();
          Serial.write(DEVICE_ADDR);
          Serial.write(STATE_LEN);
          Serial.write((uint8_t*)&state, STATE_LEN);
        }
        break;
      default:
        break;
    }
    _bDataPresent = false;
    digitalWrite(LED_PIN, HIGH);  // Turn LED off
  }
}
