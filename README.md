# 📡 Real-Time Signal Processing with MQTT and LoRa on Heltec ESP32

An ESP32-based project that simulates signal data, analyzes it with FFT, computes a moving average, and sends the data via MQTT (and optionally LoRa). Built using FreeRTOS and Heltec ESP32 WiFi LoRa v3.

Developed by **J. Edgar Hernandez**  
For: *EMAI - Sapienza University of Rome* (IoT Assignment 1)

---

## 🚀 Features

- 🎛️ Real-time sine wave signal simulation
- ⚡ Adaptive sampling using FFT peak detection
- 📊 Signal aggregation with time window averaging
- ☁️ MQTT integration for publishing data to a broker
- 📡 LoRa (SX1262) initialization for future expansion
- 🧵 Thread-safe serial output using semaphores
- 🖥️ OLED display initialization for status display

---

## 🧰 Hardware Required

- [Heltec WiFi LoRa 32 V3](https://heltec.org/project/wifi-lora-32-v3/)
  - ESP32-S3 chip
  - SX1262 LoRa radio module
  - 0.96” OLED screen

---

## 📦 Library Dependencies

Install the following libraries using the Arduino Library Manager:

- `Heltec ESP32 Dev-Boards`
- `arduinoFFT`
- `WiFi`
- `PubSubClient`

Make sure you're using the **Heltec ESP32 Board Package** (version 3.0.2 or later) from the Board Manager.

---

## 📶 MQTT Setup

In the code, update your Wi-Fi and MQTT credentials:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "192.168.x.x";
const int mqtt_port = 1883;
const char* mqtt_topic = "iot/aggregate";
```

Then, to listen to messages on your PC:

```bash
mosquitto_sub -h 192.168.x.x -t iot/aggregate
```

---

## ⚙️ LoRa Configuration

This project initializes the LoRa radio using:

```cpp
Heltec.begin(false /*Display*/, true /*LoRa*/, true /*Serial*/, true /*PABOOST*/, REGION_EU868);
```

> Note: LoRa is initialized but **not yet transmitting**. You can extend this by adding LoRaWAN or P2P logic using libraries like `LoRaWAN_ESP32`.

---

## 🧵 FreeRTOS Task Breakdown

| Task Name    | Description                                           |
|-------------|--------------------------------------------------------|
| SensorTask   | Simulates analog sine signals                         |
| FFTTask      | Applies FFT to sampled data and adjusts sample rate   |
| AggregateTask| Calculates moving average over a 5s window            |
| MQTTTask     | Publishes the average to the MQTT broker              |

Each task runs on its own core/thread, leveraging the ESP32’s FreeRTOS scheduler.

---

## 🖥️ OLED Display Output

When the device boots, the OLED screen shows:

```
J. Edgar Hernandez
Assignment 1
```

You can customize this in `initialDisplaySetup()`.

---

## 🪵 Serial Output Format

Example terminal output:
```
#SAMPLE:       2.4132
#SAMPLING_FREQ: 23.1231
#AGGREGATE:    1.1423
[MQTT] Published: {"average": 1.1423}
```

---

## 📁 File Structure

```
project/
│
├── main.ino            # Main FreeRTOS + signal logic
└── README.md           # This file
```

---

## 📜 License

MIT License — free for personal, academic, or commercial use.

---

## 👨‍💻 Author

**J. Edgar Hernandez**  
IoT Assignment 1 — EMAI, Sapienza University