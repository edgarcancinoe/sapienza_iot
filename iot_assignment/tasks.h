#ifndef TASKS_H
#define TASKS_H

// -------------- FFT ----–----------
struct FFTTaskParams {
  QueueHandle_t sampleQueueFFT;   
  float*        samplingFreq;     
  portMUX_TYPE* samplingMux;      
  SemaphoreHandle_t serialMutex;  
};

static float vReal[FFT_SAMPLE_SIZE], vImag[FFT_SAMPLE_SIZE];

void FFTTask(void *param) {
  FFTTaskParams* params = static_cast<FFTTaskParams*>(param);
  QueueHandle_t     sampleQueueFFT  = params->sampleQueueFFT;
  float*            samplingFreq    = params->samplingFreq;
  portMUX_TYPE*     samplingMux     = params->samplingMux;
  SemaphoreHandle_t serialMutex     = params->serialMutex;

  float maxFreqSum = 0.0;
  
  safeSerialPrintln("[INFO] Sarting FFT analyisis.", serialMutex);
  for (int i = 0; i < N_FFT_RUNS; i++) {
    
    fftDone = false;
    if (SERIAL_DEBUG) safeSerialPrintln(String("[INFO] Iteration no.") + String(i), serialMutex);

    // Collect samples
    for (int j = 0; j < FFT_SAMPLE_SIZE; j++) {
      if (xQueueReceive(sampleQueueFFT, &vReal[j], pdMS_TO_TICKS(100)) != pdTRUE) {
        --j;  // try again
        continue;
      }
      else {
        vImag[j] = 0;
      }
    }
    // Stop sending samples to queue and reset queue as well
    // fftDone = true;
    xQueueReset(sampleQueueFFT);
    
    // Perform FFT
    ArduinoFFT<float> FFT(vReal, vImag, FFT_SAMPLE_SIZE, *samplingFreq, false);
    FFT.windowing(vReal, FFT_SAMPLE_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SAMPLE_SIZE, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLE_SIZE);
    vTaskDelay(pdMS_TO_TICKS(FFT_TASK_RATE));

    // Find highest frequency 
    float maxPeakValue = 0.0;
    float maxPeakFrequency = 0.0;

    // Obtain mean
    float sum = 0.0;
    for (int j = 1; j < FFT_SAMPLE_SIZE/2; j++)
      sum += vReal[j];
    float mean = sum / (FFT_SAMPLE_SIZE / 2 - 1);
    
    // Obtain std
    float variance = 0.0;
    for (int j = 0; j < FFT_SAMPLE_SIZE / 2; j++) {
      float diff = vReal[j] - mean;
      variance += diff * diff;
    }
    float stddev = sqrt(variance / (FFT_SAMPLE_SIZE / 2 - 1));
    
    // Threshold value to detect peaks
    float threshold = mean + 2.0 * stddev;
    for (int j = 0; j < FFT_SAMPLE_SIZE / 2; j++) {
      if (vReal[j] > threshold) {
        float freq = (j * (*samplingFreq)) / FFT_SAMPLE_SIZE;
        if (freq > maxPeakFrequency) {
          maxPeakFrequency = freq;
        }
        if (SERIAL_DEBUG) {
          char compLine[64];
          snprintf(compLine, sizeof(compLine), "[INFO] Component:\t%.2f\t%.2f", freq, vReal[j]);
          safeSerialPrintln(compLine, serialMutex);
        }
      }
    }
    float nyqFreq = constrain(2.1f * maxPeakFrequency, MIN_SAMPLING_FREQ, MAX_SAMPLING_FREQ);
    maxFreqSum += nyqFreq;
    
    if (SERIAL_DEBUG) {
      safeSerialPrintln(String("[INFO] Max frequency found: ") + maxPeakFrequency, serialMutex);
      safeSerialPrintln(String("[INFO] Iteration's efficient frequency: ") + nyqFreq, serialMutex);
    }
  }

  float newSamplingFreq = maxFreqSum / (float) N_FFT_RUNS;

  if (SERIAL_DEBUG) {
    safeSerialPrintln(String("[INFO] \n Averaged max frequency value: ") + newSamplingFreq, serialMutex);
    safeSerialPrintln("[DEBUG] Completed FFT medition, deleting task.", serialMutex);
  }
  
  taskENTER_CRITICAL(samplingMux);
  *samplingFreq = newSamplingFreq;
  taskEXIT_CRITICAL(samplingMux);

  rtcStartUs = 0;
  fftDone = true;
  vTaskDelete(NULL);
}

