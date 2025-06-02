/*
 * ---------------------------------------------------------------
 * Polar Alignment Motor Controller – FYSETC E4 V1.0 (ESP32 + TMC2209)
 * Author: Antonino Nicoletti + ChatGPT (June 2025)
 * ---------------------------------------------------------------
 *
 * 🧩 REQUIRED LIBRARY:
 *   - TMCStepper: https://github.com/teemuatlut/TMCStepper
 *     → Install via Arduino IDE > Tools > Manage Libraries
 *
 * 🛠️ ARDUINO IDE SETTINGS:
 *   - Board:              “ESP32 Dev Module”
 *   - Port:               "/dev/cu.wchusbserial10" (may vary)
 *   - CPU Frequency:      “240 MHz (WiFi/BT)”
 *   - Flash Frequency:    “40 MHz”
 *   - Flash Mode:         “DIO”
 *   - Flash Size:         “4MB (32Mb)”
 *   - Partition Scheme:   “Huge APP (3MB No OTA/1MB SPIFFS)”
 *   - PSRAM:              “Enabled”
 *   - Core Debug Level:   “None”
 *   - Upload Speed:       “115200”
 *
 * 🔌 MOTOR CONNECTIONS:
 *   - AZM motor → MOTX port
 *     • STEP = GPIO27, DIR = GPIO26, EN = GPIO25
 *     • UART = GPIO39 (Z-MIN → PDN)
 *
 *   - ALT motor → MOTY port
 *     • STEP = GPIO33, DIR = GPIO32, EN = GPIO25 (shared)
 *     • UART = GPIO36 (Y-MIN → PDN)
 *
 * 📡 SUPPORTED SERIAL COMMANDS (115200 baud):
 *
 * 🆕 TPPA-Compatible:
 *   - `?`                  → Query status. Reply: `<Idle|MPos:x,y,z|` (followed by blank line)
 *   - `J=G53X1.5F300`      → Absolute move AZM to 1.5° at speed 300 steps/sec
 *   - `J=G53Y-1.2F200`     → Absolute move ALT to -1.2° at speed 200
 *   - `J=G91G21X0.8F150`   → Relative move AZM +0.8° at speed 150
 *   - `J=G91G21Y-0.5F100`  → Relative move ALT -0.5° at speed 100
 *
 * 🔁 Legacy / Debug:
 *   - `AZM:+2.5`           → Move azimuth +2.5°
 *   - `AZM:-1.0`           → Move azimuth -1.0°
 *   - `ALT:+0.5`           → Move altitude +0.5°
 *   - `ALT:-0.2`           → Move altitude -0.2°
 *   - `POS?` or `STA?`     → Query current positions
 *   - `RST`                → Reset both positions to 0
 *
 * 🔙 RESPONSES:
 *   - `OK`                 → Command successful
 *   - `<Idle|MPos:x,y,z|`  → Status (for `?`)
 *   - `ERR:<reason>`       → Invalid command or limits
 */

#include <TMCStepper.h>

// --- SYSTEM CONFIG ---
const float MOTOR_STEPS_PER_REV = 200.0;
const float MICROSTEPPING = 16.0;
const float GEAR_RATIO_AZM = 100.0;
const float GEAR_RATIO_ALT = 90.0;

const float STEPS_PER_DEGREE_AZM = (MOTOR_STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_AZM) / 360.0;
const float STEPS_PER_DEGREE_ALT = (MOTOR_STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_ALT) / 360.0;

const float LIMIT_MIN_AZM = -10.0;
const float LIMIT_MAX_AZM =  10.0;
const float LIMIT_MIN_ALT = -15.0;
const float LIMIT_MAX_ALT =  15.0;

// --- PINOUT ---
#define EN_PIN_AZM     25
#define DIR_PIN_AZM    26
#define STEP_PIN_AZM   27
#define SERIAL_RX_AZM  39

#define EN_PIN_ALT     25  // shared
#define DIR_PIN_ALT    32
#define STEP_PIN_ALT   33
#define SERIAL_RX_ALT  36

#define DRIVER_ADDRESS 0
#define R_SENSE        0.11f

HardwareSerial AZMSerial(2);
HardwareSerial ALTSerial(1);
TMC2209Stepper driverAZM(&AZMSerial, R_SENSE, DRIVER_ADDRESS);
TMC2209Stepper driverALT(&ALTSerial, R_SENSE, DRIVER_ADDRESS);

