/*****************************************************************************************
 *  POLAR-ALIGNMENT MOTOR CONTROLLER – FYSETC-E4 (ESP32 + TMC2209)
 *  ---------------------------------------------------------------------------------------
 *  Author      : Antonino Nicoletti  +  ChatGPT
 *  File        : polar-align-controller.ino
 *  License     : MIT
 *  Last update : 2025-06-05
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

/* ╔═════════════════════════ USER-TUNABLE CONSTANTS ══════════════════════╗
 * These five defines are the *only* things you should ever touch
 * when porting the sketch to a different mount, gear ratio or driver mode.
 * ╚═══════════════════════════════════════════════════════════════════════╝ */
constexpr float MOTOR_FULL_STEPS   = 200.0f; // 200 for 1.8° motors – 400 for 0.9°
constexpr float MICROSTEPPING      = 16.0f;  // ESP32 can handle 1/32, but 1/16 is quiet & safe
constexpr float GEAR_RATIO_AZM     = 100.0f; // → 100:1 worm gear on my mount
constexpr float GEAR_RATIO_ALT     =  90.0f; // →  90:1 belt reduction
constexpr uint16_t RMS_CURRENT_MA  = 500;    // Motor RMS current (≈0.7 A) – tweak to fight wind

/* ── Soft travel limits (degrees).  Keep conservative to avoid mechanical bind ───────── */
constexpr float LIMIT_AZM_MIN_DEG  = -10.0f;
constexpr float LIMIT_AZM_MAX_DEG  =  10.0f;
constexpr float LIMIT_ALT_MIN_DEG  = -15.0f;
constexpr float LIMIT_ALT_MAX_DEG  =  15.0f;

/* ── Uncomment to send a <Run|...| frame every STATUS_INTERVAL_MS while moving ───────── */
// #define STATUS_INTERVAL_MS 250

/* ── Compile-time conversion helpers (steps ⇄ degrees) ───────────────────────────────── */
constexpr float STEPS_PER_DEG_AZM =
        (MOTOR_FULL_STEPS * MICROSTEPPING * GEAR_RATIO_AZM) / 360.0f;
constexpr float STEPS_PER_DEG_ALT =
        (MOTOR_FULL_STEPS * MICROSTEPPING * GEAR_RATIO_ALT) / 360.0f;

/* ╔══════════════════════════ PIN DEFINITIONS ═══════════════════════════╗ */
constexpr uint8_t PIN_EN      = 25;   // shared Enable (FYSETC ties both drivers)
constexpr uint8_t PIN_DIR_AZM = 26;
constexpr uint8_t PIN_STEP_AZM= 27;
constexpr uint8_t PIN_DIR_ALT = 32;
constexpr uint8_t PIN_STEP_ALT= 33;
constexpr int8_t  PIN_RX_AZM  = 39;   // UART RX – TX is -1 (unused)
constexpr int8_t  PIN_RX_ALT  = 36;

constexpr uint8_t DRIVER_ADDR = 0;    // standalone address (all zeros)
constexpr float    R_SENSE    = 0.11f;

/* ╔══════════════════════════ GLOBAL STATE ══════════════════════════════╗ */
float posDegAZM = 0.0f;               // logical position reported back to TPPA
float posDegALT = 0.0f;
volatile bool feedHold = false;       // set by ‘!’ (GRBL feed-hold)

/* ╔══════════════════════════ DRIVER INSTANCES ══════════════════════════╗ */
HardwareSerial uartAzm(2);
HardwareSerial uartAlt(1);

TMC2209Stepper drvAzm(&uartAzm, R_SENSE, DRIVER_ADDR);
TMC2209Stepper drvAlt(&uartAlt, R_SENSE, DRIVER_ADDR);

/* ╔══════════════════════════ UTILITIES ═════════════════════════════════╗ */

/**
 * Send a minimal GRBL-compatible status frame.
 * Example:  <Idle|MPos:-0.500,2.375,0|
 */
inline void sendStatus(const char* state)
{
    Serial.print('<');
    Serial.print(state);
    Serial.print("|MPos:");
    Serial.print(posDegAZM, 3);
    Serial.print(',');
    Serial.print(posDegALT, 3);
    Serial.println(",0|");
}

