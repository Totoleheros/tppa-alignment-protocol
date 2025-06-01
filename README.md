# Serial Alt-Az Polar Alignment Controller

This project defines a **simple serial communication protocol** for controlling a two-axis (ALT/AZM) motorized system for polar alignment corrections. It is inspired by Avalon’s Polar Alignment System and designed to be compatible with tools like the TPPA plugin in NINA.

---

## 📦 Features

- UART control of **two TMC2209 stepper drivers** (Azimuth & Altitude)
- Simple **serial command interface** (`AZM:+1.2`, `ALT:-0.5`, etc.)
- **Position tracking** and software **movement limits**
- `RST` to reset position, `POS?` to query state
- Fully customizable motor specs and gear ratios

---

## 🛠 Hardware Requirements

| Component       | Notes                                   |
|----------------|------------------------------------------|
| FYSETC E4 V1.0  | ESP32-based controller with 4x TMC2209  |
| 2x stepper motors | 1.8° recommended (e.g. 17HS19-2004S1) |
| Power supply    | 12V (recommended for silent operation)  |
| UART wiring     | Z-MIN → PDN (MOTX), Y-MIN → PDN (MOTY)  |
| Optional        | 1–10 µF capacitor between RST and GND (for reliable flashing) |

---

## ⚙️ Arduino IDE Setup

1. **Install ESP32 support**:
   - Open Arduino IDE
   - File → Preferences → Additional Boards Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Tools → Board → Board Manager → Install "esp32" (version 2.0.17 recommended)

2. **Install Library**:
   - Sketch → Include Library → Manage Libraries
   - Search for `TMCStepper`, install latest version

3. **Board Configuration**:

| Setting               | Value                                      |
|-----------------------|--------------------------------------------|
| Board                 | ESP32 Dev Module                           |
| CPU Frequency         | 240MHz (WiFi/BT)                           |
| Flash Frequency       | 40MHz                                      |
| Flash Mode            | DIO                                        |
| Flash Size            | 4MB (32Mb)                                 |
| Partition Scheme      | Huge APP (3MB No OTA / 1MB SPIFFS)         |
| PSRAM                 | Enabled                                    |
| Upload Speed          | 115200                                     |
| Port                  | `/dev/cu.wchusbserial10` (may vary)        |

---

## 🔌 Motor Wiring (default)

| Function | Motor | Pin (ESP32) | Label on E4 |
|----------|-------|-------------|-------------|
| STEP     | AZM   | GPIO27      | MOTX        |
| DIR      | AZM   | GPIO26      | MOTX        |
| EN       | Both  | GPIO25      | shared      |
| UART RX  | AZM   | GPIO39      | Z-MIN       |
| STEP     | ALT   | GPIO33      | MOTY        |
| DIR      | ALT   | GPIO32      | MOTY        |
| UART RX  | ALT   | GPIO36      | Y-MIN       |

---

## 🧪 Serial Commands

| Command         | Description                                      |
|----------------|--------------------------------------------------|
| `AZM:+1.25`     | Move azimuth motor +1.25°                        |
| `AZM:-0.50`     | Move azimuth motor -0.50°                        |
| `ALT:+0.80`     | Move altitude motor +0.80°                       |
| `ALT:-1.20`     | Move altitude motor -1.20°                       |
| `RST`           | Reset both positions to zero                     |
| `POS?` / `STA?` | Return current position of both axes             |

### ✅ Firmware Replies

| Response         | Meaning                                          |
|------------------|--------------------------------------------------|
| `OK`             | Command completed                               |
| `BUSY`           | Motor is moving                                 |
| `ERR:AZM out of bounds` | Exceeded ±10° azimuth limit         |
| `ERR:ALT out of bounds` | Exceeded ±15° altitude limit         |
| `ERR:Unknown command`   | Invalid or unrecognized command       |

---

## 🧠 Customization

Inside `PolarAlign.ino`, you can modify:

```cpp
const float MOTOR_STEPS_PER_REV = 200.0;  // usually 200 for 1.8°
const float MICROSTEPPING = 16.0;
const float GEAR_RATIO_AZM = 100.0;
const float GEAR_RATIO_ALT = 90.0;
```

Software limits:

```cpp
const float LIMIT_MIN_AZM = -10.0;
const float LIMIT_MAX_AZM =  10.0;
const float LIMIT_MIN_ALT = -15.0;
const float LIMIT_MAX_ALT =  15.0;
```

---

## 🤝 Compatibility

- ✅ NINA Plugin: TPPA (Three-Point Polar Alignment)
- ✅ Compatible with direct serial control via USB or TCP bridge

---

## 🧾 License

MIT — do whatever you want, improve and share back!

---

## 🛰️ Acknowledgments

Built by [Antonino Nicoletti] with guidance from [Stefan Berg](https://discord.gg/nina) (TPPA/NINA) and based on FYSETC 

## 🤝 Contributions Welcome

This repository is **open source and community-driven**. Feedback, improvements, and implementations in various ecosystems are more than welcome.

---

## ✉️ Contact

For questions or suggestions, feel free to reach me via GitHub issues or email: antonino.antispam@free.fr
