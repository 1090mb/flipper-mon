# Flipper-Mon 🦖

[![Platform: Flipper Zero](https://img.shields.io/badge/Platform-Flipper%20Zero-orange.svg)](https://flipperzero.one/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Status: v1.2 Stable](https://img.shields.io/badge/Status-v1.2%20Stable-blue.svg)](https://github.com/yourusername/flipper-mon)

**Flipper-Mon** is a persistent digital pet game for the Flipper Zero. It combines classic virtual pet care with real-world hardware exploration. Every Flipper Zero gets a unique pet linked to its hardware identity!

---

## 🕹️ Game Mechanics

### **The Nursery**
Monitor and care for your pet in its living quarters.
* **Care Actions:** Feed your pet to restore **HP** or play with it to increase **HAP** (Happiness).
* **Evolution:** Once your pet reaches **Level 5**, it evolves from a small Yeti into its larger "Abominable" form.
* **Hardware Identity:** Your pet’s name is procedurally generated using your Flipper's unique hardware name (e.g., `Mylo-mon`).

### **NFC Scavenging (The Mystery Box)**
Your pet "consumes" the data from real-world NFC tags. Scanning different tags provides randomized rewards based on the tag's unique UID:
* 🍬 **Rare Candy:** Massive Level boost (10% chance).
* 🍗 **Feast:** Instantly refills Health to 100%.
* 🧸 **Super Toy:** Instantly refills Happiness to 100%.
* ✨ **Standard XP:** Common level progression.



---

## 🎮 Controls

| Action | Input (Main Menu) | Input (Nursery) |
| :--- | :--- | :--- |
| **Feed (+HP)** | — | Left |
| **Play (+HAP)** | — | Right |
| **Select** | OK | — |
| **Back/Save** | Back | Back |

---

## 🛠️ Technical Features
* **Persistent Storage:** All stats are saved to `/ext/apps_data/flippermon/save.dat`. Data is automatically saved upon leveling up or exiting the Nursery.
* **Thread-Safe Rendering:** Built using the `ViewModel` architecture to prevent screen flickering or null-pointer crashes.
* **Audio Engine:** Utilizes the Flipper's piezo speaker for interactive beeps and level-up fanfares.

---

## 📂 Installation

Ensure you have [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) installed.

1. Clone the repository to your computer.
2. Connect your Flipper Zero via USB.
3. Run:
   ```bash
   ufbt launch