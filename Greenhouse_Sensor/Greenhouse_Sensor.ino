//----------------------------------------------------
// Includes

#include <Wire.h>
#include "DHT.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include "time.h"

//----------------------------------------------------
// Defines

#define DEVICE "ESP32"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Quickly enable/disable infux logging
#define LOG_TO_INFLUX true

// Time in seconds between collecting data
#define COLLECTION_PERIOD_S (1 * 60)
#define COLLECTION_PERIOD_MS (1000 * COLLECTION_PERIOD_S)

// Temp we want the greenhouse
#define TARGET_TEMP_F 70

// Temp above which we always want the windows open
#define ALWAYS_OPEN_TEMP_F (TARGET_TEMP_F + 15)
// Temp below witch we always want the windows closed
#define ALWAYS_CLOSE_TEMP_F (TARGET_TEMP_F - 5)

#define ALWAYS_FAN_TEMP_F (TARGET_TEMP_F + 10)
#define ALWAYS_NO_FAN_TEMP_F (TARGET_TEMP_F + 5)

// Temp delta we define as "rapid" and should assume its still going up
#define RAPID_RISE_TEMP_DELTA 10
// Time period for the temp delta to be considered "rapid"
#define RAPID_RISE_TEMP_MIN 30

// Temp below which we should close the windows if its been long enough since open
#define DROPPING_TEMP_F (TARGET_TEMP_F + 5)
// Time in seconds since last open before we consider closing again
#define DROPPING_TIME_SINCE_OPEN_S (30 * 60)
#define DROPPING_TIME_SINCE_OPEN_MS (1000 * DROPPING_TIME_SINCE_OPEN_S)
// How long in minutes the temp needs to be consistently falling to be considered cooling down
#define FALLING_TEMP_MIN 120

#define TEMP_HISTORY_DEPTH 50

#define WIFI_SSID "Terrace Shed"
#define WIFI_PASSWORD "iot4life"
#define HOST_NAME "greenhouse-monitor-iot"

#define INFLUXDB_URL "http://tiger-pi.local:8086"
#define INFLUXDB_DB "greenhouse"

// InfluxDB v2 timezone
#define TZ_INFO "UTC-07"
// From https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
#define time_zone "PST8PDT,M3.2.0,M11.1.0"
#define ntpServer1 "pool.ntp.org"
#define ntpServer2 "time.nis.gov"

#define DHT_TYPE DHT22
#define DT22_PIN 13

#define WINDOW_OPEN_PIN 33
#define WINDOW_CLOSE_PIN 32

#define FAN_CONTROL_PIN 25

// Time in seconds to wait for window to close
#define WINDOW_MOVE_TIME_S 20
#define WINDOW_MOVE_TIME_MS 1000*WINDOW_MOVE_TIME_S

#define SERIAL_SPEED 115200

// How often to poll for webserial actions
#define POLL_DELAY_MS 500

//----------------------------------------------------
// Globals

AsyncWebServer server(80);

time_t startup;

DHT dht(DT22_PIN, DHT_TYPE);
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB);
Point sensor("weather");
Point window_state("window_events");
Point fan_state("fan_events");

float temp, humidity;
float temp_history[TEMP_HISTORY_DEPTH];

// Populate the temp_history on first reading
bool history_init = true;

long last_open_time_ms = 0;
bool window_open = false;
bool fan_is_on = false;

bool trigger_status = false;
bool trigger_dump = false;
bool trigger_delta = false;
bool trigger_fan_on = false;
bool trigger_fan_off = false;
bool trigger_open = false;
bool trigger_close = false;

//----------------------------------------------------
// Functions

void setup() {
  // Start serial communication
  Serial.begin(SERIAL_SPEED);

  init_fan_control();
  init_window_control();
  init_wifi();
  init_sensor();
  init_influx();
}

void init_fan_control() {
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  digitalWrite(FAN_CONTROL_PIN, LOW);
}

