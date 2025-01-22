//----------------------------------------------------
// Includes

#include <Arduino.h>
#include <string>
#include <WiFi.h>
#include <WirelessControl.h>

//----------------------------------------------------
// Defines

#define SERIAL_SPEED 115200

// Put credentials in separate file not tracked in git
#include "wifi-info.h"

// Syslog server connection info
#define SYSLOG_SERVER "tigerbackup.local"
#define SYSLOG_IP IPAddress(192, 168, 3, 204)
#define SYSLOG_PORT 514
#define DEVICE_HOSTNAME "greenhouse-esp32"
#define APP_NAME "env-control"

//----------------------------------------------------
// Globals

WiFiUDP udpClient;
Syslog LOGGER(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);

//----------------------------------------------------
// Functions

void setup() {
  // Start serial communication
  Serial.begin(SERIAL_SPEED);

  WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connected to " + WiFi.SSID() + " as " + WiFi.localIP().toString() + " ... letting the connection cook 5s");
  delay(5000);
  Serial.println("Cooking done, calling monitor");
  WirelessControl::monitor();
  Serial.println("Monitor done");


	//WiFiUDP udpClient;
	//LOGGER = new Syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);


  Serial.println("Connected to " + WiFi.SSID() + " as " + WiFi.localIP().toString());
  try {
	  LOGGER.log(LOG_INFO, "Connected to " + WiFi.SSID() + " as " + WiFi.localIP().toString());
  } catch (const std::exception &e) {
	  Serial.println("Failed to log to syslog: " + String(e.what()));
  }
}

void loop() {
  long loop_start_ms = millis();

  WirelessControl::monitor();

  LOGGER.log(LOG_DEBUG, "Testing loop. Sleeping for 2 seconds");
  delay(2000);
}
