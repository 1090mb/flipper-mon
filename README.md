# Flipper-Mon 🦖

[![Platform: Flipper Zero](https://img.shields.io/badge/Platform-Flipper%20Zero-orange.svg)](https://flipperzero.one/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**Flipper-Mon** is a persistent digital pet game for the Flipper Zero. It features hardware-linked naming, SD card save states, and interactive care mechanics.

---

## ✨ Features
- **Persistent Progress:** Stats (Level, Health, Happiness) are saved to `/ext/apps_data/flippermon/save.dat`.
- **Unique Hardware Identity:** Your pet's name is procedurally generated based on your Flipper's unique ID.
- **Audio Feedback:** Piezo-speaker chirps for feeding and fanfares for leveling up.
- **Evolution:** Level 5+ transforms your Yeti into its larger "Abominable" form.
- **NFC Scavenging:** Level up by scanning real-world NFC tags and cards.

---

## 🎮 Controls

| Action | Input (Main Menu) | Input (Nursery) |
| :--- | :--- | :--- |
| **Feed (+HP)** | — | Left |
| **Play (+HAP)** | — | Right |
| **Navigation** | Up/Down | Back |
| **Select** | OK | — |

---

## 🛠️ Build & Installation

Ensure you have [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) installed.

1. Connect Flipper via USB.
2. Run `ufbt launch` from the project root.

---

## 📜 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