/**
 * Blocking step generator.  Feed-hold can interrupt the move at any time
 * (mid-jog support).  Acceleration is **not** implemented – moves are slow.
 */
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
        /* Feed-hold check every µstep – crude but effective at <1 kHz */
        if (feedHold) break;

        digitalWrite(pinStep, HIGH);
        delayMicroseconds(600);
        digitalWrite(pinStep, LOW);
        delayMicroseconds(600);

#ifdef STATUS_INTERVAL_MS
        if (millis() - tLast >= STATUS_INTERVAL_MS) {
            sendStatus("Run");
            tLast = millis();
        }
#endif
    }
    posVar += deltaDeg;   // update logical coordinate even if aborted
}

/* ╔══════════════════════════ SETUP() ═══════════════════════════════════╗ */
void setup()
{
    /* Serial0 = USB CDC.  115 200 bps is plenty for polar moves */
    Serial.begin(115200);
    Serial.setTimeout(10);
    delay(150);                      // give the host time to enumerate

    /* Purge ESP32 boot log so TPPA sees only clean frames */
    while (Serial.available()) Serial.read();

    /* GPIO direction */
    pinMode(PIN_EN,        OUTPUT);
    pinMode(PIN_DIR_AZM,   OUTPUT);
    pinMode(PIN_STEP_AZM,  OUTPUT);
    pinMode(PIN_DIR_ALT,   OUTPUT);
    pinMode(PIN_STEP_ALT,  OUTPUT);
    digitalWrite(PIN_EN, LOW);       // Enable drivers (FYSETC logic low)

    /* Bring up UARTs dedicated to each TMC2209 (receive-only) */
    uartAzm.begin(115200, SERIAL_8N1, PIN_RX_AZM, -1);
    uartAlt.begin(115200, SERIAL_8N1, PIN_RX_ALT, -1);

    /* Driver configuration – identical for both axes */
    auto initDriver = [](TMC2209Stepper& drv){
        drv.begin();
        drv.pdn_disable(true);       // PDN pin as UART
        drv.rms_current(RMS_CURRENT_MA);
        drv.microsteps(MICROSTEPPING);
        drv.en_spreadCycle(false);   // StealthChop
    };
    initDriver(drvAzm);
    initDriver(drvAlt);

    sendStatus("Idle");              // first thing TPPA will look for
}

