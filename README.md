# ESP32 Industrial HVAC Control System
**University Assignment: Embedded Systems & Real-Time Operating Systems**

## 📌 Project Overview
This project implements a safety-critical HVAC (Heating, Ventilation, and Air Conditioning) control system using an **ESP32** and **FreeRTOS**. The system monitors environmental conditions and manages a cooling relay based on a user-defined target temperature, while maintaining industrial-grade safety fail-safes.

## 🛠️ Features
*   **Multithreaded Architecture:** Uses FreeRTOS tasks to handle sensors, user input (ADC), and system logic concurrently.
*   **Thread Safety:** Implements **Semaphores (Mutexes)** to prevent data corruption when accessing shared system states.
*   **Safety Fail-Safes:**
    *   **Watchdog Timer (WDT):** Automatically reboots the system if the main control loop hangs.
    *   **Emergency Stop:** Hardware interrupt-driven button to immediately shut down operations.
    *   **Anomaly Detection:** Validates sensor data for "impossible" values or hardware failure.
    *   **Overheat Protection:** Automatic shutdown if temperatures exceed 40°C.
*   **Signal Smoothing:** Implements software-based averaging for ADC potentiometer readings to prevent "jitter."

## 🔌 Hardware Configuration
| Component | ESP32 Pin | Function |
| :--- | :--- | :--- |
| **DHT22** | GPIO 15 | Temperature & Humidity Sensing |
| **Potentiometer** | GPIO 34 | Target Temperature Adjustment |
| **Relay** | GPIO 13 | AC Compressor / Fan Control |
| **Buzzer** | GPIO 12 | Alarm / Fail-safe Indicator |
| **LED** | GPIO 14 | System Status Indicator |
| **Button** | GPIO 27 | Emergency Override (Interrupt) |

## ⚙️ Logic & Calibration
The target temperature is set via a potentiometer using the following linear calibration:
$$TargetTemp = 15.0 + (ADC_{value} \times \frac{20.0}{4095.0})$$
This allows a setpoint range between **15°C and 35°C**.

## 🚀 How to Run
1.  **Library Dependencies:** Ensure you have the `DHT sensor library` by Adafruit installed in your Arduino IDE or VS Code PlatformIO.
2.  **Hardware Setup:** Wire the components according to the Pin Definitions table above.
3.  **Flash:** Upload `sketch.ino` to your ESP32.
4.  **Monitor:** Open the Serial Monitor at **115200 baud** to view real-time task telemetry and system status.

---
*Developed as part of a University Assignment for Embedded Systems.*
