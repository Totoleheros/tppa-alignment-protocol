
# Serial Alt-Az Polar Alignment Controller

This project defines a **simple serial communication protocol** for controlling a two-axis (ALT/AZM) motorized system for polar alignment corrections. It is inspired by Avalon’s Polar Alignment System and designed to be compatible with tools like the TPPA plugin in NINA. It is also inspired by the OnStep project since we use here the Fysetc E4 V1.0 motherboard to connect the two stepper motors. A specific Arduino code must be injected in the E4 card. 

---

## 📦 Features

- UART control of **two TMC2209 stepper drivers** (Azimuth & Altitude)
- Simple **serial command interface** (historical: `AZM:+1.2`, `ALT:-0.5`, etc.)
- **Position tracking** and software **movement limits**
- `RST` to reset position, `POS?` or `STA?` to query state
- `HOME` to return to origin (0,0) position
- Optional **G-code inspired commands** (`J=G91X+1.0F300`)
- Fully customizable motor specs and gear ratios

---

## 📽️ Demo

A video demonstration of the first working prototype can be seen here: https://d.pr/v/Lk6GNp.

---

## 🔩 Hardware Setup

Refer to [HARDWARE.md](./HARDWARE.md) for the complete hardware assembly guide and wiring instructions.

You can also view photos of the physical setup in [`IMAGES/ASSEMBLY`](./IMAGES/ASSEMBLY).

---

## 🛠 Hardware Requirements

| Component         | Notes                                                 |
|------------------|--------------------------------------------------------|
| FYSETC E4 V1.0    | ESP32-based controller with 4x TMC2209                |
| 2x stepper motors | 1.8° recommended (e.g. 17HS19-2004S1)                 |
| Power supply      | 12V (recommended for silent operation)               |
| UART wiring       | Z-MIN → PDN (MOTX), Y-MIN → PDN (MOTY)               |
| Optional          | 1–10 µF capacitor between RST and GND (ESP32 stability) |

---

## ⚙️ Arduino IDE Setup

1. **Install ESP32 support**:
   - Arduino IDE → Preferences → Additional Boards Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Then install "esp32" via the Boards Manager (v2.0.17 recommended)

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
| Port                  | `/dev/cu.wchusbserialXXXX`                 |

---

## 🔌 Motor Wiring (default)

| Function | Motor | ESP32 GPIO | Label on E4 |
|----------|-------|-------------|-------------|
| STEP     | AZM   | GPIO27      | MOTX        |
| DIR      | AZM   | GPIO26      | MOTX        |
| EN       | Both  | GPIO25      | Shared      |
| UART RX  | AZM   | GPIO39      | Z-MIN       |
| STEP     | ALT   | GPIO33      | MOTY        |
| DIR      | ALT   | GPIO32      | MOTY        |
| UART RX  | ALT   | GPIO36      | Y-MIN       |

---

## 🧪 Serial Commands

### ✅ Historical (default) format

| Command         | Description                                      |
|----------------|--------------------------------------------------|
| `AZM:+1.25`     | Move azimuth motor +1.25°                        |
| `AZM:-0.50`     | Move azimuth motor -0.50°                        |
| `ALT:+0.80`     | Move altitude motor +0.80°                       |
| `ALT:-1.20`     | Move altitude motor -1.20°                       |
| `RST`           | Reset internal positions of both axes           |
| `HOME`          | Return both motors to position 0,0              |
| `POS?` / `STA?` | Query current position of both axes             |
| `?`             | Alias for position query, G-code style          |

> 🧾 When sending `POS?`, `STA?`, or `?`, the reply format is:
>
> `<Idle|MPos:+1.234,-0.567,0|`
>
> *(followed by an empty line, as required by TPPA)*

---

### 🧪 Experimental (G-code inspired) - Should work with TPPA

| Command Example              | Description                                      |
|-----------------------------|--------------------------------------------------|
| `J=G53X+1.25F400`            | Move AZM +1.25° at speed 400 (absolute)         |
| `J=G91X-0.50F300`            | Move AZM -0.50° at speed 300 (relative)         |
| `J=G91Y+0.20F200`            | Move ALT +0.20° at speed 200 (relative)         |
| `J=G53Y-0.80F400`            | Move ALT -0.80° at speed 400 (absolute)         |

> ✅ `G53` = absolute mode, `G91` = relative mode  
> ✅ `X` targets AZM, `Y` targets ALT  
> ✅ `Fxxx` is the speed, required as an integer

---

## 🧠 Customization

Edit `PolarAlignV2.ino` to reflect your setup:

```cpp
const float MOTOR_STEPS_PER_REV = 200.0;
const float MICROSTEPPING = 16.0;
const float GEAR_RATIO_AZM = 100.0;
const float GEAR_RATIO_ALT = 90.0;
```

```cpp
const float LIMIT_MIN_AZM = -10.0;
const float LIMIT_MAX_AZM =  10.0;
const float LIMIT_MIN_ALT = -15.0;
const float LIMIT_MAX_ALT =  15.0;
```

---

## 🧾 License

MIT — use, modify, and share freely.

---

## 🛰️ Acknowledgments

Thanks to [Stefan Berg](https://discord.gg/nina), the OnStep community, and all testers.  
Project built by **Antonino Nicoletti**.
