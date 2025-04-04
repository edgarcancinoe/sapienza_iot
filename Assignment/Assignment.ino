#include <Wire.h>               
#include "HT_SSD1306Wire.h"
#include "arduinoFFT.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_system.h" // Required for esp_random()

//////////////////////      DECLARATIONS      //////////////////////

// Sampling Configuration
#define MIN_SAMPLING_FREQ 50.0
#define MAX_SAMPLING_FREQ 1000.0
#define SAMPLE_BUFFER_SIZE 256

// FFT Configuration
#define FFT_SAMPLE_SIZE 256
#define FFT_TASK_RATE 2000 // MS

// Aggregation Configuration
#define AGGREGATE_WINDOW_DURATION .2
#define AGGREGATE_TASK_RATE 200 // MS

// MQTT Configuration
#define MQTT_TASK_RATE 200 // MS

// WiFi Configuration
const char* ssid = "iPhone de Edgar";
const char* password = "01234567";

// MQTT Configuration
const char* mqtt_server = "172.20.10.2";
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

#define MAX_COMPONENTS 3

SignalConfig sim_signal;  // This will hold the randomly generated signal

// Helper function for random float in range
float randomFloat(float min, float max) {
  return min + ((float)random() / (float)RAND_MAX) * (max - min);
}

// Create a random signal config
void createRandomSignal() {
  int numComponents = random(2, MAX_COMPONENTS + 1);  // 2 or 3
  sim_signal.N = numComponents;

  for (int i = 0; i < numComponents; i++) {
    sim_signal.f_k[i] = randomFloat(20.0, 200.0);     // Frequency between 20-200 Hz
    sim_signal.a_k[i] = randomFloat(10.0, 25.0);      // Amplitude between 10-25
  }

  Serial.printf("[INFO] Created random sim_signal with %d components:\n", sim_signal.N);
  for (int i = 0; i < MAX_COMPONENTS; i++) {
    Serial.println(sim_signal.N);
    Serial.printf("[INFO]  - Component %d: %.2f Hz @ %.2f amplitude\n", i + 1, sim_signal.f_k[i], sim_signal.a_k[i]);
  }
}

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
    delay(100);
    Serial.print(".");
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
      if (xQueueReceive(sampleQueueFFT, &vReal[i], pdMS_TO_TICKS(100)) != pdTRUE) {
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

    float sum = 0.0;
    for (int i = 1; i < FFT_SAMPLE_SIZE / 2; i++) {
      sum += vReal[i];
    }
    float mean = sum / (FFT_SAMPLE_SIZE / 2 - 1);

    float variance = 0.0;
    for (int i = 1; i < FFT_SAMPLE_SIZE / 2; i++) {
      float diff = vReal[i] - mean;
      variance += diff * diff;
    }
    float stddev = sqrt(variance / (FFT_SAMPLE_SIZE / 2 - 1));

    float threshold = mean + 2.0 * stddev;

    float maxPeakValue = 0.0;
    float maxPeakFrequency = 0.0;
    
    safeSerialPrintln("[INFO] Detected components:");
    for (int i = 0; i < FFT_SAMPLE_SIZE / 2; i++) {
      if (vReal[i] > threshold) {
        float freq = (i * current_sampling_freq) / FFT_SAMPLE_SIZE;
        char compLine[64];
        snprintf(compLine, sizeof(compLine), "#COMPONENT:\t%.2f\t%.2f", freq, vReal[i]);
        safeSerialPrintln(compLine);
        if (vReal[i] > maxPeakValue) {
          maxPeakValue = vReal[i];
          maxPeakFrequency = freq;
        }
      }
    }

    if (isnan(maxPeakFrequency) || maxPeakFrequency <= 0.0 || maxPeakFrequency > current_sampling_freq / 2.0) {
      maxPeakFrequency = MIN_SAMPLING_FREQ * 2.0;
    }

    float newSamplingFreq = 2.1 * maxPeakFrequency;

    // Print Freq result
    char freqMsg[128];
    snprintf(freqMsg, sizeof(freqMsg), "[INFO] Max. Frequency Detected: %.4f", maxPeakFrequency);
    safeSerialPrintln(freqMsg);
    snprintf(freqMsg, sizeof(freqMsg), "#SAMPLING_FREQ: %.4f", current_sampling_freq);
    safeSerialPrintln(freqMsg);

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
  
  randomSeed(esp_random());
  createRandomSignal();     

  
  serialMutex = xSemaphoreCreateMutex();

  // WiFi & MQTT
  setupWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  
  initialDisplaySetup();

  sampleQueueFFT = xQueueCreate(SAMPLE_BUFFER_SIZE, sizeof(float));
  sampleQueueAggregate = xQueueCreate(SAMPLE_BUFFER_SIZE, sizeof(float));
  aggregateQueue = xQueueCreate(10, sizeof(float));

  xTaskCreatePinnedToCore(SensorTask, "Sensor", 8192, &sim_signal, 1, NULL, 1);
  xTaskCreatePinnedToCore(FFTTask, "FFT", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(AggregateTask, "Aggregate", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(MQTTTask, "MQTT", 8192, NULL, 1, NULL, 0);

}

void loop() {
  // Not used with FreeRTOS setup
}