#ifndef CONFIG_H
#define CONFIG_H

// DEBUG
#define SERIAL_DEBUG true
#define PUBLISH_SIGNAL true
#define PUBLISH_AGGREGATE true

// DeepSleep configuration
#define SLEEP_DURATION_SEC 4
#define AWAKE_DURATION_SEC 8
// Signal construction
#define MAX_COMPONENTS 2
#define MIN_FREQ 5
#define MAX_FREQ 20
#define MIN_AMP 15
#define MAX_AMP 20

// Sampling Configuration
#define MIN_SAMPLING_FREQ 10.0
#define MAX_SAMPLING_FREQ 200.0
#define SAMPLE_BUFFER_SIZE 1024

// FFT Configuration
#define N_FFT_RUNS 2
#define FFT_SAMPLE_SIZE 1024
#define FFT_TASK_RATE 50 // MS

// Aggregation Configuration
#define AGGREGATE_WINDOW_DURATION 0.25f // seconds
#define AGGREGATE_TASK_RATE 1 // MS

// MQTT Configuration
#define MQTT_TASK_RATE 10 // MS

// WiFi Configuration
const char* ssid = "FRITZ!Box 7530 LP";
const char* password = "70403295595551907386";

// const char* ssid = "iPhone de Edgar";
// const char* password = "01234567";

// const char* ssid = "PortofinoHomes";
// const char* password = "Portofinohomes_2203";

// MQTT Configuration
const char* mqtt_server = "192.168.178.50"; // Rome
// const char* mqtt_server    = "172.20.10.2"; // Hotspot
// const char* mqtt_server = "192.168.0.108"; // Altro

const int mqtt_port = 1883;
const char* mqtt_topic = "iot/aggregate";


#endif