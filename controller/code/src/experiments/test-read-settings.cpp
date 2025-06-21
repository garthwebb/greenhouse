//----------------------------------------------------
// Includes

#include <Arduino.h>
#include <WiFi.h>
#include <monitor.h>
#include <time.h>
#include <string>
#include <HTTPClient.h>

#include "WirelessControl.h"
#include "Logger.h"
#include "ExternalSettings.h"

#define SETTINGS_HOST "tigerbackup.local"
#define SETTINGS_PORT 80
#define SETTINGS_PATH "/greenhouse/settings.json"
#define SETTINGS_URL String(SETTINGS_HOST) + String(SETTINGS_PATH)

//----------------------------------------------------
// Globals

ExternalSettings *SETTINGS;

void setup() {
	Serial.begin(SERIAL_SPEED);
    WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD, HOSTNAME);

	SETTINGS = new ExternalSettings(SETTINGS_HOST, SETTINGS_PORT, SETTINGS_PATH);
}

void loop () {
	SETTINGS->monitor();

	Serial.println("Wait five seconds");
	delay(5000);
}
