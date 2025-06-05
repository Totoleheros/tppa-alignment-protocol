/*****************************************************************************************
 *  POLAR-ALIGNMENT MOTOR CONTROLLER – FYSETC-E4 (ESP32 + TMC2209)
 *  ---------------------------------------------------------------------------------------
 *  Author      : Antonino Nicoletti  +  ChatGPT
 *  File        : polar-align-controller.ino
 *  License     : MIT
 *  Last update : 2025-06-05   (AZM soft-limit étendu à ±15 °)
 *
 *  PURPOSE ───────────────────────────────────────────────────────────────────────────────
 *    Drive two stepper motors (Azimuth & Altitude) for N.I.N.A.’s “Three-Point Polar
 *    Alignment” (TPPA) routine.  The board pretends to be a *minimal* GRBL-compatible
 *    controller so that TPPA can send $J-style jog commands and poll status frames.
 *
 *    • Immediate ‘ok’ acknowledgement, exactly as GRBL expects
 *    • Status frame format  <Idle|MPos:X,Y,0|
 *    • No EEPROM, homing cycle, alarms or planner queue – it is **just enough**
 *      for polar-alignment moves of ±15 ° at low speed.
 *
 *  BUILD ─────────────────────────────────────────────────────────────────────────────────
 *    Board           : “ESP32 Dev Module”            (the FYSETC-E4 uses an ESP32-WROOM-32)
 *    Upload speed    : 921 600 bps  (115 200 is safe if USB adapters misbehave)
 *    Partition scheme: Huge APP (3 MB) – no OTA needed
 *    Library         : TMCStepper (UART access to TMC2209)
 *
 *  ELECTRICAL MAP ────────────────────────────────────────────────────────────────────────
 *                 FYSETC-E4 V1.0 silk screen     ESP32 GPIO
 *                 ───────────────────────────    ──────────
 *     Azimuth:  STEP = MOT-X.STEP                27
 *               DIR  = MOT-X.DIR                 26
 *               EN   = /ENABLE (shared)          25
 *               UART = Z-MIN (PDN signal)        39  (RX-only)
 *
 *     Altitude: STEP = MOT-Y.STEP                33
 *               DIR  = MOT-Y.DIR                 32
 *               EN   = /ENABLE (shared)          25
 *               UART = Y-MIN (PDN signal)        36  (RX-only)
 *
 *  SERIAL PROTOCOL QUICK REFERENCE ──────────────────────────────────────────────────────
 *     ► Jog absolute      $J=G53X+1.0F300\n    (X ≡ Azimuth, Y ≡ Altitude)
 *     ► Jog relative      $J=G91G21Y-0.8F200\n (unit token G21 is ignored)
 *     ► Status poll       ?\n                 → <Idle|MPos:-0.500,2.375,0|
 *     ► Unlock            $X\n                → ok\n            (no alarms here)
 *     ► Feed hold / resume !  /  ~            (see TODO)
 *     ► Legacy console    AZM:+0.5  ALT:-1.0  etc.  (for USB-serial debugging)
 *
 *****************************************************************************************/

#include <TMCStepper.h>                    // Single dependency (UART driver)

/* ╔═════════════════════════ USER-TUNABLE CONSTANTS ════════════════════╗
 *  (change here only when adapting to a different mechanics/driver)     */
constexpr float MOTOR_FULL_STEPS   = 200.0f;   // 200 for 1.8° motors – 400 for 0.9°
constexpr float MICROSTEPPING      = 16.0f;    // 1/16 is quiet & safe
constexpr float GEAR_RATIO_AZM     = 100.0f;   // worm gear
constexpr float GEAR_RATIO_ALT     =  90.0f;   // belt reduction
constexpr uint16_t RMS_CURRENT_MA  = 500;      // motor RMS current (≈0.7 A)

/* ── Soft travel limits (degrees).  Now compliant with TPPA ±15° ─────── */
constexpr float LIMIT_AZM_MIN_DEG  = -15.0f;   // 
constexpr float LIMIT_AZM_MAX_DEG  =  15.0f;   // 
constexpr float LIMIT_ALT_MIN_DEG  = -15.0f;   // 
constexpr float LIMIT_ALT_MAX_DEG  =  15.0f;   // 

/* ── Uncomment to send a <Run|...| frame every STATUS_INTERVAL_MS ────── */
// #define STATUS_INTERVAL_MS 250

/* ── Compile-time conversion helpers (steps ⇄ degrees) ───────────────── */
constexpr float STEPS_PER_DEG_AZM =
        (MOTOR_FULL_STEPS * MICROSTEPPING * GEAR_RATIO_AZM) / 360.0f;
constexpr float STEPS_PER_DEG_ALT =
        (MOTOR_FULL_STEPS * MICROSTEPPING * GEAR_RATIO_ALT) / 360.0f;

/* ╔══════════════════════════ PIN DEFINITIONS ══════════════════════════╗ */
constexpr uint8_t PIN_EN      = 25;
constexpr uint8_t PIN_DIR_AZM = 26;
constexpr uint8_t PIN_STEP_AZM= 27;
constexpr uint8_t PIN_DIR_ALT = 32;
constexpr uint8_t PIN_STEP_ALT= 33;
constexpr int8_t  PIN_RX_AZM  = 39;
constexpr int8_t  PIN_RX_ALT  = 36;

constexpr uint8_t DRIVER_ADDR = 0;
constexpr float    R_SENSE    = 0.11f;

