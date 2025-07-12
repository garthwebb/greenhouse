//----------------------------------------------------
// Includes

#include <Arduino.h>
#include <WiFi.h>
#include <monitor.h>
#include <Wire.h>
#include <time.h>
#include <string>
#include <WebSerial.h>
#include <esp_task_wdt.h>
#include "esp_system.h"

#include <Logger.h>

#include "ClimateControl.h"
#include "WirelessControl.h"
#include "AdminAccess.h"
#include "TimeHandler.h"
#include "InfluxDBHandler.h"
#include "Telemetry.h"
#include "ExternalSettings.h"

//----------------------------------------------------
// Globals


struct ControlObjects *CONTROLS = new ControlObjects();
struct SensorObjects *SENSORS = new SensorObjects();


ClimateControl *CLIMATE = nullptr;
AdminAccess *ADMIN = nullptr;
InfluxDBHandler *INFLUX = nullptr;
Telemetry *TELEMETRY = nullptr;
Logger *LOGGER = nullptr;
ExternalSettings *SETTINGS = nullptr;

long last_heartbeat_ms = millis();

//----------------------------------------------------
// Functions

void register_admin_commands() {
    ADMIN->register_command("status", []() { ADMIN->print_status(); } );
    ADMIN->register_command("delta", []() { ADMIN->print_delta(); } );
    ADMIN->register_command("fan on", []() { CONTROLS->fan->turn_on(); } );
    ADMIN->register_command("fan off", []() { CONTROLS->fan->turn_off(); } );
    ADMIN->register_command("open", []() { CONTROLS->window->open(); } );
    ADMIN->register_command("close", []() { CONTROLS->window->close(); } );
    ADMIN->register_command("enable logging", []() { CLIMATE->enable_influx_collection(INFLUX); });
    ADMIN->register_command("disable logging", []() { CLIMATE->disable_influx_collection(); });
    ADMIN->register_command("help", []() { ADMIN->print_help(); });
}

void check_for_reset() {
    esp_reset_reason_t reason = esp_reset_reason();

    // Log the reset reason, or nothing if no reset was detected
    switch(reason) {
        case ESP_RST_UNKNOWN:
            LOGGER->log_error("Reset reason: Unknown");
            break;
        case ESP_RST_POWERON:
            LOGGER->log_error("Reset reason: Power on");
            break;
        case ESP_RST_EXT:
            LOGGER->log_error("Reset reason: External pin");
            break;
        case ESP_RST_SW:    
            LOGGER->log_error("Reset reason: Software reset");
            break;
        case ESP_RST_PANIC:
            LOGGER->log_error("Reset reason: Panic");
            break;
        case ESP_RST_INT_WDT:
            LOGGER->log_error("Reset reason: Interrupt watchdog");
            break;
        case ESP_RST_TASK_WDT:
            LOGGER->log_error("Reset reason: Task watchdog");
            break;
        case ESP_RST_WDT:
            LOGGER->log_error("Reset reason: Other watchdog");
            break;
        case ESP_RST_DEEPSLEEP:
            LOGGER->log_error("Reset reason: Deep sleep");
            break;
        case ESP_RST_BROWNOUT:
            LOGGER->log_error("Reset reason: Brownout");
            break;
        case ESP_RST_SDIO:
            LOGGER->log_error("Reset reason: SDIO");
            break;
    }
}

uint32_t loop_time_buckets[WDT_TIMEOUT_S];
void clear_loop_buckets() {
    for (int i = 0; i < WDT_TIMEOUT_S; i++) {
        loop_time_buckets[i] = 0;
    }
}   

void setup() {
    // Start serial communication
    Serial.begin(SERIAL_SPEED);

	// Initialize the logger so WirelessControl can use it, but LOGGER should not be used
	// until after the init_wifi() returns
    LOGGER = new Logger();
    LOGGER->init(SYSLOG_SERVER, SYSLOG_PORT, HOSTNAME, APP_NAME);

    WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD, HOSTNAME);
    // Give the WiFi time to connect
    delay(3000); 

    // See if there is a reset reason for the last restart
    check_for_reset();
	LOGGER->log("Greenhouse monitor power cycled, starting up ...");

    TimeHandler::init_ntp();

    SETTINGS = new ExternalSettings(SETTINGS_HOST, SETTINGS_PORT, SETTINGS_PATH);
    SETTINGS->monitor();

    CONTROLS->fan = new FanControl(FAN_CONTROL_PIN);
    CONTROLS->window = new WindowControl(WINDOW_OPEN_PIN, WINDOW_CLOSE_PIN);
    CONTROLS->mist = new MistControl(MIST_CONTROL_PIN);

    SENSORS->temphumid = new TempHumiditySensor(DT22_PIN);
    SENSORS->temp = new SensorHandler();
    SENSORS->light = new LightSensor();

    CLIMATE = new ClimateControl(SETTINGS, SENSORS, CONTROLS);
    ADMIN = new AdminAccess(SETTINGS, CONTROLS, SENSORS, CLIMATE);
    TELEMETRY = new Telemetry(INFLUXDB_URL, TELEMETRY_DB, HOSTNAME);

    if (LOG_TO_INFLUX) {
        INFLUX = new InfluxDBHandler(INFLUXDB_URL, INFLUXDB_DB, DEVICE);
        CLIMATE->enable_influx_collection(INFLUX);
    }

    if (LOG_TELEMETRY) {
        TELEMETRY->enable();
    } else {
        TELEMETRY->disable();
    }

    register_admin_commands();

    clear_loop_buckets();

    // Initialize the Watchdog Timer
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    // Add the current task to the Watchdog Timer, (the behavior when the task handler == NULL)
    esp_task_wdt_add(NULL);
}

// Make sure we start with an immediate reading
unsigned long last_collection_ms = millis() - COLLECTION_PERIOD_MS;
unsigned long last_monitor_ms = millis() - MONITOR_PERIOD_MS;

void loop() {
    unsigned long loop_start_ms = millis();

    // Make sure we still have a wifi connection
    WirelessControl::monitor();

    // Determine when we're done waiting
    if (millis() >= last_collection_ms + COLLECTION_PERIOD_MS) {
        // Check and load new settings if they've changed
        SETTINGS->monitor();

        // Send a new reading to InfluxDB
        CLIMATE->report_metrics();

        // Report back the state of our host device
        TELEMETRY->report_metrics();

        last_collection_ms = millis();
    }

    if (millis() >= last_monitor_ms + MONITOR_PERIOD_MS) {
        // Make decisions on fan and window control based on current temperature and humidity
        CLIMATE->monitor();
        last_monitor_ms = millis();
    }

    // While we are between collection periods, check for webserial commands and monitor the window
    ADMIN->handle_commands();

	if (millis() > last_heartbeat_ms + HEARTBEAT_PERIOD_MS) {
        String loop_time_buckets_str = "";
        for (int i = 0; i < WDT_TIMEOUT_S; i++) {
            loop_time_buckets_str += String(i) + ":" + String(loop_time_buckets[i]) + ", ";
        }
        LOGGER->log_debug("Loop time buckets: " + loop_time_buckets_str);
        clear_loop_buckets();

        float temp = temperatureRead();
		LOGGER->log("Greenhouse monitor running: last collection=" + String(long(millis() - last_collection_ms)) + "ms");
		last_heartbeat_ms = millis();
	}

    int loop_seconds = (millis() - loop_start_ms) / 1000;
    loop_time_buckets[loop_seconds]++;

    esp_task_wdt_reset();
}
