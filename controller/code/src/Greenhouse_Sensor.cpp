//----------------------------------------------------
// Includes

#include <Arduino.h>
#include <Wire.h>
#include <WebSerial.h>
#include "time.h"
#include <FanControl.h>
#include <WindowControl.h>
#include <SensorControl.h>
#include <WirelessControl.h>
#include <AdminAccess.h>
#include <TimeHandler.h>
#include <InfluxDBHandler.h>

//----------------------------------------------------
// Defines

#define DEVICE "ESP32"

#define SERIAL_SPEED 115200

//---- GPIO Pins
#define DT22_PIN 13
#define WINDOW_OPEN_PIN 33
#define WINDOW_CLOSE_PIN 32
#define FAN_CONTROL_PIN 25

// Put credentials in separate file not tracked in git
#include "wifi-info.h"

//-- Influx DB
#define INFLUXDB_URL "http://tiger-pi.local:8086"
#define INFLUXDB_DB "greenhouse"
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

#define TEMP_HISTORY_DEPTH 400

// How often to poll for webserial actions
#define POLL_DELAY_MS 500

//----------------------------------------------------
// Globals

FanControl *FAN;
WindowControl *WINDOW;
SensorControl *SENSOR;
AdminAccess *ADMIN;
InfluxDBHandler *INFLUX;

float temp_history[TEMP_HISTORY_DEPTH];

//----------------------------------------------------
// Functions

void init_temp_history() {
  Serial.print("Initializing history ... ");

  // Initialize all history values to the current temperature 
  for (int i = 0; i < TEMP_HISTORY_DEPTH; i++) {
    temp_history[i] = SENSOR->current_temperature();
  }

  Serial.println("done");
}

void add_temp_history() {
  // Shift all history right, dropping earliest value
  for (int i = TEMP_HISTORY_DEPTH - 1; i > 0; i--) {
    temp_history[i] = temp_history[i-1];
  }
  temp_history[0] = SENSOR->current_temperature();
}

void record_new_reading() {
  if (!INFLUX->add_reading(SENSOR->current_temperature(), SENSOR->current_humidity())) {
    WebSerial.print("[ERROR] Failed to add reading: ");
    WebSerial.print(INFLUX->last_error());
  }
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
  if (FAN->is_on) {
    return false;
  }

  // If the window is not open, don't turn the fan on
  if (!WINDOW->is_open) {
    return false;
  }

  if (SENSOR->current_temperature() >= ALWAYS_FAN_TEMP_F) {
    INFLUX->annotate_fan_on("absolute", SENSOR->current_temperature());
    return true;
  }

  return false;
}

bool need_fan_off() {
    // If the fan is already off, don't do anything
    if (!FAN->is_on) {
        return false;
    }
    
    if (SENSOR->current_temperature() <= ALWAYS_NO_FAN_TEMP_F) {
        INFLUX->annotate_fan_off("absolute", SENSOR->current_temperature());
        return true;
    }

    return false;
}

bool need_window_opened() {
  // Don't continue if window is already open
  if (WINDOW->is_open) {
    return false;
  }

  // Open unconditionally if temp is high enough
  if (SENSOR->current_temperature() >= ALWAYS_OPEN_TEMP_F) {
    WebSerial.print("Opening window (absolute): ");
    WebSerial.print(SENSOR->current_temperature());
    WebSerial.print("F >= ");
    WebSerial.print(ALWAYS_OPEN_TEMP_F);
    WebSerial.println("F");

    INFLUX->annotate_window_open("absolute", SENSOR->current_temperature());
    return true;
  }

  // Open depending on how quickly temp is rising
  if (SENSOR->current_temperature() >= TARGET_TEMP_F) {
    float temp_delta = temp_delta_over(RAPID_RISE_TEMP_MIN);
    if (temp_delta > RAPID_RISE_TEMP_DELTA) {
        WebSerial.print("Rapid rise minutes: ");
        WebSerial.println(RAPID_RISE_TEMP_MIN);
        
        WebSerial.print("Opening window (conditional): ");
        WebSerial.print(temp_delta);
        WebSerial.print("F > ");
        WebSerial.print(RAPID_RISE_TEMP_DELTA);
        WebSerial.println("F");
        
        INFLUX->annotate_window_open("conditional", temp_delta);
        return true;
    }
  }

  return false;
}

