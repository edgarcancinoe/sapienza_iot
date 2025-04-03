#include <Wire.h>               
#include "HT_SSD1306Wire.h"
#include "arduinoFFT.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "heltec.h"

//////////////////////      DECLARATIONS      //////////////////////

// Sampling Configuration
#define MIN_SAMPLING_FREQ 16.0
#define MAX_SAMPLING_FREQ 48.0
#define SAMPLE_BUFFER_SIZE 256

// FFT Configuration
#define FFT_SAMPLE_SIZE 128
#define FFT_TASK_RATE 3000 // MS

// Aggregation Configuration
#define AGGREGATE_WINDOW_DURATION 5.0
#define AGGREGATE_TASK_RATE 5000 // MS

// MQTT Configuration
#define MQTT_TASK_RATE 5000 // MS

// WiFi Configuration
const char* ssid = "FRITZ!Box 7530 LP";
const char* password = "70403295595551907386";

// MQTT Configuration
const char* mqtt_server = "192.168.178.50";
const int mqtt_port = 1883;
const char* mqtt_topic = "iot/aggregate";

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);

const bool serial_visualization = true;

// Signal configuration
typedef struct {
  float a_k[2];
  float f_k[2];
  int N;
} SignalConfig;

SignalConfig signal1 = {
  .a_k = {2.0, 4.0},
  .f_k = {8.0, 10.0},
  .N = 2
};

// FreeRTOS Mux
portMUX_TYPE samplingMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t serialMutex;

// Display
static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Sampling
float current_sampling_freq = MAX_SAMPLING_FREQ;

// DataQueues for FreeRTOS tasks
QueueHandle_t sampleQueueFFT;
QueueHandle_t sampleQueueAggregate;
QueueHandle_t aggregateQueue;

//////////////////////      FUNCTIONS      ////////////////////// 

// Turn on display
void VextON(void) {
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

// Setup Display
void initialDisplaySetup() {
  VextON();
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(4, 8, "J. Edgar Hernandez");
  display.drawString(4, 16, "Assignment 1");
  display.display();  
}

// WiFi connection
void setupWiFi() {
  delay(10);
  Serial.println("[INFO] Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("[INFO].");
  }
  Serial.print("\n");
  Serial.println("[INFO] WiFi connected. IP: " + WiFi.localIP().toString());
  
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.println("[INFO] Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("[INFO] MQTT Connected!");
    } else {
      Serial.println("[WARNING] failed, rc=" + String(mqttClient.state()) + ", trying again in 3 seconds");
      delay(3000);
    }
  }
}

// Thread-safe serial print
void safeSerialPrintln(const String& line) {
  if (serial_visualization && serialMutex != NULL) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
      Serial.println(line);
      xSemaphoreGive(serialMutex);
    }
  }
}

// Generate the sin() aggregation signal
float generateSignal(float t, const SignalConfig* cfg) {
  float result = 0;
  for (int i = 0; i < cfg->N; i++) {
    result += cfg->a_k[i] * sin(2 * PI * cfg->f_k[i] * t);
  }
  return result;
}

// Sensor Task
void SensorTask(void *param) {
  SignalConfig* cfg = (SignalConfig*) param;
  float t = 0.0;
  
  while (true) {
    taskENTER_CRITICAL(&samplingMux);
    float dt = 1.0 / current_sampling_freq;
    taskEXIT_CRITICAL(&samplingMux);

    float sample = generateSignal(t, cfg);
    
    // Print sample
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "#SAMPLE:\t%.4f", sample);
    safeSerialPrintln(buffer);

    // Send to queues
    if (xQueueSend(sampleQueueFFT, &sample, pdMS_TO_TICKS(50)) != pdTRUE) {
      safeSerialPrintln("[ERROR] Timeout while sending to sampleQueueFFT");
    }
    if (xQueueSend(sampleQueueAggregate, &sample, pdMS_TO_TICKS(50)) != pdTRUE) {
      safeSerialPrintln("[ERROR] Timeout while sending to sampleQueueAggregate");
    }

    t += dt;
    vTaskDelay(pdMS_TO_TICKS(dt * 1000));
  }
}