void init_window_control() {
  Serial.println("Initialziing pins");
  pinMode(WINDOW_OPEN_PIN, OUTPUT);
  pinMode(WINDOW_CLOSE_PIN, OUTPUT);

  digitalWrite(WINDOW_CLOSE_PIN, LOW);
  digitalWrite(WINDOW_OPEN_PIN, LOW);
}

void init_wifi() {
  listNetworks();

  Serial.println("\n");
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t status = WiFi.status();
  while (status != WL_CONNECTED) {
    switch (status) {
      case WL_NO_SHIELD:
        Serial.println("\tResult: No sheild");
        break;
      case WL_IDLE_STATUS:
        Serial.println("\tResult: Idle");
        break;
      case WL_NO_SSID_AVAIL:
        Serial.println("\tResult: No SSID available");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("\tResult: Scan completed");
        break;
      case WL_CONNECTED:
        Serial.println("\tResult: Connected");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("\tResult: Connect failed");
        break;
      case WL_CONNECTION_LOST:
        Serial.println("\tResult: Connection lost");
        break;
      case WL_DISCONNECTED:
        Serial.println("\tResult: disconnected");
        break;
      default:
        Serial.println("\tResult: unknown return");
    }
  
    delay(10000);
    status = WiFi.status();
  }

  WiFi.setHostname(HOST_NAME);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  WebSerial.msgCallback(onMessage);
  WebSerial.begin(&server);
  server.begin();
}

void listNetworks() {
  // scan for nearby networks:
  Serial.println("** Scan Networks **");
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    Serial.println("Couldn't get a wifi connection");
    while (true);
  }

  // print the list of networks seen:
  Serial.print("number of available networks:");
  Serial.println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") ");
    Serial.print(WiFi.SSID(thisNet));
    Serial.print("\tSignal: ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.print("\tEncryption: ");
    printEncryptionType(WiFi.encryptionType(thisNet));
  }
}

void printEncryptionType(int thisType) {
  // read the encryption type and print out the name:
  switch (thisType) {
    case 5:
      Serial.println("WEP");
      break;
    case 2:
      Serial.println("WPA");
      break;
    case 4:
      Serial.println("WPA2");
      break;
    case 7:
      Serial.println("None");
      break;
    case 8:
      Serial.println("Auto");
      break;
   default:
      Serial.println(thisType);
  }
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
  //timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  configTzTime(time_zone, ntpServer1, ntpServer2);

  startup = time(nullptr);
  Serial.print("Started at: ");
  Serial.println(localtime(&startup));

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    WebSerial.println("No time available (yet)");
    return;
  }
  WebSerial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
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
    WebSerial.print("InfluxDB write failed: ");
    WebSerial.println(client.getLastErrorMessage());
  }
}

void annotate_close_window(const char * reason, float trigger) {
  sensor.clearFields();
  window_state.addField("is_open", false);
  window_state.addField("reason", reason);
  window_state.addField("trigger", trigger);

  if (!client.writePoint(window_state)) {
    WebSerial.print("InfluxDB write failed: ");
    WebSerial.println(client.getLastErrorMessage());
  }
}

void annotate_fan_on(const char * reason, float trigger) {
  sensor.clearFields();
  fan_state.addField("fan_on", true);
  fan_state.addField("reason", reason);
  fan_state.addField("trigger", trigger);
  

  if (!client.writePoint(fan_state)) {
    WebSerial.print("InfluxDB write failed: ");
    WebSerial.println(client.getLastErrorMessage());
  }
}

void annotate_fan_off(const char * reason, float trigger) {
  sensor.clearFields();
  fan_state.addField("fan_on", false);
  fan_state.addField("reason", reason);
  fan_state.addField("trigger", trigger);

  if (!client.writePoint(fan_state)) {
    WebSerial.print("InfluxDB write failed: ");
    WebSerial.println(client.getLastErrorMessage());
  }
}

void init_temp_history() {
  Serial.println("Initializing history");

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
      WebSerial.print("InfluxDB write failed: ");
      WebSerial.println(client.getLastErrorMessage());
    }
  }

  WebSerial.print("Logging: temp: ");
  WebSerial.print(temp);
  WebSerial.print(" / humidity: ");
  WebSerial.println(humidity);
}