/* ╔══════════════════════════ GLOBAL STATE ═════════════════════════════╗ */
float posDegAZM = 0.0f;
float posDegALT = 0.0f;
volatile bool feedHold = false;

/* ╔══════════════════════════ DRIVER INSTANCES ═════════════════════════╗ */
HardwareSerial uartAzm(2);
HardwareSerial uartAlt(1);
TMC2209Stepper drvAzm(&uartAzm, R_SENSE, DRIVER_ADDR);
TMC2209Stepper drvAlt(&uartAlt, R_SENSE, DRIVER_ADDR);

/* ───────────────────────────────────────────────────────────────────── */
inline void sendStatus(const char* state)
{
    Serial.print('<');  Serial.print(state);
    Serial.print("|MPos:");  Serial.print(posDegAZM, 3);
    Serial.print(',');        Serial.print(posDegALT, 3);
    Serial.println(",0|");
}

void stepAxis(uint8_t pinStep, uint8_t pinDir,
              float deltaDeg, float stepsPerDeg, float& posVar)
{
    long totalSteps = lroundf(deltaDeg * stepsPerDeg);
    if (!totalSteps) return;

    digitalWrite(pinDir, totalSteps > 0 ? HIGH : LOW);
    totalSteps = labs(totalSteps);

#ifdef STATUS_INTERVAL_MS
    uint32_t tLast = millis();
#endif

    for (long s = 0; s < totalSteps; ++s)
    {
        if (feedHold) break;
        digitalWrite(pinStep, HIGH);  delayMicroseconds(600);
        digitalWrite(pinStep, LOW);   delayMicroseconds(600);
#ifdef STATUS_INTERVAL_MS
        if (millis() - tLast >= STATUS_INTERVAL_MS) {
            sendStatus("Run");  tLast = millis();
        }
#endif
    }
    posVar += deltaDeg;
}

/* ══════════════════════════════════════════════════════════════════════ */
void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(10);
    delay(150);
    while (Serial.available()) Serial.read();  // clear boot garbage

    pinMode(PIN_EN, OUTPUT);        digitalWrite(PIN_EN, LOW);
    pinMode(PIN_DIR_AZM, OUTPUT);   pinMode(PIN_STEP_AZM, OUTPUT);
    pinMode(PIN_DIR_ALT, OUTPUT);   pinMode(PIN_STEP_ALT, OUTPUT);

    uartAzm.begin(115200, SERIAL_8N1, PIN_RX_AZM, -1);
    uartAlt.begin(115200, SERIAL_8N1, PIN_RX_ALT, -1);

    auto initDrv = [](TMC2209Stepper& d){
        d.begin(); d.pdn_disable(true); d.rms_current(RMS_CURRENT_MA);
        d.microsteps(MICROSTEPPING); d.en_spreadCycle(false);
    };
    initDrv(drvAzm);  initDrv(drvAlt);

    sendStatus("Idle");
}

/* ══════════════════════════════════════════════════════════════════════ */
void loop()
{
    if (Serial.available() && Serial.peek() == '?') {
        Serial.read();  sendStatus(feedHold ? "Hold" : "Idle"); Serial.println(); return;
    }

    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n'); line.trim(); if (line.isEmpty()) return;

    if (line == "!"){ feedHold = true;  Serial.println("ok"); return; }
    if (line == "~"){ feedHold = false; Serial.println("ok"); return; }
    if (line == "$X"){ Serial.println("ok"); return; }

    if (line.startsWith("$J=") || line.startsWith("J="))
    {
        line = line.substring(line.indexOf('=')+1);
        bool rel=false;
        if      (line.startsWith("G91")){ rel=true;  line.remove(0,3);}
        else if (line.startsWith("G53")){ rel=false; line.remove(0,3);}
        else { Serial.println("error:Expected G91/G53"); return; }

        line.replace("G21",""); line.trim();
        int fPos=line.indexOf('F'); if (fPos<2){Serial.println("error:Bad $J");return;}

        char axis=line.charAt(0); float val=line.substring(1,fPos).toFloat();
        Serial.println("ok"); sendStatus("Run");

        if (axis=='X'){
            float tgt=rel?posDegAZM+val:val;
            if (tgt<LIMIT_AZM_MIN_DEG||tgt>LIMIT_AZM_MAX_DEG) Serial.println("error:AZM limit");
            else stepAxis(PIN_STEP_AZM,PIN_DIR_AZM,tgt-posDegAZM,STEPS_PER_DEG_AZM,posDegAZM);
        }else if(axis=='Y'){
            float tgt=rel?posDegALT+val:val;
            if (tgt<LIMIT_ALT_MIN_DEG||tgt>LIMIT_ALT_MAX_DEG) Serial.println("error:ALT limit");
            else stepAxis(PIN_STEP_ALT,PIN_DIR_ALT,tgt-posDegALT,STEPS_PER_DEG_ALT,posDegALT);
        }else Serial.println("error:Axis");

        sendStatus(feedHold?"Hold":"Idle");
        return;
    }

    /* --- small console helpers (unchanged) --------------------------- */
    if (line.startsWith("AZM:")){ /* … idem … */ }
    if (line.startsWith("ALT:")){ /* … idem … */ }
    if (line=="RST"){ posDegAZM=posDegALT=0; Serial.println("OK"); sendStatus("Idle"); return;}
    if (line=="HOME"){ /* … idem … */ return;}
    if (line=="POS?"||line=="STA?"){ /* … idem … */ return;}

    Serial.println("ERR:Unknown command");
}

/* TODO / roadmap unchanged */
