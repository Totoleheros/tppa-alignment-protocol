# Serial Alt‑Az Polar Alignment Controller

Minimal **GRBL‑style** firmware + hardware recipe for driving a two‑axis (Azimuth & Altitude) mount during polar‑alignment routines such 
as **TPPA** in **N.I.N.A.**  
It runs on the *FYSETC E4 V1.0* (ESP32 + dual TMC2209) and pretends to be a micro‑controller that understands `$J=…` jog commands and `<Idle|MPos:…|` status frames.

> **TL;DR** – Flash the sketch, wire the motors, set N.I.N.A. to talk to a **generic GRBL device**, **leave the TPPA “Gear Ratio” field at `1.0`**, and the routine will move your mount by up to **± 15 °**.

---

## 📦 Key Features

| Area            | What you get                                                                   |
|-----------------|--------------------------------------------------------------------------------|
| **Driver layer**| UART control of two **TMC2209** drivers, µstepping + stealthChop               |
| **Protocol**    | ✔ Immediate `ok` on `$J=`<br>✔ GRBL status frames (`<Idle|…|` / `<Run|…|`)<br>✔ Feed‑Hold `!` / Cycle‑Start `~` stubs |
| **Legacy CLI**  | Still accepts `AZM:+2.5`, `ALT:-1.0`, `HOME`, `RST`, etc.                      |
| **Safety**      | Software limits (now **± 15 ° azimuth, ± 15 ° altitude**), soft‑home, optional status heartbeat every 250 ms |
| **Customisable**| One place to change motor steps, gearing, current, travel limits – **all gearing is handled in firmware; keep external gear‑ratio settings at 1.0** |
| **Hardware**    | Single FYSETC E4 board – no extra Arduino, no extra drivers                    |

---

## 🖥️ Demo

First functional prototype: <https://d.pr/v/Lk6GNp>

---

## 🔩 Hardware Overview

See **[`HARDWARE.md`](./HARDWARE.md)** for full assembly photos and wiring diagrams.

| Part | Notes |
|------|-------|
| **FYSETC E4 V1.0** | ESP32‑WROOM‑32, 4 × on‑board TMC2209 – we use two of them (MOT‑X = Azimuth, MOT‑Y = Altitude) |
| **Stepper motors** | 1.8 ° NEMA‑17 recommended (e.g. 17HS19‑2004S1) |
| **Supply**         | 12 V DC (quiet) — 24 V also works if your mechanics can take it |
| **USB cable**      | USB‑C → host PC |
| *(optional)*       | 1–10 µF cap between **RST** & **GND** if flashing is flaky |

### Default GPIO Map (no firmware changes needed)

| Signal   | Axis | ESP32 GPIO | E4 silkscreen |
|----------|------|-----------|---------------|
| STEP     | AZM  | 27        | **MOT‑X** |
| DIR      | AZM  | 26        | **MOT‑X** |
| EN       | Both | 25        | `/ENABLE` |
| UART RX  | AZM  | 39        | Z‑MIN |
| STEP     | ALT  | 33        | **MOT‑Y** |
| DIR      | ALT  | 32        | **MOT‑Y** |
| UART RX  | ALT  | 36        | Y‑MIN |

---

## ⚙️ Arduino IDE Setup

1. **Install ESP32 core**

   ```text
   Preferences → Additional Board URLs:
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

   Boards Manager → *esp32* (≥ v2.0.17).

2. **Install library**

   *TMCStepper* (latest).

3. **Board menu**

   | Option             | Value |
   |--------------------|-------|
   | Board              | **ESP32 Dev Module** |
   | CPU Freq           | 240 MHz (WiFi/BT) |
   | Flash Freq / Mode  | 40 MHz / DIO |
   | Flash Size         | 4 MB |
   | Partition Scheme   | Huge APP (3 MB / 1 MB SPIFFS) |
   | PSRAM              | Enabled |
   | Upload speed       | 115 200 bps (conservative) |
   | Port               | `COMx` / `/dev/tty.usbmodem…` |

Compile ⇒ Upload.  
On first boot the controller prints `<Idle|MPos:0.000,0.000,0|`.

---

## 🧪 Serial Command Reference

### 1️⃣ GRBL‑Style (**used by TPPA**)

| Example                  | Meaning |
|--------------------------|---------|
| `$J=G53X+5.00F400`       | Absolute jog **+5 °** on **Azimuth** |
| `$J=G91G21Y-6.50F300`    | Relative jog **–6.5 °** on **Altitude** |
| `?`                      | Poll → `<Idle|MPos:…|>` |
| `$X`                     | Unlock (always answers `ok`) |
| `!` / `~`                | Feed‑Hold / Resume (simple flag) |

> **Tip :** keep **“Gear Ratio” = 1.0** in the TPPA settings; the firmware already includes all mechanical reductions.

### 2️⃣ Legacy Console (for manual USB testing)

| Command      | Action |
|--------------|--------|
| `AZM:+2.5`   | Jog +2.5 ° (blocking, replies `BUSY` → `OK`) |
| `ALT:-1`     | Jog –1 ° |
| `HOME`       | Soft‑home to 0,0 |
| `RST`        | Zero logical coordinates |
| `POS?` / `STA?` | Report `POS:AZM=…,ALT=…` |

---

## 🛠️ Configuration Knobs

Open **`polar-align-controller.ino`** and tweak:

```cpp
constexpr float MICROSTEPPING      = 16.0f;   // 8, 16, 32…
constexpr float GEAR_RATIO_AZM     = 100.0f;  // mount mechanics
constexpr float GEAR_RATIO_ALT     =  90.0f;
constexpr uint16_t RMS_CURRENT_MA  = 600;     // motor torque vs. noise
```

Soft limits (degrees):

```cpp
constexpr float LIMIT_AZM_MIN_DEG = -15.0f;
constexpr float LIMIT_AZM_MAX_DEG =  15.0f;
constexpr float LIMIT_ALT_MIN_DEG = -15.0f;
constexpr float LIMIT_ALT_MAX_DEG =  15.0f;
```

Uncomment `#define STATUS_INTERVAL_MS 250` to broadcast `<Run|…|` every 250 ms while moving (handy for log viewers).

---

## 🛣 Roadmap

* Trapezoidal acceleration (replace fixed delay loop)  
* Non‑blocking motion queue so **Feed‑Hold** pauses instead of aborting  
* Low‑current sleep (`M18`) when idle  
* CRC check on UART replies for robustness  

Pull requests welcome!

---

## 📄 License


MIT — do whatever you want, just keep the header.

---


## 🙏 Acknowledgements

* **Stefan Berg** – author of the **Three-Point Polar Alignment** plug-in and core N.I.N.A. contributor; his late-night Discord chats made this micro-firmware feasible.  
* **Avalon Instruments** – for the idea of a lean, GRBL-style alignment controller.  
* **OnStep** – ongoing source of inspiration for all things mount control.  
* Maintained by **Antonino Nicoletti** ([antonino.antispam@free.fr]]) – *clear skies!*