// FFT Task
void FFTTask(void *param) {
  while (true) {
    float vReal[FFT_SAMPLE_SIZE], vImag[FFT_SAMPLE_SIZE];

    for (int i = 0; i < FFT_SAMPLE_SIZE; i++) {
      if (xQueueReceive(sampleQueueFFT, &vReal[i], pdMS_TO_TICKS(50)) != pdTRUE) {
        safeSerialPrintln("[ERROR] Timeout while receiving from sampleQueueFFT in FFTTask");
        i--; // retry this index
        continue;
      }
      vImag[i] = 0.0;
    }

    ArduinoFFT<float> FFT(vReal, vImag, FFT_SAMPLE_SIZE, current_sampling_freq, false);
    FFT.windowing(vReal, FFT_SAMPLE_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SAMPLE_SIZE, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLE_SIZE);

    // Print FFT result
    char freqMsg[128];
    snprintf(freqMsg, sizeof(freqMsg), "#SAMPLING_FREQ: %.4f", current_sampling_freq);
    safeSerialPrintln(freqMsg);

    if (serial_visualization) {
      char fftLine[1024];
      size_t offset = 0;
      for (int i = 0; i < FFT_SAMPLE_SIZE / 2; i++) {
        int written = snprintf(fftLine + offset, sizeof(fftLine) - offset, "%.4f%s", vReal[i], (i < FFT_SAMPLE_SIZE / 2 - 1) ? "\t" : "");
        if (written <= 0) break;
        offset += written;
      }
      safeSerialPrintln(fftLine);
    }

    float peakFrequency = FFT.majorPeak(vReal, FFT_SAMPLE_SIZE, current_sampling_freq);
    if (isnan(peakFrequency) || peakFrequency <= 0.0 || peakFrequency > current_sampling_freq / 2.0) {
      peakFrequency = MIN_SAMPLING_FREQ * 2.0;
    }

    float newSamplingFreq = 2.1 * peakFrequency;
    newSamplingFreq = constrain(newSamplingFreq, MIN_SAMPLING_FREQ, MAX_SAMPLING_FREQ);

    taskENTER_CRITICAL(&samplingMux);
    current_sampling_freq = newSamplingFreq;
    taskEXIT_CRITICAL(&samplingMux);

    vTaskDelay(pdMS_TO_TICKS(FFT_TASK_RATE));
  }
}

// Aggregation Task
void AggregateTask(void *param) {
  while (true) {
    float current_freq;
    taskENTER_CRITICAL(&samplingMux);
    current_freq = current_sampling_freq;
    taskEXIT_CRITICAL(&samplingMux);

    int available = uxQueueMessagesWaiting(sampleQueueAggregate);
    int n_samples = min((int)(current_freq * AGGREGATE_WINDOW_DURATION), available);
    
    float sum = 0;
    int received = 0;

    for (int i = 0; i < n_samples; i++) {
      float sample;
      if (xQueueReceive(sampleQueueAggregate, &sample, pdMS_TO_TICKS(50)) == pdTRUE) {
        sum += sample;
        received++;
      } else {
        safeSerialPrintln("[ERROR] Timeout receiving from sampleQueueAggregate");
      }
    }

    float avg = (received > 0) ? (sum / received) : 0.0;

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "#AGGREGATE:\t%.4f", avg);
    safeSerialPrintln(buffer);

    if (xQueueSend(aggregateQueue, &avg, pdMS_TO_TICKS(5)) != pdTRUE) {
        float dummy;
        xQueueReceive(aggregateQueue, &dummy, 0);
        xQueueSend(aggregateQueue, &avg, 0);
        safeSerialPrintln("[WARNING] aggregateQueue was full, oldest value dropped");
    }

    vTaskDelay(pdMS_TO_TICKS(AGGREGATE_TASK_RATE));
  }
}

// MQTT Task
void MQTTTask(void *param) {
  while (true) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    // Mantain connection alive
    mqttClient.loop();

    // Retrieve average and sent via MQTT
    float avg;

    if (xQueueReceive(aggregateQueue, &avg, pdMS_TO_TICKS(5000)) == pdTRUE) {
      char payload[64];
      snprintf(payload, sizeof(payload), "{\"average\": %.4f}", avg);

      if (mqttClient.publish(mqtt_topic, payload)) {
        safeSerialPrintln("[MQTT] Published: " + String(payload));
      } else {
        safeSerialPrintln("[MQTT] Failed to publish");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(MQTT_TASK_RATE));
  }
}

// **** Setup ****
void setup() {
  
  Serial.begin(115200);
  
  
  serialMutex = xSemaphoreCreateMutex();

  // WiFi & MQTT
  setupWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);

  // LoRa Setup
  Heltec.begin(false /*Display*/, true /*LoRa*/, true /*Serial*/, true /*PABOOST*/, REGION_EU868);
  
  initialDisplaySetup();

  sampleQueueFFT = xQueueCreate(SAMPLE_BUFFER_SIZE, sizeof(float));
  sampleQueueAggregate = xQueueCreate(SAMPLE_BUFFER_SIZE, sizeof(float));
  aggregateQueue = xQueueCreate(10, sizeof(float));

  xTaskCreatePinnedToCore(SensorTask, "Sensor", 8192, &signal1, 1, NULL, 1);
  xTaskCreatePinnedToCore(FFTTask, "FFT", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(AggregateTask, "Aggregate", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(MQTTTask, "MQTT", 8192, NULL, 1, NULL, 0);

}

void loop() {
  // Not used with FreeRTOS setup
}