//----------------------------------------------------
// Includes

#include <Arduino.h>
#include <WiFi.h>
#include <monitor.h>
#include <Wire.h>
#include <time.h>
#include <string>
#include <WebSerial.h>
#include <ArduinoOTA.h>
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

bool ota_listener_enabled = OTA_LISTENER_ENABLED_BY_DEFAULT;
bool ota_in_progress = false;
unsigned long ota_start_ms = 0;
int ota_last_progress_pct = -1;

String ota_status_message() {
    if (!ota_listener_enabled) {
        return "disabled";
    }

    if (!WirelessControl::is_connected) {
        return "enabled, waiting for wifi";
    }

    if (ota_in_progress) {
        return "in progress";
    }

    return "enabled, idle";
}

//----------------------------------------------------
// Functions

void register_admin_commands() {
    ADMIN->register_command("status", []() { ADMIN->print_status(); } );
    ADMIN->register_command("delta", []() { ADMIN->print_delta(); } );
    ADMIN->register_command("ota status", []() {
        String status = ota_status_message();
        WebSerial.println("OTA status: " + status);
        Serial.println("OTA status: " + status);
    });
    ADMIN->register_command("ota enable", []() {
        ota_listener_enabled = true;
        WebSerial.println("OTA listener enabled");
        LOGGER->log("OTA listener enabled");
    });
    ADMIN->register_command("ota start", []() {
        ota_listener_enabled = true;
        WebSerial.println("OTA listener enabled: push update with PlatformIO espota now");
        LOGGER->log("OTA listener enabled via ota start command");
    });
    ADMIN->register_command("ota disable", []() {
        if (ota_in_progress) {
            WebSerial.println("OTA disable rejected: update already in progress");
            LOGGER->log_error("OTA disable rejected: update already in progress");
            return;
        }

        ota_listener_enabled = false;
        WebSerial.println("OTA listener disabled");
        LOGGER->log("OTA listener disabled");
    });
    ADMIN->register_command("fan on", []() { CONTROLS->fan->turn_on(); } );
    ADMIN->register_command("fan off", []() { CONTROLS->fan->turn_off(); } );
    ADMIN->register_command("open", []() { CONTROLS->window->open(); } );
    ADMIN->register_command("close", []() { CONTROLS->window->close(); } );
    ADMIN->register_command("enable logging", []() { CLIMATE->enable_influx_collection(INFLUX); });
    ADMIN->register_command("disable logging", []() { CLIMATE->disable_influx_collection(); });
    ADMIN->register_command("help", []() { ADMIN->print_help(); });
}

void setup_ota() {
#if ENABLE_OTA_UPDATE
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPort(OTA_PORT);

    if (String(OTA_PASSWORD).length() > 0) {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
        if (ota_in_progress) {
            LOGGER->log_error("OTA start ignored: update already in progress");
            return;
        }

        ota_in_progress = true;
        ota_start_ms = millis();
        ota_last_progress_pct = -1;
        LOGGER->log("OTA update started");
        Serial.println("OTA update started");
    });

    ArduinoOTA.onEnd([]() {
        ota_in_progress = false;
        LOGGER->log("OTA update completed in " + String((millis() - ota_start_ms) / 1000.0f, 2) + "s");
        Serial.println("OTA update completed");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        int progress_pct = (progress * 100U) / total;
        if (progress_pct != ota_last_progress_pct && (progress_pct % 10 == 0 || progress_pct == 100)) {
            ota_last_progress_pct = progress_pct;
            LOGGER->log("OTA update progress: " + String(progress_pct) + "%");
        }

        // Feed watchdog during transfer to avoid WDT reset on slower uploads.
        esp_task_wdt_reset();
    });

    ArduinoOTA.onError([](ota_error_t error) {
        ota_in_progress = false;
        LOGGER->log_error("OTA update failed with error code: " + String(static_cast<int>(error)));
        Serial.printf("OTA error [%u]\n", error);
    });

    ArduinoOTA.begin();
    LOGGER->log("OTA service initialized on port " + String(OTA_PORT));
#endif
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

    SENSORS->temphumid = new TempHumiditySensor();
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

    setup_ota();

    if (ota_listener_enabled) {
        LOGGER->log("OTA listener starts enabled");
    } else {
        LOGGER->log("OTA listener starts disabled (manual enable required)");
    }

    register_admin_commands();

    // Initialize the Watchdog Timer
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    // Add the current task to the Watchdog Timer, (the behavior when the task handler == NULL)
    esp_task_wdt_add(NULL);

    WebSerial.println("===================== GREENHOUSE MONITOR STARTED =====================");
}

// Make sure we start with an immediate reading

unsigned long last_heartbeat_ms = millis() - HEARTBEAT_PERIOD_MS;
unsigned long last_collection_ms = millis() - COLLECTION_PERIOD_MS;
unsigned long last_monitor_ms = millis() - MONITOR_PERIOD_MS;

void loop() {
    // Make sure we still have a wifi connection
    WirelessControl::monitor();

#if ENABLE_OTA_UPDATE
    // Handle OTA only when command-enabled and WiFi is healthy.
    if (ota_listener_enabled && WirelessControl::is_connected) {
        ArduinoOTA.handle();
    }
#endif

    // Keep runtime responsive and watchdog-safe while OTA is active.
    if (ota_in_progress) {
        ADMIN->handle_commands();
        SENSORS->temphumid->clear_cache();
        esp_task_wdt_reset();
        return;
    }

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
        float temp = temperatureRead();
        LOGGER->log("Greenhouse monitor running: last metrics collection took place " + String(float(millis() - last_collection_ms) / 1000.0f, 2) + "s ago");
		last_heartbeat_ms = millis();
	}

    // Not great.  Clear the per loop cache, in case we happened to have read a value this time through
    SENSORS->temphumid->clear_cache();
    esp_task_wdt_reset();
}
