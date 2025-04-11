#ifndef MONITOR_H
#define MONITOR_H

#include "wifi-info.h"

//----------------------------------------------------
// Defines

#define HOSTNAME "greenhouse-monitor"

#define DEVICE "ESP32"

#define SERIAL_SPEED 115200

//---- GPIO Pins
#define DT22_PIN 13
#define WINDOW_OPEN_PIN 33
#define WINDOW_CLOSE_PIN 32
#define FAN_CONTROL_PIN 25

#define ENABLE_ADMIN_ACCESS true

//-- Influx DB
//#define INFLUXDB_URL "http://tiger-pi.local:8086"
#define INFLUXDB_URL "http://192.168.3.88:8086"
#define INFLUXDB_DB "greenhouse"
#define TELEMETRY_DB "telemetry"
// Quickly enable/disable infux logging
#define LOG_TO_INFLUX true

// Temp we want the greenhouse
#define TARGET_TEMP_F 70

// Temp above which we always want the windows open
#define ALWAYS_OPEN_TEMP_F (TARGET_TEMP_F + 15)
// Temp below witch we always want the windows closed
#define ALWAYS_CLOSE_TEMP_F (TARGET_TEMP_F - 5)

#define ALWAYS_FAN_TEMP_F (TARGET_TEMP_F + 10)
#define ALWAYS_NO_FAN_TEMP_F (TARGET_TEMP_F + 5)

// Time in seconds between collecting data
#define COLLECTION_PERIOD_S (1 * 60)
#define COLLECTION_PERIOD_MS (1000 * COLLECTION_PERIOD_S)

// Send a heartbeat every 10 minutes to the logger
#define HEARTBEAT_PERIOD_S (10 * 60)
#define HEARTBEAT_PERIOD_MS (1000 * HEARTBEAT_PERIOD_S)

// How often to poll for webserial actions
#define POLL_DELAY_MS 500

// Set a short watchdog timer
#define WDT_TIMEOUT_S 10

#endif