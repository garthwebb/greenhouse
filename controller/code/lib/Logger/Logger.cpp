#include "Logger.h"

WiFiUDP udpClient;

// Create a new syslog instance with LOG_KERN facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);

bool Logger::log(String msg) {
	return log_info(msg);
}

bool Logger::log_info(String msg) {
	return syslog.log(LOG_INFO, msg.c_str());
}

bool Logger::log_error(String msg) {
	return syslog.log(LOG_ERR, msg.c_str());
}

bool Logger::log_debug(String msg) {
	return syslog.log(LOG_DEBUG, msg.c_str());
}
