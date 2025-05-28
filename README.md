# TPPA-Compatible Protocol for Motorized Polar Alignment System

## 📡 Overview

This repository documents a minimal, serial-based protocol for controlling a motorized Alt-Az platform for polar alignment, compatible with the TPPA plugin in N.I.N.A.

## 📘 Description

The protocol allows simple serial communication to:

- Apply correction movements in Altitude and Azimuth.
- Report current correction status.
- Be fully open and adaptable for DIY platforms.

## 📐 Commands

| Command         | Description                                      |
|----------------|--------------------------------------------------|
| `MOVE:AZM=+0.12` | Move AZM by +0.12° (East = +, West = -)         |
| `MOVE:ALT=-0.07` | Move ALT by -0.07° (Up = +, Down = -)           |
| `POS?`          | Query current corrected position                 |
| `STOP`          | Stop all motion immediately                      |
| `RESET`         | Reset internal position to zero                  |

## ✍️ Author

Antonino — passionate maker, not a professional developer 😉  
With assistance from ChatGPT.

## 📅 Status

Draft — open to feedback and collaboration!
