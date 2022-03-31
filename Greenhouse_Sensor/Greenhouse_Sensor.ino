//----------------------------------------------------
// Includes

#include <Wire.h>
#include "DHT.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

//----------------------------------------------------
// Defines

#define DEVICE "ESP32"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Quickly enable/disable infux logging
#define LOG_TO_INFLUX false

// Time in seconds between collecting data
#define COLLECTION_PERIOD_S (1 * 60)
#define COLLECTION_PERIOD_MS (1000 * COLLECTION_PERIOD_S)

// Temp we want the greenhouse
#define TARGET_TEMP_F 70

// Temp above which we always want the windows open
#define ALWAYS_OPEN_TEMP_F (TARGET_TEMP_F + 15)
// Temp below witch we always want the windows closed
#define ALWAYS_CLOSE_TEMP_F (TARGET_TEMP_F - 5)

// Temp delta we define as "rapid" and should assume its still going up
#define RAPID_RISE_TEMP_DELTA 10
// Time period for the temp delta to be considered "rapid"
#define RAPID_RISE_TEMP_S (30*60)
// Number of periods for the rapid rise
#define RAPID_RISE_PERIODS ((int) (RAPID_RISE_TEMP_S / COLLECTION_PERIOD_S))

// Temp below which we should close the windows if its been long enough since open
#define DROPPING_TEMP_F (TARGET_TEMP_F + 5)
// Time in seconds since last open before we consider closing again
#define DROPPING_TIME_SINCE_OPEN_S (30 * 60)
#define DROPPING_TIME_SINCE_OPEN_MS (1000 * DROPPING_TIME_SINCE_OPEN_S)

#define TEMP_HISTORY_DEPTH 50

#define WIFI_SSID "Terrace House"
#define WIFI_PASSWORD "kawai!!!!"

#define INFLUXDB_URL "http://192.168.1.223:8086"
#define INFLUXDB_DB "greenhouse"

// InfluxDB v2 timezone
#define TZ_INFO "UTC−07"

#define DHT_TYPE DHT22
#define DT22_PIN 13

// #define WINDOW_OPEN_PIN 12
// #define WINDOW_CLOSE_PIN 14

#define WINDOW_OPEN_PIN 34
#define WINDOW_CLOSE_PIN 35

// Time in seconds to wait for window to close
#define WINDOW_MOVE_TIME_S 20
#define WINDOW_MOVE_TIME_MS 1000*WINDOW_MOVE_TIME_S

#define SERIAL_SPEED 115200

//----------------------------------------------------
// Globals

AsyncWebServer server(80);

DHT dht(DT22_PIN, DHT_TYPE);
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB);
Point sensor("weather");
Point window_state("window_events");

float temp, humidity;
float temp_history[TEMP_HISTORY_DEPTH];

// Populate the temp_history on first reading
bool history_init = true;

long last_open_time_ms = 0;
bool window_open = false;

//----------------------------------------------------
// Functions

void setup() {
  // Start serial communication
  Serial.begin(SERIAL_SPEED);

  init_window_control();
  init_wifi();
  init_sensor();
  init_influx();
}

void init_window_control() {
  Serial.println("Initialziing pins");
  pinMode(WINDOW_OPEN_PIN, OUTPUT);
  pinMode(WINDOW_CLOSE_PIN, OUTPUT);

  digitalWrite(WINDOW_CLOSE_PIN, HIGH);
  digitalWrite(WINDOW_OPEN_PIN, HIGH);
}

void init_wifi() {
  Serial.println("\n");
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  WebSerial.begin(&server);
  server.begin();
}

void init_sensor() {
  Serial.println("Initialziing DHT22");
  // Connect to the DHT Sensor
  dht.begin();
}

void init_influx() {
  Serial.println("Initialziing InfluxDB");

  // Setup tags to send with influx datapoints
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WIFI_SSID);

  // Accurate time is necessary for certificate validation and writing in batches
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void collect_new_reading() {
  humidity = dht.readHumidity();

  // Read temperature as Fahrenheit (isFahrenheit = true)
  temp = dht.readTemperature(true);

  // Clear fields for reusing the point. Tags will remain untouched
  sensor.clearFields();

  // Store temp & humidity
  sensor.addField("temperature", temp);
  sensor.addField("humidity", humidity);
}