/* ╔══════════════════════════ MAIN LOOP() ═══════════════════════════════╗ */
void loop()
{
    /* ─────────────────── Poll handler (‘?’) ──────────────────────────── */
    if (Serial.available() && Serial.peek() == '?')
    {
        Serial.read();               // consume the question mark
        sendStatus(feedHold ? "Hold" : "Idle");
        Serial.println();            // GRBL uses CR/LF – LF alone is accepted
        return;
    }

    /* ─────────────────── Command reception ───────────────────────────── */
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) return;

    /* ─────────────────── Feed-hold (!) / Resume (~) ──────────────────── */
    if (line == "!") {               // GRBL ‘feed hold’
        feedHold = true;
        Serial.println("ok");        // acknowledge quickly
        return;
    }
    if (line == "~") {               // GRBL ‘cycle start’
        feedHold = false;
        Serial.println("ok");
        return;
    }

    /* ─────────────────── Unlock ($X) ─────────────────────────────────── */
    if (line == "$X")
    {
        Serial.println("ok");        // we never lock anyway
        return;
    }

    /* ─────────────────── Jog ($J=… / J=…) ────────────────────────────── */
    if (line.startsWith("$J=") || line.startsWith("J="))
    {
        /* Strip prefix and optional whitespace */
        line = line.substring(line.indexOf('=') + 1);

        /* Mode token: G91 (relative) or G53 (absolute) */
        bool relative = false;
        if      (line.startsWith("G91")) { relative = true;  line.remove(0,3); }
        else if (line.startsWith("G53")) { relative = false; line.remove(0,3); }
        else    { Serial.println("error:Expected G91/G53"); return; }

        /* TPPA always appends G21 (mm mode) – not relevant for degrees */
        line.replace("G21", "");
        line.trim();

        int fPos = line.indexOf('F');
        if (fPos < 2) { Serial.println("error:Bad $J format"); return; }

        char axis = line.charAt(0);
        float value = line.substring(1, fPos).toFloat();
        // NOTE: feedrate (F) unused – constant delayMicroseconds above

        Serial.println("ok");        // GRBL spec: ACK first, move later
        sendStatus("Run");

        if (axis == 'X')
        {
            float target = relative ? posDegAZM + value : value;
            if (target < LIMIT_AZM_MIN_DEG || target > LIMIT_AZM_MAX_DEG)
                Serial.println("error:AZM limit");
            else
                stepAxis(PIN_STEP_AZM, PIN_DIR_AZM,
                         target - posDegAZM, STEPS_PER_DEG_AZM, posDegAZM);
        }
        else if (axis == 'Y')
        {
            float target = relative ? posDegALT + value : value;
            if (target < LIMIT_ALT_MIN_DEG || target > LIMIT_ALT_MAX_DEG)
                Serial.println("error:ALT limit");
            else
                stepAxis(PIN_STEP_ALT, PIN_DIR_ALT,
                         target - posDegALT, STEPS_PER_DEG_ALT, posDegALT);
        }
        else Serial.println("error:Axis");

        sendStatus(feedHold ? "Hold" : "Idle");
        return;
    }

    /* ─────────────────── Legacy console commands ───────────────────────
     * Not used by TPPA; handy for direct USB-Serial testing.             */

    if (line.startsWith("AZM:"))
    {
        float d = line.substring(4).toFloat();
        if (posDegAZM + d < LIMIT_AZM_MIN_DEG || posDegAZM + d > LIMIT_AZM_MAX_DEG)
            { Serial.println("ERR:AZM limit"); return; }
        Serial.println("BUSY");
        stepAxis(PIN_STEP_AZM, PIN_DIR_AZM, d, STEPS_PER_DEG_AZM, posDegAZM);
        Serial.println("OK");
        sendStatus(feedHold ? "Hold" : "Idle");
        return;
    }

    if (line.startsWith("ALT:"))
    {
        float d = line.substring(4).toFloat();
        if (posDegALT + d < LIMIT_ALT_MIN_DEG || posDegALT + d > LIMIT_ALT_MAX_DEG)
            { Serial.println("ERR:ALT limit"); return; }
        Serial.println("BUSY");
        stepAxis(PIN_STEP_ALT, PIN_DIR_ALT, d, STEPS_PER_DEG_ALT, posDegALT);
        Serial.println("OK");
        sendStatus(feedHold ? "Hold" : "Idle");
        return;
    }

    if (line == "RST")
    {
        posDegAZM = posDegALT = 0.0f;
        Serial.println("OK");
        sendStatus("Idle");
        return;
    }

    if (line == "HOME")
    {
        Serial.println("BUSY");
        stepAxis(PIN_STEP_AZM, PIN_DIR_AZM, -posDegAZM, STEPS_PER_DEG_AZM, posDegAZM);
        stepAxis(PIN_STEP_ALT, PIN_DIR_ALT, -posDegALT, STEPS_PER_DEG_ALT, posDegALT);
        Serial.println("OK");
        sendStatus("Idle");
        return;
    }

    if (line == "POS?" || line == "STA?")
    {
        Serial.print("POS:AZM=");
        Serial.print(posDegAZM, 3);
        Serial.print(",ALT=");
        Serial.println(posDegALT, 3);
        return;
    }

    /* ─────────────────── Fallback ────────────────────────────────────── */
    Serial.println("ERR:Unknown command");
}

/* ╔══════════════════════════ TODO / ROADMAP ════════════════════════════╗
 * • Replace the fixed microsecond delay with an acceleration ramp
 *   (see TMCStepper’s ‘StallGuard’ + constant acceleration planner).
 * • Non-blocking motion queue so feed-hold can *pause* instead of
 *   *aborting* the jog.
 * • Implement power-down (`M18`) and current reduction when idle.
 * • CRC verification for UART replies from the TMC2209 (robustness).
 * • Upgrade to ESP32-S3 and use native USB CDC for faster enumeration.
 * ╚══════════════════════════════════════════════════════════════════════╝ */
