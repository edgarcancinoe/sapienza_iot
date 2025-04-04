# 📡 Adaptive Signal Processing & MQTT Communication on Heltec ESP32

This project demonstrates an IoT system running on a **Heltec WiFi LoRa 32 V3** that simulates analog signals, computes FFT to identify dominant frequencies, dynamically adapts the sampling rate, computes aggregate values, and publishes them over MQTT. The goal is to showcase **energy-aware data acquisition and communication** using FreeRTOS.

Developed by **J. Edgar Hernandez**  
📘 *EMAI – Sapienza University of Rome*  
🎓 *IoT Assignment 1*

---

## 🚀 Features

- ✅ Simulates sine wave signals with up to 3 frequency components.
- 🔍 Real-time **FFT** for peak detection and frequency domain analysis.
- ⚡ **Adaptive Sampling**: Modifies sampling rate based on FFT analysis to save energy.
- 📊 Aggregates signal over 5-second windows.
- 📤 Publishes results over MQTT to an edge server.
- 📟 Uses onboard OLED for startup info.
- 🧵 FreeRTOS multi-tasking: Sensor, FFT, Aggregator, MQTT worker.

---

## 🔧 Setup Instructions

### 🛠 Requirements

- Heltec WiFi LoRa 32 V3 board
- PlatformIO / Arduino IDE
- MQTT broker (e.g., Mosquitto)
- WiFi network
- Serial monitor (115200 baud)

### 📦 Libraries Used

- `arduinoFFT`
- `PubSubClient`
- `Wire.h` (I2C)
- `HT_SSD1306Wire` (OLED Display)

---

## 📡 Configuration

### WiFi & MQTT

Edit these lines in the sketch:

```cpp
const char* ssid = "iPhone de Edgar";
const char* password = "01234567";
const char* mqtt_server = "172.20.10.2";  // Your machine’s IP
const int mqtt_port = 1883;
const char* mqtt_topic = "iot/aggregate";
```

To find your IP address, run:

```bash
ifconfig   # (macOS/Linux)
ipconfig   # (Windows)
```

---

## 🧠 Signal Simulation

Signal is randomly generated using:

```
SUM( a_k * sin(2π f_k t) )
```

Where:
- `f_k ∈ [20Hz, 200Hz]`
- `a_k ∈ [10, 25]`
- `N = 2 or 3` components

---

## 📉 Adaptive Sampling Frequency

### 🎯 Objective

Use **Nyquist Theorem** to adapt sampling rate:
> `new_rate = 2.1 * max_frequency_detected`

This helps reduce:
- 🪫 **Energy consumption**
- 🌐 **MQTT bandwidth usage**

Sampling ranges:
- `MIN_SAMPLING_FREQ = 50 Hz`
- `MAX_SAMPLING_FREQ = 1000 Hz`

---

## 📦 Aggregation + MQTT

Every **5 seconds**, the system:
- Averages sampled values
- Publishes via MQTT in JSON format:

```json
{"average": 12.3456}
```

---

## 📊 Energy Evaluation

To compare **adaptive vs constant (oversampled)** behavior:

1. Run the system with adaptive sampling:
   - Collect timestamps and sampling frequency.
   - Estimate number of samples (`samples = freq * time`).
2. Run again with a fixed high sampling rate (e.g., 1000 Hz).
3. Calculate energy savings:

```
saving % = (1 - samples_adaptive / samples_constant) * 100%
```

You can log all `#SAMPLING_FREQ:` messages from the serial output and analyze them in Python or Excel.

---

## 📈 MQTT Monitoring

Use `mosquitto_sub` to view messages:

```bash
mosquitto_sub -h 172.20.10.2 -t "iot/aggregate"
```

---

## 📁 Folder Structure

```bash
├── src/
│   └── main.cpp
├── README.md
└── platformio.ini
```

---

## ✅ Future Improvements

- LoRaWAN + TTN cloud integration
- Energy metering using INA219
- CSV export for signal logs
- OTA updates

---

## 👨‍💻 Author

J. Edgar Hernandez  
*IoT Assignment 1, Sapienza EMAI*
