#include <Telemetry.h>
#include "WirelessControl.h"

extern Logger *LOGGER;

Telemetry::Telemetry(const String &url, const String &db) {
    LOGGER->log("Initializing Telemtry logger");
    Serial.println("Initialziing Telemtry logger: ");

    client = new InfluxDBClient(url, db);

    if (client->validateConnection()) {
        Serial.println("\tConnected to Telemetry InfluxDB: " + client->getServerUrl());
        LOGGER->log("Connected to Telemetry InfluxDB at " + client->getServerUrl());
    } else {
        Serial.println("Telemetry InfluxDB connection failed: " + client->getLastErrorMessage());
        LOGGER->log_error("Telemetry InfluxDB connection failed: " + client->getLastErrorMessage());
    }

    if (log_events && WirelessControl::is_connected) {
        LOGGER->log("Logging telemetry to InfluxDB is enabled");
    }
}

bool Telemetry::report() {
	Serial.println("Reporting telemetry data");

	float chipTemp = temperatureRead();
	float voltage = 3.3; // Replace with a constant or an appropriate function to retrieve the voltage
	float freeHeap = esp_get_free_heap_size();
	float minFreeHeap = esp_get_minimum_free_heap_size();
	float taskCount = uxTaskGetNumberOfTasks();
	float taskHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
	unsigned long uptime = millis() / 1000; // Uptime in seconds

	int8_t rssi = WiFi.RSSI();
	String mac = WiFi.macAddress();
	String ssid = WiFi.SSID();
	String device = DEVICE_HOSTNAME;

	esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

	String model = chip_model_to_string(chip_info.model);
	String revision = String(chip_info.revision);
	String cores = String(chip_info.cores);

	Point telemetry("host_info");
	telemetry.addTag("device", device);
	telemetry.addTag("SSID", ssid);
	telemetry.addTag("mac", mac);
	telemetry.addTag("model", model);
	telemetry.addTag("revision", revision);
	telemetry.addTag("cores", cores);
	telemetry.addField("chip_temp", chipTemp);
	telemetry.addField("vdd33", voltage);
	telemetry.addField("free_heap", freeHeap);
	telemetry.addField("min_free_heap", minFreeHeap);
	telemetry.addField("task_count", taskCount);
	telemetry.addField("task_high_water_mark", taskHighWaterMark);
	telemetry.addField("rssi", rssi);
	telemetry.addField("uptime", uptime);

	if (!log_events || !WirelessControl::is_connected) {
		return true;
	}

	if (!client->writePoint(telemetry)) {
		LOGGER->log_error("Failed to add telemetry: " + client->getLastErrorMessage());
		return false;
	}

	return true;
}

String Telemetry::chip_model_to_string(int model) {
	switch (model) {
		case CHIP_ESP32:
			return "ESP32";
		case CHIP_ESP32S2:
			return "ESP32-S2";
		case CHIP_ESP32S3:
			return "ESP32-S3";
		case CHIP_ESP32C3:
			return "ESP32-C3";
		case CHIP_ESP32H2:
			return "ESP32-H2";
		default:
			return "Unknown";
	}
}

void Telemetry::enable() {
    log_events = true;
}

void Telemetry::disable() {
    log_events = false;
}