// Return the detla between the temp "minutes" minutes ago and now
float temp_delta_over(uint16_t minutes) {
  uint16_t seconds = minutes * 60;
  uint16_t start_period = int(seconds/COLLECTION_PERIOD_S);
  float start_period_temp = temp_history[start_period];
  float current_period_temp =  temp_history[0];

  return current_period_temp - start_period_temp;
}

bool need_fan_on() {
  // If the fan is already on, don't do anything
  if (fan_is_on) {
    return false;
  }

  // If the window is not open, don't turn the fan on
  if (!window_open) {
    return false;
  }

  if (temp >= ALWAYS_FAN_TEMP_F) {
    annotate_fan_on("absolute", temp);
    return true;
  }

  return false;
}

bool need_fan_off() {
  // If the fan is already off, don't do anything
  if (!fan_is_on) {
    return false;
  }
  
  if (temp <= ALWAYS_NO_FAN_TEMP_F) {
    annotate_fan_off("absolute", temp);
    return true;
  }

  return false;
}

void fan_on() {
  WebSerial.println("Fan on");
  digitalWrite(FAN_CONTROL_PIN, HIGH);
  fan_is_on = true;
}

void fan_off() {
  WebSerial.println("Fan off");
  digitalWrite(FAN_CONTROL_PIN, LOW);
  fan_is_on = false;
}

