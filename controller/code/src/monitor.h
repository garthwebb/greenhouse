#ifndef MONITOR_H
#define MONITOR_H

#include "wifi-info.h"

//----------------------------------------------------
// Defines

#ifndef HOSTNAME
#define HOSTNAME "greenhouse-esp32"
#endif

#define DEVICE "ESP32"

#define SERIAL_SPEED 115200

//---- GPIO Pins : Set defaults but defer to the platformio file for specific boards for pin numbers

// Main temp+humidity sensor
#ifndef DT22_PIN
#define DT22_PIN 13
#endif

#ifndef WINDOW_OPEN_PIN
#define WINDOW_OPEN_PIN 33
#endif

#ifndef WINDOW_CLOSE_PIN
#define WINDOW_CLOSE_PIN 32
#endif

#ifndef FAN_CONTROL_PIN
#define FAN_CONTROL_PIN 25
#endif

#ifndef MIST_CONTROL_PIN
#define MIST_CONTROL_PIN 25
#endif

//---- Temperature Sensors
#ifndef ONE_WIRE_BUS_PIN
#define ONE_WIRE_BUS_PIN D8
#endif

#define ONE_WIRE_SETTLE_MILLIS 500

//---- How to access external settings
#ifndef SETTINGS_HOST
#define SETTINGS_HOST "tigerbackup.local"
#endif
#ifndef SETTINGS_PORT
#define SETTINGS_PORT 80
#endif
#ifndef SETTINGS_PATH
#define SETTINGS_PATH "/greenhouse/settings.json"
#endif

// Syslog server connection info
#define SYSLOG_SERVER "tigerbackup.local"
#define SYSLOG_PORT 514
#define APP_NAME "env-control"

#define ENABLE_ADMIN_ACCESS true

//-- Influx DB
//#define INFLUXDB_URL "http://tiger-pi.local:8086"
#define INFLUXDB_URL "http://192.168.3.88:8086"
#define INFLUXDB_DB "greenhouse"
#define TELEMETRY_DB "telemetry"

// Quickly enable/disable infux logging.  Can be controlled via platformio.ini
#ifndef LOG_TO_INFLUX
#define LOG_TO_INFLUX true
#endif

// Quickly enable/disable telemetry logging via syslog.  Can be controlled via platformio.ini
#ifndef LOG_TELEMETRY
#define LOG_TELEMETRY true
#endif

// Whether to log to a central syslog server
#ifndef LOG_TO_SYSLOG
#define LOG_TO_SYSLOG true
#endif

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

// Set a short watchdog timer
#define WDT_TIMEOUT_S 10

#endif