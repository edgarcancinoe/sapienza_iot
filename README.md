# Embedded Sensor Monitoring and Visualization System

This project implements an embedded system for real-time environmental sensing, data processing, and remote visualization, using HelTec's WiFi + Lora V3 (ESP32-like) Micro-Controller board.

#### [▶ Operation Demonstration video (YouTube)](https://www.youtube.com/watch?v=XXU3Zl5cux4)

<p align="center">
  <a href="https://www.youtube.com/watch?v=XXU3Zl5cux4">
    <img src="https://img.youtube.com/vi/XXU3Zl5cux4/maxresdefault.jpg" 
         alt="Demonstration video" width="560" height="315"/>
  </a>
</p>

## On energy and power
An TI INA219 sensor was employed to measure the energy usage of the microcontroller. In the following video, the plot of voltages, current, and power can be seen, althoug the most notorious graph corresponds to the power (mW). The operation of the implementation requires ~600mW (it ranges from 400mW to 800mW, centered at approximately 600mW) of instantaneous power for the sampling + WiFi transmision, and goes as low as ~50mW when in deep sleep mode.

#### [▶ Power consumption Demonstration video (YouTube)](https://www.youtube.com/watch?v=D7BbXF6Wk3c)
<p align="center">
  <a href="https://www.youtube.com/watch?v=D7BbXF6Wk3c">
    <img src="https://img.youtube.com/vi/D7BbXF6Wk3c/maxresdefault.jpg" 
         alt="Demonstration video" width="560" height="315"/>
  </a>
</p>
---

## 📦 Project Structure

```
├── config.h                # Centralized configuration for MQTT, pins, thresholds
├── utils.h                 # Utility functions: moving average, value mapping, memory usage
├── tasks.h                 # Task declarations for FreeRTOS: sensor, MQTT
├── ;   # Real-time data visualization of MQTT data
├── visualization.py        # Serial-debug data visualization interface
```

---

## 🧠 Overview
1. Firmware generates a synthetic sinusoidal signal.
2. At startup, performs the FFT N times, averages the results to identify the fastest relevant frequency, and then adjusts the sampling rate based on that data using the Nyquist criterion for efficient sampling.
3. Processes the signal, aggregates values over a configurable window, and publishes the results via MQTT.
4. Python scripts visualize the data by reading from the serial port and subscribing to MQTT topics.

---

## ⚙️ File Details

### `config.h`
Defines all necessary constants
### `utils.h`
Utility functions used across tasks
### `tasks.h`
FreeRTOS task functions:
- `MQTTTask()`
- `FFTTask()`
- `SensorTask()`
- `AggregateTask()`

---

## 📊 Visualization Tools (Python)

### `mqtt_visualization.py`
- Connects to the same MQTT broker
- Plot sensor data in real-time
- Measures and shows the frequency with which data is sent via MQTT

### `visualization.py`
- Uses serial-published data to show sensing
- Measures and shows the sampling frequency with which sensor data is sent via Serial port


---

## 📡 MQTT Topics

- `mosquitto -c mosquitto.conf -v`
- `mosquitto_sub -h localhost -t iot/aggregate`

---

## 🧑‍💻 Author

- *[Jose Edgar Hernandez Cancino Estrada]*


