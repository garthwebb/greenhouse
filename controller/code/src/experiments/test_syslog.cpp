//----------------------------------------------------
// Includes

#include <Arduino.h>
#include <WiFi.h>
#include <string>

#include "monitor.h"
#include "Logger.h"
#include "WirelessControl.h"
#include "AdminAccess.h"

//----------------------------------------------------
// Defines

#define SERIAL_SPEED 115200

//----------------------------------------------------
// Globals

Logger *LOGGER;

//----------------------------------------------------
// Functions

void setup() {
  // Start serial communication
  Serial.begin(SERIAL_SPEED);

  WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connected to " + WiFi.SSID() + " as " + WiFi.localIP().toString());

  LOGGER = new Logger();
  LOGGER->log("Connected to " + WiFi.SSID() + " as " + WiFi.localIP().toString());
}

void loop() {
  long loop_start_ms = millis();

  WirelessControl::monitor();

  LOGGER->log_debug("Testing loop. Sleeping for 5 seconds");
  delay(5000);
  LOGGER->log("Done sleeping");
}
