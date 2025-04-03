# Real-Time Signal Processing with MQTT and LoRa on Heltec ESP32

📡 A FreeRTOS-based ESP32 application that simulates a signal, processes it using FFT, calculates a moving average, and sends the result via MQTT and LoRa.

---

## 📌 Features

- Simulated signal generation with configurable frequencies/amplitudes
- Real-time FFT (Fast Fourier Transform) processing
- Adaptive sampling frequency based on peak frequency
- Aggregation of sensor data over time windows
- WiFi and MQTT integration to publish average values
- Support for LoRa (EU868) transmission
- OLED display support (optional)
- Thread-safe logging using semaphores

---

## 🔧 Hardware

- [Heltec WiFi LoRa 32 (V3)](https://heltec.org/project/wifi-lora-32-v3/)
- Onboard:
  - LoRa radio (SX1262)
  - OLED display
- Compatible with:
  - Arduino IDE

---

## 🚀 Getting Started

### 1. Clone the repo
```bash
git clone https://github.com/yourusername/esp32-fft-mqtt-lora.git