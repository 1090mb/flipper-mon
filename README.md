# Flipper-Mon 🦖

[![Platform: Flipper Zero](https://img.shields.io/badge/Platform-Flipper%20Zero-orange.svg)](https://flipperzero.one/)
[![SDK: 2026 Stable](https://img.shields.io/badge/SDK-2026%20Stable-blue.svg)](https://github.com/flipperdevices/flipperzero-firmware)

**Flipper-Mon** is an interactive digital pet and hardware-exploration game for the Flipper Zero. It brings the nostalgia of 90s virtual pets to your favorite hacking tool, utilizing NFC for growth and IR for interaction.

---

## 🕹️ Game Mechanics

### **The Nursery**
The heart of the game. Here you can monitor your pet's vitals and interact with it directly.
* **Vitals:** Track **HP** (Health), **HAP** (Happiness), and **LVL** (Level).
* **Interactions:** Use the D-Pad to Feed or Play with your Yeti.
* **Evolution:** Watch your pet grow! At **Level 5**, your Yeti evolves into a larger, more powerful form.

### **Hardware Integration**
* **NFC Scavenging:** Your pet "eats" data. Scan any NFC tag (Amiibo, hotel key, credit card) to gain experience points.
* **IR Burst:** Use the front-facing IR LED to emit signals during "Battle" mode.

---

## 🎮 Controls

| Action | Input (Main Menu) | Input (Nursery) |
| :--- | :--- | :--- |
| **Move/Select** | Up/Down | — |
| **Confirm** | OK | — |
| **Feed (+HP)** | — | Left |
| **Play (+HAP)** | — | Right |
| **Sleep/Wake** | — | OK |
| **Go Back** | Back | Back |
| **Exit App** | Back (Double-Tap) | — |

---

## 🛠️ Build & Installation

This project uses **uFBT**, the official micro Flipper Build Tool.

### **1. Clone the Repository**
```bash
git clone [https://github.com/yourusername/flipper-mon.git](https://github.com/yourusername/flipper-mon.git)
cd flipper-mon
