//----------------------------------------------------
// Includes

#include <Arduino.h>
#include <WiFi.h>
#include <monitor.h>
#include <Wire.h>
#include <time.h>
#include <string>
#include <WebSerial.h>

#include "FanControl.h"
#include "WindowControl.h"
#include "SensorControl.h"
#include "ClimateControl.h"
#include "WirelessControl.h"
#include "AdminAccess.h"
#include "TimeHandler.h"
#include "InfluxDBHandler.h"
#include "Logger.h"

//----------------------------------------------------
// Globals

FanControl *FAN;
WindowControl *WINDOW;
SensorControl *SENSOR;
ClimateControl *CLIMATE;
AdminAccess *ADMIN;
InfluxDBHandler *INFLUX;
Logger *LOGGER;

long last_heartbeat_ms = millis();

//----------------------------------------------------
// Functions

void record_new_reading() {
    INFLUX->add_reading(SENSOR->current_temperature(), SENSOR->current_humidity());
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
    if (CLIMATE->over_max_temp()) {
        LOGGER->log_info("Opening window (absolute): " + String(SENSOR->current_temperature()) + "F >= " + String(ALWAYS_OPEN_TEMP_F) + "F");
        INFLUX->annotate_window_open("absolute", SENSOR->current_temperature());
        return true;
    }

    // Open depending on how quickly temp is rising
    if (CLIMATE->in_rapid_rise()) {
        LOGGER->log_info("Opening window (rapid rise): " + String(CLIMATE->rise_delta()) + "F > " + String(RAPID_RISE_TEMP_DELTA) + "F");
        INFLUX->annotate_window_open("conditional", CLIMATE->rise_delta());
        return true;
    }

    return false;
}

bool need_window_closed() {
    // Don't continue if window is already closed
    if (!WINDOW->is_open) {
        return false;
    }

    // Close unconditionally if temp is low enough
    if (CLIMATE->under_min_temp()) {
        LOGGER->log_info("Closing window (absolute): " + String(SENSOR->current_temperature()) + "F < " + String(ALWAYS_CLOSE_TEMP_F) + "F");
        INFLUX->annotate_window_closed("absolute", SENSOR->current_temperature());
        return true;
    }

    // After dropping below a threshold temp, check to see if we've been consistently falling before closing
    if (CLIMATE->in_rapid_fall()) {
        LOGGER->log_info("Closing window (conditional): " + String(SENSOR->current_temperature()) + "F < "
                          + String(DROPPING_TEMP_F) + "F AND temp delta = " + String((double) CLIMATE->fall_delta()));

        INFLUX->annotate_window_closed("conditional", CLIMATE->fall_delta());
        return true;
    }

    return false;
}

void register_admin_commands() {
    ADMIN->register_command("status", []() { ADMIN->print_status(); } );
    ADMIN->register_command("history", []() { ADMIN->print_temp_history(); } );
    ADMIN->register_command("delta", []() { ADMIN->print_delta(); } );
    ADMIN->register_command("fan on", []() { FAN->turn_on(); } );
    ADMIN->register_command("fan off", []() { FAN->turn_off(); } );
    ADMIN->register_command("open", []() { WINDOW->open(); } );
    ADMIN->register_command("close", []() { WINDOW->close(); } );
    ADMIN->register_command("enable logging", []() { INFLUX->enable_logging(); });
    ADMIN->register_command("disable logging", []() { INFLUX->disable_logging(); });
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

void setup() {
    // Start serial communication
    Serial.begin(SERIAL_SPEED);

	// Initialize the logger so WirelessControl can use it, but LOGGER should not be used
	// until after the init_wifi() returns
    LOGGER = new Logger();

    WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD);

	LOGGER->log("Greenhouse monitor power cycled, starting up ...");

    TimeHandler::init_ntp();

    LOGGER->log("Synced time with NTP");

    FAN = new FanControl(FAN_CONTROL_PIN);
    WINDOW = new WindowControl(WINDOW_OPEN_PIN, WINDOW_CLOSE_PIN);
    SENSOR = new SensorControl(DT22_PIN);
    CLIMATE = new ClimateControl(SENSOR);
    ADMIN = new AdminAccess(FAN, WINDOW, CLIMATE);
    INFLUX = new InfluxDBHandler(INFLUXDB_URL, INFLUXDB_DB, DEVICE, WIFI_SSID);

    if (LOG_TO_INFLUX) {
        INFLUX->enable_logging();
    } else {
        INFLUX->disable_logging();
    }

    register_admin_commands();
  }

void loop() {
    long loop_start_ms = millis();
    
    // Make sure we still have a wifi connection
    WirelessControl::monitor();

    // Send a new reading to InfluxDB
    record_new_reading();

    // Make decisions on fan and window control based on current temperature and humidity
    CLIMATE->monitor();

    handle_fan_control();
    handle_window_control();

    // Determine when we're done waiting
    while (millis() < loop_start_ms + COLLECTION_PERIOD_MS) {
        // While we are between collection periods, check for webserial commands and monitor the window
        ADMIN->handle_commands();
        WINDOW->monitor();

        delay(POLL_DELAY_MS);
    }

	if (millis() > last_heartbeat_ms + HEARTBEAT_PERIOD_MS) {
		LOGGER->log("Greenhouse monitor running");
		last_heartbeat_ms = millis();
	}
}
