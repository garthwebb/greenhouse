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

#include "Logger.h"
#include "FanControl.h"
#include "MistControl.h"
#include "WindowControl.h"
#include "TempHumiditySensor.h"
#include "TempSensor.h"
#include "LightSensor.h"
#include "ClimateControl.h"
#include "AdminAccess.h"
#include "WirelessControl.h"

#include "Telemetry.h"

//----------------------------------------------------
// Globals

FanControl *FAN;
MistControl *MIST;
WindowControl *WINDOW;
TempHumiditySensor *LEAD_SENSOR;
SensorHandler *LOG_SENSORS;
LightSensor *LIGHT_SENSOR;
Telemetry *TELEMETRY;
Logger *LOGGER;
ClimateControl *CLIMATE;
AdminAccess *ADMIN;

void current_readings() {
	WebSerial.println("Current Readings:");
	Serial.println("Current Readings:");

	WebSerial.printf("LEAD Sensor: temp: %.2fF, humidity: %.2f%%\n", LEAD_SENSOR->current_temperature(), LEAD_SENSOR->current_humidity());
	Serial.printf("LEAD Sensor: temp: %.2fF, humidity: %.2f%%\n", LEAD_SENSOR->current_temperature(), LEAD_SENSOR->current_humidity());

	LOG_SENSORS->load_readings();
	for (auto &sensor : LOG_SENSORS->sensors) {
		WebSerial.printf("Sensor [%s]: temp: %.2fC\n", sensor.get_address_string().c_str(), sensor.temp);
		Serial.printf("Sensor [%s]: temp: %.2fC\n", sensor.get_address_string().c_str(), sensor.temp);
	}

	LIGHT_SENSOR->read();
	WebSerial.printf("Light Sensor: full=%d, ir=%d, visible=%d, lux=%.2f lux\n",
						LIGHT_SENSOR->getFullLuminosity(), LIGHT_SENSOR->getIR(), LIGHT_SENSOR->getVisible(), LIGHT_SENSOR->getLux());
	Serial.printf("Light Sensor: full=%d, ir=%d, visible=%d, lux=%.2f lux\n",
						LIGHT_SENSOR->getFullLuminosity(), LIGHT_SENSOR->getIR(), LIGHT_SENSOR->getVisible(), LIGHT_SENSOR->getLux());

}

void register_admin_commands() {
    ADMIN->register_command("fan on", []() { FAN->turn_on(); } );
    ADMIN->register_command("fan off", []() { FAN->turn_off(); } );
    ADMIN->register_command("mist on", []() { MIST->turn_on(); } );
    ADMIN->register_command("mist off", []() { MIST->turn_off(); } );
    ADMIN->register_command("open", []() { WINDOW->open(); } );
    ADMIN->register_command("close", []() { WINDOW->close(); } );
	ADMIN->register_command("current", []() { current_readings(); } );
	ADMIN->register_command("scan", []() { LOG_SENSORS->scan(); } );
}

void setup() {
    // Start serial communication
    Serial.begin(SERIAL_SPEED);

	// We're defining this because other code uses it, but we are disabling logging in platformio.ini
    LOGGER = new Logger();
	if (LOG_TO_SYSLOG) {
		LOGGER->init(SYSLOG_SERVER, SYSLOG_PORT, HOSTNAME, APP_NAME);
	}

    WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD, HOSTNAME);

    FAN = new FanControl(FAN_CONTROL_PIN);
	MIST = new MistControl(MIST_CONTROL_PIN);
    WINDOW = new WindowControl(WINDOW_OPEN_PIN, WINDOW_CLOSE_PIN);
    LEAD_SENSOR = new TempHumiditySensor(DT22_PIN);
	LOG_SENSORS = new SensorHandler();
	TELEMETRY = new Telemetry(INFLUXDB_URL, TELEMETRY_DB, HOSTNAME);
	LIGHT_SENSOR = new LightSensor();
    CLIMATE = new ClimateControl(LEAD_SENSOR);
    ADMIN = new AdminAccess(FAN, WINDOW, CLIMATE);

	register_admin_commands();

	if (LOG_TELEMETRY) {
		TELEMETRY->enable();
	} else {
		TELEMETRY->disable();
	}
}

void loop() {
    // Make sure we still have a wifi connection
    WirelessControl::monitor();

    // While we are between collection periods, check for webserial commands and monitor the window
    ADMIN->handle_commands();
}