// -------------- MQTT --------------
struct MQTTTaskParams {
  PubSubClient* mqttClient;
  SemaphoreHandle_t serialMutex;
  QueueHandle_t aggregateQueue;
};

void MQTTTask(void *param) {
  MQTTTaskParams* params = static_cast<MQTTTaskParams*>(param);
  PubSubClient* mqttClient = params->mqttClient;
  SemaphoreHandle_t serialMutex = params->serialMutex;
  QueueHandle_t aggregateQueue = params->aggregateQueue;

  while (true) {
    if (!mqttClient->connected()) {
      reconnectMQTT(mqttClient);
    }
    mqttClient->loop();

    StampedMsg<float> avg;
    if (xQueueReceive(aggregateQueue, &avg, pdMS_TO_TICKS(5000)) == pdTRUE) {
      char payload[64];
      snprintf(payload, sizeof(payload), "{\"average\": %.4f, \"timeStamp\": %llu}", avg.payload, (unsigned long long)avg.timeStamp);
    
      if (mqttClient->publish(mqtt_topic, payload)) {
        if (SERIAL_DEBUG) safeSerialPrintln(String("[MQTT] Published: ") + payload, serialMutex);
      } else {
        if (SERIAL_DEBUG) safeSerialPrintln("[MQTT] Failed to publish", serialMutex);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(MQTT_TASK_RATE));
  }
}

// ------------- SENSING -------------
struct SensorTaskParams {
  SignalConfig* cfg;
  float* samplingFreq;
  portMUX_TYPE* samplingMux;
  SemaphoreHandle_t serialMutex;
  QueueHandle_t sampleQueueFFT;
  QueueHandle_t sampleQueueAggregate;
};

void SensorTask(void *param) {
  SensorTaskParams* params           = static_cast<SensorTaskParams*>(param);
  SignalConfig*     cfg              = params->cfg;
  float*            samplingFreq     = params->samplingFreq;
  portMUX_TYPE*     samplingMux      = params->samplingMux;
  SemaphoreHandle_t serialMutex      = params->serialMutex;
  QueueHandle_t     sampleQueueFFT   = params->sampleQueueFFT;
  QueueHandle_t     sampleQueueAggregate = params->sampleQueueAggregate;

  uint64_t virtual_deltaUs = 0;
  float    fracAccumulator = 0.0f;

  // for periodic wake
  TickType_t xLastWakeTime = xTaskGetTickCount();
  float      lastFs        = 0.0f;
  TickType_t xDelayTicks   = 0;            // cached delay in ticks

  // --- TIME TRACKING FOR SLEEP/WAKE ---
  uint64_t sleepDurationUs = (uint64_t)SLEEP_DURATION_SEC  * 1000000ULL;
  uint64_t awakeDurationUs = (uint64_t)AWAKE_DURATION_SEC  * 1000000ULL;

  while (true) {
    
    float fs;
    taskENTER_CRITICAL(samplingMux);
      fs = *samplingFreq;
    taskEXIT_CRITICAL(samplingMux);

    if (fs != lastFs) {
      float usPerSample = 1e6f / fs;
      xDelayTicks = (TickType_t)((usPerSample * configTICK_RATE_HZ) / 1e6f + 0.5f);
      lastFs      = fs;
    }

    float usPerSample   = 1e6f / fs;
    uint64_t baseUs     = (uint64_t)usPerSample;
    fracAccumulator    += (usPerSample - (float)baseUs);
    if (fracAccumulator >= 1.0f) {
      baseUs++;
      fracAccumulator -= 1.0f;
    }
    virtual_deltaUs += baseUs;

    float tSec = virtual_deltaUs * 1e-6f;
    StampedMsg<float> sample;
    sample.timeStamp = virtual_deltaUs;
    sample.payload   = sampleSignal(tSec, cfg);

    if (PUBLISH_SIGNAL) {
      safeSerialPrintln(formatStampedMessage(sample, "SAMPLE"), serialMutex);
    }

    if (!fftDone) {
      if (xQueueSend(sampleQueueFFT, &sample.payload, pdMS_TO_TICKS(100)) != pdTRUE) {
        if (SERIAL_DEBUG) safeSerialPrintln("[ERROR] Timeout sending to sampleQueueFFT", serialMutex);
      }
    }

    if (xQueueSend(sampleQueueAggregate, &sample, pdMS_TO_TICKS(50)) != pdTRUE) {
      StampedMsg<float> dropped;
      xQueueReceive(sampleQueueAggregate, &dropped, 0);
      xQueueSend(sampleQueueAggregate, &sample, 0);
      if (SERIAL_DEBUG) safeSerialPrintln("[WARNING] sampleQueueAggregate full: dropped oldest sample", serialMutex);
    }

    // Check if time to deep sleep
    uint64_t nowUs = esp_timer_get_time();
    if (fftDone && rtcStartUs == 0) {
      rtcStartUs = nowUs;
    }
    if (fftDone && rtcStartUs != 0 && (nowUs - rtcStartUs >= awakeDurationUs)) {
      if (SERIAL_DEBUG) {
        safeSerialPrintln("[INFO] Entering deep sleep", serialMutex);
      }
      esp_sleep_enable_timer_wakeup(sleepDurationUs);
      esp_deep_sleep_start();
    }

    vTaskDelayUntil(&xLastWakeTime, xDelayTicks);
  }
}

// ---------- Aggregation ----------

struct AggregateTaskParams {
  QueueHandle_t sampleQueueAggregate;
  QueueHandle_t aggregateQueue;
  float* samplingFreq;
  portMUX_TYPE* samplingMux;
  SemaphoreHandle_t serialMutex;
};

void AggregateTask(void *param) {
  AggregateTaskParams* params = static_cast<AggregateTaskParams*>(param);
  QueueHandle_t sampleQueueAggregate = params->sampleQueueAggregate;
  QueueHandle_t aggregateQueue = params->aggregateQueue;
  float* samplingFreq = params->samplingFreq;
  portMUX_TYPE* samplingMux = params->samplingMux;
  SemaphoreHandle_t serialMutex = params->serialMutex;

  float lastFreq = -1.0f;
  int   nSamples = 0;

  while (true) {
    float currentFreq;
    taskENTER_CRITICAL(samplingMux);
    currentFreq = *samplingFreq;
    taskEXIT_CRITICAL(samplingMux);

    if (currentFreq != lastFreq) {
      lastFreq = currentFreq;
      nSamples = (int) floorf(currentFreq * AGGREGATE_WINDOW_DURATION);
      xQueueReset(sampleQueueAggregate);
      char buf[64];
      snprintf(buf, sizeof(buf),"[DEBUG] Recalculated nSamples = %d (f=%.1f Hz)", nSamples, lastFreq);
      safeSerialPrintln(buf, serialMutex);
    }
    
    // Skip until we have enough samples to perform aggregation
    int available = uxQueueMessagesWaiting(sampleQueueAggregate);
    if (nSamples > available) {
      continue;
    }

    float val_sum           = 0;
    uint64_t time_stamp_sum = 0;
    int received            = 0;

    for (int i = 0; i < nSamples; i++) {
      StampedMsg<float> sample;
      if (xQueueReceive(sampleQueueAggregate, &sample, pdMS_TO_TICKS(50)) == pdTRUE) {
        val_sum += sample.payload;
        time_stamp_sum += sample.timeStamp;
        received++;
      } else {
        safeSerialPrintln("[ERROR] Timeout receiving from sampleQueueAggregate", serialMutex);
      }
    }

    StampedMsg<float> avg;
    avg.payload = (received > 0) ? (val_sum / float(received)) : 0.0;
    avg.timeStamp = (received > 0) ? (time_stamp_sum / float(received)) : 0;
    float ts = (float) avg.timeStamp;

    if (PUBLISH_AGGREGATE) {
      String msg = formatStampedMessage(avg, "AGGREGATE");
      safeSerialPrintln(msg, serialMutex);
    }

    if (xQueueSend(aggregateQueue, &avg, pdMS_TO_TICKS(10)) != pdTRUE) {
      float dummy;
      xQueueReceive(aggregateQueue, &dummy, 0);
      xQueueSend(aggregateQueue, &avg, 0);
      safeSerialPrintln("[WARNING] aggregateQueue is full MQTT is probably not connected. Oldest value dropped.", serialMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(AGGREGATE_TASK_RATE));
  }
}

#endif