void annotate_open_window(const char * reason, float trigger) {
  sensor.clearFields();
  window_state.addField("is_open", true);
  window_state.addField("reason", reason);
  window_state.addField("trigger", trigger);
  

  if (!client.writePoint(window_state)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void annotate_close_window(const char * reason, float trigger) {
  sensor.clearFields();
  window_state.addField("is_open", false);
  window_state.addField("reason", reason);
  window_state.addField("trigger", trigger);

  if (!client.writePoint(window_state)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void init_temp_history() {
  // Initialize all history values to the current temperature 
  for (int i = 0; i < TEMP_HISTORY_DEPTH; i++) {
    Serial.print("Setting history[");
    Serial.print(i);
    Serial.print("] = ");
    Serial.println(temp);
    temp_history[i] = temp;
  }
}

void add_temp_history() {
  // Shift all history right, dropping earliest value
  for (int i = TEMP_HISTORY_DEPTH - 1; i > 0; i--) {
    temp_history[i] = temp_history[i-1];
  }
  temp_history[0] = temp;
}

void write_reading_to_influx() {
  if (LOG_TO_INFLUX) {
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
  }

  Serial.print("Temp: ");
  Serial.println(temp);
  Serial.print("Humidity: ");
  Serial.println(humidity);
}

bool need_window_opened() {
  // Don't continue if window is already open
  if (window_open) {
    return false;
  }

  // Open unconditionally if temp is high enough
  if (temp >= ALWAYS_OPEN_TEMP_F) {
    Serial.print("Opening window (absolute): ");
    Serial.print(temp);
    Serial.print("F >= ");
    Serial.print(ALWAYS_OPEN_TEMP_F);
    Serial.println("F");

    annotate_open_window("absolute", temp);
    return true;
  }

  // Open depending on how quickly temp is rising
  float start_period_temp = temp_history[RAPID_RISE_PERIODS];
  float temp_delta = temp_history[0] - start_period_temp;
  if (temp >= TARGET_TEMP_F) {
    if (temp_delta > RAPID_RISE_TEMP_DELTA) {
      Serial.print("Rapid rise periods: ");
      Serial.println(RAPID_RISE_PERIODS);
      
      Serial.print("Opening window (conditional): ");
      Serial.print(temp_delta);
      Serial.print("F > ");
      Serial.print(RAPID_RISE_TEMP_DELTA);
      Serial.println("F");
      
      annotate_open_window("conditional", temp_delta);
      return true;
    }
  }

  return false;
}

bool need_window_closed() {
  // Don't continue if window is already closed
  if (!window_open) {
    return false;
  }

  // Close unconditionally if temp is low enough
  if (temp < ALWAYS_CLOSE_TEMP_F) {
    Serial.print("Closing window (absolute): ");
    Serial.print(temp);
    Serial.print("F < ");
    Serial.print(ALWAYS_CLOSE_TEMP_F);
    Serial.println("F");

    annotate_close_window("absolute", temp);
    return true;
  }

  // Close a bit higher than set point, but only if enough time has passed
  // since last open
  if (temp < DROPPING_TEMP_F) {
    long open_delta = millis() - last_open_time_ms;
    if (open_delta > DROPPING_TIME_SINCE_OPEN_MS) {
      Serial.print("Closing window (conditional): ");
      Serial.print(temp);
      Serial.print("F < ");
      Serial.print(DROPPING_TEMP_F);
      Serial.print("F AND ");
      Serial.print(open_delta);
      Serial.print("ms > ");
      Serial.print(DROPPING_TIME_SINCE_OPEN_MS);
      Serial.println("ms");

      annotate_close_window("conditional", open_delta);
      return true;
    }
  }

  return false;
}

void open_window() {
  digitalWrite(WINDOW_OPEN_PIN, LOW);
  delay(WINDOW_MOVE_TIME_MS);
  digitalWrite(WINDOW_OPEN_PIN, HIGH);

  window_open = true;
  last_open_time_ms = millis();
}

void close_window() {
  digitalWrite(WINDOW_CLOSE_PIN, LOW);
  delay(WINDOW_MOVE_TIME_MS);
  digitalWrite(WINDOW_CLOSE_PIN, HIGH);

  window_open = false;
}

void loop() {
  collect_new_reading();
  write_reading_to_influx();

  // Initialize the history on first reading
  if (history_init) {
    Serial.println("Initializing history");
    init_temp_history();
    history_init = false;
  }

  long wait_ms = COLLECTION_PERIOD_MS;
  if (need_window_opened()) {
    Serial.println("Opening window ...");
    open_window();
    wait_ms -= WINDOW_MOVE_TIME_MS;
  } else if (need_window_closed()) {
    Serial.println("Closing window ...");
    close_window();
    wait_ms -= WINDOW_MOVE_TIME_MS;
  }

  WebSerial.print("Waiting for ");
  WebSerial.print((float) (wait_ms/1000));
  WebSerial.println("s");

  Serial.print("Waiting for ");
  Serial.print(wait_ms/1000);
  Serial.println("s");
  Serial.flush();
  delay(wait_ms);
}
