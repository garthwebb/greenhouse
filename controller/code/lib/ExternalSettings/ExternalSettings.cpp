#include "ExternalSettings.h"
#include <HTTPClient.h>

#define HTTP_TIMEOUT 5000
#define HTTP_CONNECT_TIMEOUT 3000

extern Logger *LOGGER;

HTTPClient HTTP;

ExternalSettings::ExternalSettings(String host, uint16_t port, String path) : _host(host), _port(port), _path(path) {
	_reset_connection();
}

void ExternalSettings::_reset_connection() {
	HTTP.end();
	HTTP.begin(_host, _port, _path);
	HTTP.setTimeout(HTTP_TIMEOUT);
	HTTP.setConnectTimeout(HTTP_CONNECT_TIMEOUT);

	const char* headerNames[] = {"Last-Modified"};
	HTTP.collectHeaders(headerNames, sizeof(headerNames)/sizeof(headerNames[0]));
}

void ExternalSettings::monitor() {
    _reset_connection();

	Serial.println("Fetching external settings ...");

	int httpCode = HTTP.GET();
	
	// Check for timeout or connection errors
	if (httpCode == HTTPC_ERROR_CONNECTION_REFUSED || 
	    httpCode == HTTPC_ERROR_SEND_HEADER_FAILED || 
	    httpCode == HTTPC_ERROR_SEND_PAYLOAD_FAILED || 
	    httpCode == HTTPC_ERROR_NOT_CONNECTED || 
	    httpCode == HTTPC_ERROR_CONNECTION_LOST || 
	    httpCode == HTTPC_ERROR_READ_TIMEOUT) {
		LOGGER->log_error("HTTP request failed/timed out: " + String(httpCode));
		Serial.printf("HTTP request failed/timed out: %d\n", httpCode);
		return;
	}

	String lastModified = HTTP.header("Last-Modified");

	if (_last_modified == lastModified) {
		Serial.println("No changes detected, skipping update.");
		return;
	}

	_last_modified = lastModified;
	Serial.printf("Response %d received, last modified: %s\n", httpCode, lastModified.c_str());
	LOGGER->log("Updated external settings content detected, reloading ...");

	String response = HTTP.getString();

	// Clean up previous readings be for we deserialize the JSON document
	_doc.clear();
  	DeserializationError error = deserializeJson(_doc, response);

	// Test if parsing succeeds
	if (error) {
		LOGGER->log_error("deserializeJson() failed: " + String(error.f_str()));
		Serial.println("deserializeJson() failed: " + String(error.f_str()));
		return;
	}
}