bool need_window_opened() {
  // Don't continue if window is already open
  if (window_open) {
    return false;
  }

  // Open unconditionally if temp is high enough
  if (temp >= ALWAYS_OPEN_TEMP_F) {
    WebSerial.print("Opening window (absolute): ");
    WebSerial.print(temp);
    WebSerial.print("F >= ");
    WebSerial.print(ALWAYS_OPEN_TEMP_F);
    WebSerial.println("F");

    annotate_open_window("absolute", temp);
    return true;
  }

  // Open depending on how quickly temp is rising
  if (temp >= TARGET_TEMP_F) {
    float temp_delta = temp_delta_over(RAPID_RISE_TEMP_MIN);
    if (temp_delta > RAPID_RISE_TEMP_DELTA) {
      WebSerial.print("Rapid rise minutes: ");
      WebSerial.println(RAPID_RISE_TEMP_MIN);
      
      WebSerial.print("Opening window (conditional): ");
      WebSerial.print(temp_delta);
      WebSerial.print("F > ");
      WebSerial.print(RAPID_RISE_TEMP_DELTA);
      WebSerial.println("F");
      
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
    WebSerial.print("Closing window (absolute): ");
    WebSerial.print(temp);
    WebSerial.print("F < ");
    WebSerial.print(ALWAYS_CLOSE_TEMP_F);
    WebSerial.println("F");

    annotate_close_window("absolute", temp);
    return true;
  }

  // After dropping below a threshold temp, check to see if we've been consistently falling before closing
  if (temp < DROPPING_TEMP_F) {
    float temp_delta = temp_delta_over(FALLING_TEMP_MIN);
    // We've fallen more than gained over the last FALLING_TEMP_MIN minutes
    if (temp_delta < 0) {

    //long open_delta = millis() - last_open_time_ms;
    //if (open_delta > DROPPING_TIME_SINCE_OPEN_MS) {
      WebSerial.print("Closing window (conditional): ");
      WebSerial.print(temp);
      WebSerial.print("F < ");
      WebSerial.print(DROPPING_TEMP_F);
      WebSerial.print("F AND temp delta = ");
      //WebSerial.print((double) open_delta);
      //WebSerial.print("ms > ");
      //WebSerial.print(DROPPING_TIME_SINCE_OPEN_MS);
      //WebSerial.println("ms");
      WebSerial.print((double) temp_delta);

      annotate_close_window("conditional", temp_delta);
      return true;
    }
  }

  return false;
}

void open_window() {
  WebSerial.println("Opening window ...");
  digitalWrite(WINDOW_OPEN_PIN, HIGH);
  delay(WINDOW_MOVE_TIME_MS);
  digitalWrite(WINDOW_OPEN_PIN, LOW);

  window_open = true;
  last_open_time_ms = millis();
}

void close_window() {
  // Failsafe; the fan should never be on when the windows are closed.  Its not terrible, but there's no reason for it.
  WebSerial.println("Turning off fan (if on)");
  fan_off();
  
  WebSerial.println("Closing window ...");
  digitalWrite(WINDOW_CLOSE_PIN, HIGH);
  delay(WINDOW_MOVE_TIME_MS);
  digitalWrite(WINDOW_CLOSE_PIN, LOW);

  window_open = false;
}

void onMessage(uint8_t *data, size_t len) {
  String cmd = "";

  for (int i=0; i < len; i++){
      cmd += (char) data[i];
  }

  // Trigger boolean and then do the action in the main loop The reason is this handler
  // seems to have issues if you spend to much time here
  if (cmd == "status") {
    trigger_status = true;
  } else if (cmd == "open") {
    trigger_open = true;
  } else if (cmd == "close") {
    trigger_close = true;
  } else if (cmd == "dump") {
    trigger_dump = true;
  } else if (cmd == "delta") {
    trigger_delta = true;
  } else if (cmd == "fan on") {
    trigger_fan_on = true;
  } else if (cmd == "fan off") {
    trigger_fan_off = true;
  }
}

void print_status() {
  WebSerial.print("Up since: ");
  printLocalTime();
  WebSerial.print("Current: temp=");
  WebSerial.print(temp);
  WebSerial.print("F / humidity=");
  WebSerial.print(humidity);
  WebSerial.println("%");
}

void print_temp_history() {
  for (int i = 0; i < 8; i++) {
    WebSerial.print("history[");
    WebSerial.print(i);
    WebSerial.print("] = ");
    WebSerial.println((float) temp_history[i]);
  }
}

void print_delta() {
  WebSerial.print("Rising temp delta (");
  WebSerial.print(RAPID_RISE_TEMP_MIN);
  WebSerial.print("min): ");
  WebSerial.println(temp_delta_over(RAPID_RISE_TEMP_MIN));

  WebSerial.print("Falling temp delta (");
  WebSerial.print(FALLING_TEMP_MIN);
  WebSerial.print("min): ");
  WebSerial.println(temp_delta_over(FALLING_TEMP_MIN));
}

void handleWebSerialCommands() {
  if (trigger_status) {
    print_status();
    trigger_status = false;
  }

  if (trigger_dump) {
    print_temp_history();
    trigger_dump = false;
  }

  if (trigger_delta) {
    print_delta();
    trigger_delta = false;
  }

  if (trigger_fan_on) {
    fan_on();
    trigger_fan_on = false;
  }

  if (trigger_fan_off) {
    fan_off();
    trigger_fan_off = false;
  }

  // Handle manual (via serial console) open requests
  if (trigger_open) {
    open_window();
    trigger_open = false;
  }
  // Handle manual (via serial console) close requests
  if (trigger_close) {
    close_window();
    trigger_close = false;
  }
}

void loop() {
  collect_new_reading();
  add_temp_history();
  write_reading_to_influx();

  // Initialize the history on first reading
  if (history_init) {
    init_temp_history();
    history_init = false;
  }

  if (need_fan_on()) {
    fan_on();
  } else if (need_fan_off()) {
    fan_off();
  }

  // It takes a while to open the window, so factor that into our wait time
  long wait_ms = COLLECTION_PERIOD_MS;
  if (need_window_opened()) {
    open_window();
    wait_ms -= WINDOW_MOVE_TIME_MS;
  } else if (need_window_closed()) {
    close_window();
    wait_ms -= WINDOW_MOVE_TIME_MS;
  }

  WebSerial.print("Waiting for ");
  WebSerial.print((float) (wait_ms/1000));
  WebSerial.println("s");

  // Determine when we're done waiting
  long end_wait_ms = millis() + wait_ms;
  while (millis() < end_wait_ms) {
    handleWebSerialCommands();
    delay(POLL_DELAY_MS);
  }
}