// --- STATE ---
float currentPosAZM = 0.0;
float currentPosALT = 0.0;

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(EN_PIN_AZM, OUTPUT); pinMode(DIR_PIN_AZM, OUTPUT); pinMode(STEP_PIN_AZM, OUTPUT);
  pinMode(DIR_PIN_ALT, OUTPUT); pinMode(STEP_PIN_ALT, OUTPUT);
  digitalWrite(EN_PIN_AZM, LOW); digitalWrite(EN_PIN_ALT, LOW);

  AZMSerial.begin(115200, SERIAL_8N1, SERIAL_RX_AZM, -1);
  ALTSerial.begin(115200, SERIAL_8N1, SERIAL_RX_ALT, -1);

  driverAZM.begin(); driverAZM.pdn_disable(true); driverAZM.I_scale_analog(false);
  driverAZM.rms_current(500); driverAZM.microsteps(MICROSTEPPING); driverAZM.en_spreadCycle(false);

  driverALT.begin(); driverALT.pdn_disable(true); driverALT.I_scale_analog(false);
  driverALT.rms_current(500); driverALT.microsteps(MICROSTEPPING); driverALT.en_spreadCycle(false);

  Serial.println("READY");
}

// --- MOTOR MOVE ---
void moveMotor(int stepPin, int dirPin, float degrees, float stepsPerDegree, float &pos, float speedDegPerSec = 200.0) {
  long steps = round(degrees * stepsPerDegree);
  if (steps == 0) return;

  digitalWrite(dirPin, steps > 0 ? HIGH : LOW);
  steps = abs(steps);
  int delay_us = 1e6 / (speedDegPerSec * stepsPerDegree);

  for (long i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH); delayMicroseconds(delay_us / 2);
    digitalWrite(stepPin, LOW);  delayMicroseconds(delay_us / 2);
  }
  pos += degrees;
}

// --- MAIN LOOP ---
void loop() {
  if (!Serial.available()) return;
  String input = Serial.readStringUntil('\n');
  input.trim();

  // TPPA STATUS QUERY
  if (input == "?") {
    Serial.print("<Idle|MPos:");
    Serial.print(currentPosAZM, 3); Serial.print(",");
    Serial.print(currentPosALT, 3); Serial.println(",0|");
    Serial.println();
    return;
  }

  // LEGACY COMMANDS
  if (input.startsWith("AZM:")) {
    float d = input.substring(4).toFloat();
    float newPos = currentPosAZM + d;
    if (newPos >= LIMIT_MIN_AZM && newPos <= LIMIT_MAX_AZM) {
      moveMotor(STEP_PIN_AZM, DIR_PIN_AZM, d, STEPS_PER_DEGREE_AZM, currentPosAZM);
      Serial.println("OK");
    } else Serial.println("ERR:AZM out of bounds");
    return;
  }

  if (input.startsWith("ALT:")) {
    float d = input.substring(4).toFloat();
    float newPos = currentPosALT + d;
    if (newPos >= LIMIT_MIN_ALT && newPos <= LIMIT_MAX_ALT) {
      moveMotor(STEP_PIN_ALT, DIR_PIN_ALT, d, STEPS_PER_DEGREE_ALT, currentPosALT);
      Serial.println("OK");
    } else Serial.println("ERR:ALT out of bounds");
    return;
  }

  if (input == "POS?" || input == "STA?") {
    Serial.print("POS:AZM="); Serial.print(currentPosAZM, 3);
    Serial.print(",ALT=");   Serial.println(currentPosALT, 3);
    return;
  }

  if (input == "RST") {
    currentPosAZM = 0.0; currentPosALT = 0.0;
    Serial.println("OK");
    return;
  }

  // TPPA MOVE COMMANDS
  if (input.startsWith("J=")) {
    input.remove(0, 2);
    bool isRelative = input.startsWith("G91");
    input.replace("G91", ""); input.replace("G21", ""); input.replace("G53", "");

    char axis = input.charAt(0);
    int fIndex = input.indexOf('F');
    if (fIndex == -1) { Serial.println("ERR:Missing F"); return; }

    float value = input.substring(1, fIndex).toFloat();
    float speed = input.substring(fIndex + 1).toFloat();
    float *pos = nullptr;
    float stepsPerDeg = 0;
    int stepPin = 0, dirPin = 0;

    if (axis == 'X') {
      pos = &currentPosAZM; stepsPerDeg = STEPS_PER_DEGREE_AZM;
      stepPin = STEP_PIN_AZM; dirPin = DIR_PIN_AZM;
    }
    else if (axis == 'Y') {
      pos = &currentPosALT; stepsPerDeg = STEPS_PER_DEGREE_ALT;
      stepPin = STEP_PIN_ALT; dirPin = DIR_PIN_ALT;
    }
    else { Serial.println("ERR:Unknown axis"); return; }

    float target = isRelative ? (*pos + value) : value;
    float limitMin = (axis == 'X') ? LIMIT_MIN_AZM : LIMIT_MIN_ALT;
    float limitMax = (axis == 'X') ? LIMIT_MAX_AZM : LIMIT_MAX_ALT;

    if (target < limitMin || target > limitMax) {
      Serial.print("ERR:"); Serial.print(axis); Serial.println(" out of bounds");
      return;
    }

    moveMotor(stepPin, dirPin, target - *pos, stepsPerDeg, *pos, speed);
    Serial.println("OK");
    return;
  }

  Serial.println("ERR:Unknown command");
}