bool need_window_closed() {
  // Don't continue if window is already closed
  if (!WINDOW->is_open) {
    return false;
  }

  // Close unconditionally if temp is low enough
  if (SENSOR->current_temperature() < ALWAYS_CLOSE_TEMP_F) {
    WebSerial.print("Closing window (absolute): ");
    WebSerial.print(SENSOR->current_temperature());
    WebSerial.print("F < ");
    WebSerial.print(ALWAYS_CLOSE_TEMP_F);
    WebSerial.println("F");

    INFLUX->annotate_window_closed("absolute", SENSOR->current_temperature());
    return true;
  }

  // After dropping below a threshold temp, check to see if we've been consistently falling before closing
  if (SENSOR->current_temperature() < DROPPING_TEMP_F) {
    float temp_delta = temp_delta_over(FALLING_TEMP_MIN);
    // We've fallen more than gained over the last FALLING_TEMP_MIN minutes
    if (temp_delta < 0) {

    //long open_delta = millis() - last_open_time_ms;
    //if (open_delta > DROPPING_TIME_SINCE_OPEN_MS) {
      WebSerial.print("Closing window (conditional): ");
      WebSerial.print(SENSOR->current_temperature());
      WebSerial.print("F < ");
      WebSerial.print(DROPPING_TEMP_F);
      WebSerial.print("F AND temp delta = ");
      //WebSerial.print((double) open_delta);
      //WebSerial.print("ms > ");
      //WebSerial.print(DROPPING_TIME_SINCE_OPEN_MS);
      //WebSerial.println("ms");
      WebSerial.print((double) temp_delta);

      INFLUX->annotate_window_closed("conditional", temp_delta);
      return true;
    }
  }

  return false;
}

void print_status() {
    char datetime_str[20];
    TimeHandler::localTimeString(datetime_str);

    WebSerial.printf("Up since %s\n", datetime_str);
    WebSerial.printf("- temp: %0.1f F\n", SENSOR->current_temperature());
    WebSerial.printf("- humidity: %0.1f%\n", SENSOR->current_humidity());
    WebSerial.printf("- fan is: %s\n", FAN->is_on ? "ON" : "OFF");
    WebSerial.printf("- window is: %s\n", WINDOW->is_open ? "OPEN" : "CLOSED");
}

void print_temp_history() {
  for (int i = 0; i < 8; i++) {
    WebSerial.print("history[");
    WebSerial.print(i);
    WebSerial.print("] = ");
    WebSerial.println((float) temp_history[i]);
    WebSerial.flush();
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

void register_admin_commands() {
  ADMIN->register_command("status", print_status);
  ADMIN->register_command("history", print_temp_history);
  ADMIN->register_command("delta", print_delta);
  ADMIN->register_command("fan on", []() { FAN->turn_on(); } );
  ADMIN->register_command("fan off", []() { FAN->turn_off(); } );
  ADMIN->register_command("open", []() { WINDOW->open(); } );
  ADMIN->register_command("close", []() { WINDOW->close(); } );
}

void setup() {
  // Start serial communication
  Serial.begin(SERIAL_SPEED);

  WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD);
  TimeHandler::init_ntp();

  FAN = new FanControl(FAN_CONTROL_PIN);
  WINDOW = new WindowControl(WINDOW_OPEN_PIN, WINDOW_CLOSE_PIN);
  SENSOR = new SensorControl(DT22_PIN);
  ADMIN = new AdminAccess();
  INFLUX = new InfluxDBHandler(INFLUXDB_URL, INFLUXDB_DB, DEVICE, WIFI_SSID);

  if (LOG_TO_INFLUX) {
    INFLUX->enable_logging();
  } else {
    INFLUX->disable_logging();
  }

  init_temp_history();
  register_admin_commands();
}

void handle_fan_control() {
  if (need_fan_on()) {
    FAN->turn_on();
  } else if (need_fan_off()) {
    FAN->turn_off();
  }
}

bool handle_window_control() {
  if (need_window_opened()) {
    WINDOW->open();
    return true;
  } else if (need_window_closed()) {
    WINDOW->close();
    return true;
  }

  return false;
}

void loop() {
  long loop_start_ms = millis();

  record_new_reading();
  add_temp_history();

  handle_fan_control();

  if (handle_window_control()) {
    // if we moved the window, adjust our collection period by accounting for the window move time
    loop_start_ms += WINDOW_MOVE_TIME_MS;
  }

  // Determine when we're done waiting
  while (millis() < loop_start_ms + COLLECTION_PERIOD_MS) {
    // While we are between collection periods, check for webserial commands and monitor the window
    ADMIN->handle_commands();
    WINDOW->monitor();

    WebSerial.loop();

    delay(POLL_DELAY_MS);
  }
}
