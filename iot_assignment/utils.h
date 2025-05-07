#ifndef UTILS_H
#define UTILS_H

// Display
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// ----------- DEBUGING -----–-------

// Thread-safe serial print
void safeSerialPrintln(const String& line, SemaphoreHandle_t serialMutex) {
  if (SERIAL_DEBUG && serialMutex != NULL) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
      Serial.println(line);
      xSemaphoreGive(serialMutex);
    }
  }
}

template <typename T>
struct StampedMsg {
  uint64_t timeStamp;
  T payload;
};

template <typename T>
String formatStampedMessage(const StampedMsg<T>& msg, const String& key) {
  String out = "#TS:" + String(msg.timeStamp) + "\t#" + key + ":\t" + String(msg.payload, 10);
  return out;
}

// --------- SIGNAL GENERATION ---------

// Signal configuration
typedef struct {
  float a_k[2];
  float f_k[2];
  int N;
} SignalConfig;

// Helper function for random float in range
float randomFloat(float min, float max) {
  return min + ((float)random() / (float)RAND_MAX) * (max - min);
}

void serialDescribeSignal(SignalConfig sim_signal, SemaphoreHandle_t serialMutex) {
  char buf[128];
    snprintf(buf, sizeof(buf),
             "[INFO] sim_signal has %d components:",
             sim_signal.N);
    safeSerialPrintln(String(buf), serialMutex);

    for (int i = 0; i < sim_signal.N; i++) {
      char buf[128];
      snprintf(buf, sizeof(buf),
              "[INFO]  - Component %d: %.2f Hz @ %.2f amplitude",
              i + 1,
              sim_signal.f_k[i],
              sim_signal.a_k[i]);
      safeSerialPrintln(String(buf), serialMutex);
    }
    delay(1000);
}

// Create a random signal config
SignalConfig createRandomSignal(SemaphoreHandle_t serialMutex) {
  int numComponents = random(1, MAX_COMPONENTS + 1);  // 2 or 3

  SignalConfig sim_signal;
  sim_signal.N = numComponents;

  for (int i = 0; i < numComponents; i++) {
    sim_signal.f_k[i] = randomFloat(MIN_FREQ, MAX_FREQ);     // Frequency
    sim_signal.a_k[i] = randomFloat(MIN_AMP, MAX_AMP);      // Amplitude 
  }
  
  if (SERIAL_DEBUG) {
    safeSerialPrintln("Creating random signal.", serialMutex);
    serialDescribeSignal(sim_signal, serialMutex);
  }

  return sim_signal;
}

float sampleSignal(float t, const SignalConfig* cfg) {
  float result = 0;
  for (int i = 0; i < cfg->N; i++) {
    result += cfg->a_k[i] * sin(2 * PI * cfg->f_k[i] * t);
  }
  return result;
}

// ---------- CONNECTIVITY ----------
void setupWiFi() {
  delay(10);
  WiFi.begin(ssid, password);
  Serial.println("[DEBUG] Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(" .");
  }
  Serial.print("\n");
  Serial.println("[DEBUG] WiFi connected. IP: " + WiFi.localIP().toString());
}

void reconnectMQTT(PubSubClient* mqttClient) {
  while (!mqttClient->connected()) {
    Serial.println("[DEBUG] Connecting to MQTT...");
    if (mqttClient->connect("ESP32Client")) {
      Serial.println("[DEBUG] MQTT Connected!");
    } else {
      Serial.println("[WARNING] failed, rc=" + String(mqttClient->state()) + ", trying again in 3 seconds");
      delay(3000);
    }
  }
}

// ------------- DISPLAY -------------

void VextON(void) {
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

void initialDisplaySetup() {
  if (TURN_ON_DISPLAY) {
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
}

#endif