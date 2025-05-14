#include <Arduino.h>
#include <SPI.h> 
#include "config.h"
#include "HT_SSD1306Wire.h"
#include "arduinoFFT.h"
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include "esp_system.h"
#include <stdbool.h>
#include "esp_sleep.h"

// General function and data struct implementations
#include "utils.h"

// RTC Memory variables 
RTC_DATA_ATTR SignalConfig sim_signal; // Will be initialized in setup()
RTC_DATA_ATTR float current_sampling_freq = MAX_SAMPLING_FREQ;
RTC_DATA_ATTR extern volatile bool fftDone = false;
RTC_DATA_ATTR uint64_t rtcStartUs = 0;

// -------- Global Variable Declarations --------

// WiFi and MQTT clients as globals so they persist for the lifetime of the program
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Global FreeRTOS mux and sampling configuration values
portMUX_TYPE samplingMux = portMUX_INITIALIZER_UNLOCKED;

// Global synchronization and queue objects
SemaphoreHandle_t serialMutex;
QueueHandle_t sampleQueueFFT;
QueueHandle_t sampleQueueAggregate;
QueueHandle_t aggregateQueue;

// Task implementations
#include "tasks.h"

void setup() {
  //////////////////////      Start      //////////////////////
  randomSeed(esp_random());
  Serial.begin(115200);
  SPI.begin();     
  
  serialMutex = xSemaphoreCreateMutex();
  rtcStartUs = 0;
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    if (SERIAL_DEBUG) { // Means we entered setup from deep sleep
      safeSerialPrintln("[INFO] Waking up from deep sleep.", serialMutex);
      serialDescribeSignal(sim_signal, serialMutex);
      safeSerialPrintln(String("[INFO] Sampling frequency set to: ") + String(current_sampling_freq,4), serialMutex);
      safeSerialPrintln("[DEBUG] FFT WILL NOT BE RECALCULATED.", serialMutex);
    }
  } else {
    if (SERIAL_DEBUG) safeSerialPrintln("[DEBUG] FFT WILL BE RECALCULATED.", serialMutex);
    // Initialize signal config
    sim_signal = createRandomSignal(serialMutex); 
  }

  // Initialize global queues
  sampleQueueFFT       = xQueueCreate(SAMPLE_BUFFER_SIZE, sizeof(float));
  sampleQueueAggregate = xQueueCreate(SAMPLE_BUFFER_SIZE, sizeof(StampedMsg<float>));
  aggregateQueue       = xQueueCreate(SAMPLE_BUFFER_SIZE, sizeof(StampedMsg<float>));
    
  setupWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  initialDisplaySetup();

  // Task parameters
  SensorTaskParams* sensorParams = new SensorTaskParams{&sim_signal, &current_sampling_freq, &samplingMux, serialMutex, sampleQueueFFT, sampleQueueAggregate};
  AggregateTaskParams* aggParams = new AggregateTaskParams{sampleQueueAggregate, aggregateQueue, &current_sampling_freq, &samplingMux, serialMutex};
  MQTTTaskParams* mqttParams = new MQTTTaskParams{ &mqttClient, serialMutex, aggregateQueue };
  FFTTaskParams* fftParams = new FFTTaskParams{sampleQueueFFT, &current_sampling_freq, &samplingMux, serialMutex};

  // Initialize tasks
  xTaskCreatePinnedToCore(SensorTask, "Sensor", 8192, sensorParams, 1, NULL, 1);
  if (!fftDone) xTaskCreatePinnedToCore(FFTTask, "FFT", 8192, fftParams, 2, nullptr, 0);
  xTaskCreatePinnedToCore(AggregateTask, "Aggregate", 8192, aggParams, 1, NULL, 1);
  xTaskCreatePinnedToCore(MQTTTask, "MQTT", 8192, mqttParams, 1, NULL, 0);

}

void loop() {
}