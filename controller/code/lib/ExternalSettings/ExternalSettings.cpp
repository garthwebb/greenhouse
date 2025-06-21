#include "ExternalSettings.h"
#include <HTTPClient.h>

HTTPClient HTTP;

ExternalSettings::ExternalSettings(String host, uint16_t port, String path) {
	HTTP.begin(host, port, path);

	const char* headerNames[] = {"Last-Modified"};
	HTTP.collectHeaders(headerNames, sizeof(headerNames)/sizeof(headerNames[0]));
}

void ExternalSettings::monitor() {
	Serial.println("Fetching external settings ...");

	int httpCode = HTTP.GET();
	String lastModified = HTTP.header("Last-Modified");

	if (_last_modified == lastModified) {
		Serial.println("No changes detected, skipping update.");
		return;
	}

	_last_modified = lastModified;
	Serial.printf("Response %d received, last modified: %s\n", httpCode, lastModified.c_str());

	String response = HTTP.getString();

	// Deserialize the JSON document
  	DeserializationError error = deserializeJson(_doc, response);

	// Test if parsing succeeds
	if (error) {
		Serial.print(F("deserializeJson() failed: "));
		Serial.println(error.f_str());
		return;
	}

	Serial.printf("Target temp: %d\n", get<int>("target_temp", 0));
	Serial.printf("Target humidity: %d\n", get<int>("target_humidity", 0));
	Serial.printf("Version: %f\n", get<float>("version", 0));
	Serial.printf("FooBar: %s\n", get<String>("foo_bar", "42"));
}
