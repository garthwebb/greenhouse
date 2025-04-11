#include <Logger.h>
#include "WirelessControl.h"

WiFiUDP udpClient;

// Create a new syslog instance with LOG_KERN facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);

bool Logger::log(String msg) {
	return log_info(msg);
}

bool Logger::log_info(String msg) {
	if (!WirelessControl::is_connected) {
		// Don't log if we don't have a connection
		Serial.println("WiFi disconnected: Syslog serial fallback [info]: " + msg);
		return false;
	}
	return syslog.log(LOG_INFO, msg.c_str());
}

bool Logger::log_error(String msg) {
	if (!WirelessControl::is_connected) {
		// Don't log if we don't have a connection
		Serial.println("WiFi disconnected: Syslog serial fallback [error]: " + msg);
		return false;
	}
	return syslog.log(LOG_ERR, msg.c_str());
}

bool Logger::log_debug(String msg) {
	if (!WirelessControl::is_connected) {
		// Don't log if we don't have a connection
		Serial.println("WiFi disconnected: Syslog serial fallback [debug]: " + msg);
		return false;
	}
	return syslog.log(LOG_DEBUG, msg.c_str());
}
