# Embedded Sensor Monitoring and Visualization System

This project is a comprehensive embedded system for real-time environmental sensing, data processing, and remote visualization. It combines C-based firmware running on a microcontroller with Python-based MQTT data visualization tools.

---

## 📦 Project Structure

```
├── config.h                # Centralized configuration for MQTT, pins, thresholds
├── utils.h                 # Utility functions: moving average, value mapping, memory usage
├── tasks.h                 # Task declarations for FreeRTOS: sensor, LED, MQTT
├── mqtt_visualization.py  # Real-time data visualization via MQTT using Matplotlib
├── visualization.py        # Alternate MQTT visualization interface
```

---

## 🧠 Overview

The firmware collects sensor data (e.g., temperature), processes it, and publishes values over MQTT. LED indicators respond to threshold conditions. Python scripts visualize the data in real-time using MQTT subscriptions.

---

## ⚙️ Firmware Details (C / FreeRTOS)

### `config.h`
Defines all necessary constants:
- MQTT broker address, port, client ID
- Topic names for publishing and subscribing
- LED pin numbers and control thresholds
- Sampling interval for sensor data

### `utils.h`
Utility functions used across tasks:
- `mapValue()`: Maps one range to another (e.g., sensor to LED brightness)
- `getFreeHeap()`: Memory usage insights
- `MovingAverage`: Class to smooth sensor values

### `tasks.h`
FreeRTOS task functions:
- `taskSensor()`: Reads and processes sensor data
- `taskMQTT()`: Publishes data to MQTT broker
- `taskLED()`: Controls LED status based on thresholds
- Modular task design improves maintainability and debugging

---

## 📊 Visualization Tools (Python)

### `mqtt_visualization.py`
- Connects to the same MQTT broker
- Subscribes to published topics
- Uses `matplotlib` to plot sensor data in real-time
- Ideal for live monitoring and debugging

### `visualization.py`
- Similar structure with potential differences in:
  - Plotting layout
  - Topic filtering
  - UI behavior
- May offer extended or alternate data visualization setups

---

## 🚀 Getting Started

### Requirements
- **Firmware:** PlatformIO / Arduino with FreeRTOS support
- **Python:**
  ```bash
  pip install paho-mqtt matplotlib
  ```

### Steps
1. Flash firmware to the embedded board.
2. Run `mqtt_visualization.py` or `visualization.py` to monitor data.
3. Adjust `config.h` for different thresholds or topics.

---

## 📡 MQTT Topics

- `sensor/temperature` — published sensor readings
- `device/status` — operational status
- `led/state` — LED control signal

(Make sure topics match the broker you're using.)

---

## 🧰 Extending the System

- Add new sensors and update `taskSensor()`.
- Modify visualizations for additional metrics.
- Use `utils.h` to ensure reusable logic.

---

## 🧑‍💻 Authors

- System Design: *[Your Name]*
- Firmware Development: *[Your Name]*
- Visualization Tools: *[Your Name]*

---

## 📄 License

MIT License. See `LICENSE` for more information.
