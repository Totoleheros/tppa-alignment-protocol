# Serial Alt-Az Polar Alignment Controller

This project defines a **simple serial communication protocol** for controlling a two-axis (ALT/AZM) motorized system for polar alignment corrections. It is inspired by Avalon’s Polar Alignment System and designed to be compatible with tools like the TPPA plugin in NINA.

## 👨‍🔧 Project Context

I'm an amateur maker, not a professional developer. This initiative emerged from a passion project involving **motorized adjustment of polar alignment** on an equatorial mount. I'm using an **OnStepX-based controller**, but I want this protocol to remain **platform-agnostic and open** so that anyone can adapt it to their own setup.

This effort is being developed in collaboration with the TPPA plugin author, who is considering integrating this kind of control directly into NINA — provided we can settle on a clear, standard protocol.

## 🧠 Protocol Overview

The communication occurs over a **serial port** at 115200 baud (8N1). Each command sent to the controller must be terminated with `\n`.

Commands are **text-based** and return a string ending with `#`.

---

## 📡 Supported Commands

### 1. `MOVE AZM:<delta> ALT:<delta>`
Moves the mount in azimuth and altitude by the specified deltas (in degrees).

**Example:**
```txt
MOVE AZM:+0.25 ALT:-0.12
→ OK#
```

---

### 2. `GETPOS`
Returns the current ALT/AZM position of the alignment system.

**Example:**
```txt
GETPOS
→ AZM:+0.25 ALT:-0.12#
```

---

### 3. `STOP`
Immediately halts any ongoing movement.

**Example:**
```txt
STOP
→ OK#
```

---

### 4. `PING`
Basic ping to test communication.

**Example:**
```txt
PING
→ PONG#
```

---

### 5. `HELP`
Returns the list of available commands.

**Example:**
```txt
HELP
→ MOVE, GETPOS, STOP, PING, HELP#
```

---

## ✍️ Serial Output Format

All responses must end with a `#` character.

- OK → `OK#`
- Errors (optional extension): `ERR:message#`
- Position: `AZM:<float> ALT:<float>#`

---

## 🔁 Example Communication

```txt
→ MOVE AZM:+0.15 ALT:-0.07
← OK#

→ GETPOS
← AZM:+0.15 ALT:-0.07#
```

---

## 🛠️ Implementation Notes

The physical system includes:
- Two stepper motors (ALT and AZM)
- Motor drivers
- A controller capable of reading serial commands and applying microstep-accurate movements

Precision target: adjustments < 30 arcseconds (~0.0083°)

---

## 🌐 Future Considerations

- Dynamic port detection
- Query for mechanical limits
- Support for homing / reset commands
- Configurable step-per-degree values

---

## 🤝 Contributions Welcome

This repository is **open source and community-driven**. Feedback, improvements, and implementations in various ecosystems are more than welcome.

---

## ✉️ Contact

For questions or suggestions, feel free to reach me via GitHub issues or email: antonino.antispam@free.fr
