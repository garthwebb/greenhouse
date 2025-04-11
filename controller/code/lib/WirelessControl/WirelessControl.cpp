#include <WirelessControl.h>
#include <WiFi.h>

extern Logger *LOGGER;

bool WirelessControl::is_connected = false;

void WirelessControl::init_wifi(const char *ssid, const char *passwd) {
    WiFi.setHostname(HOSTNAME);

    listNetworks();

    Serial.println("Connecting to " + String(ssid) + ":");

    WiFi.begin(ssid, passwd);

	while (!connect()) {
        Serial.print("Failed to connect to WiFi, retrying...");
        WiFi.disconnect();
        WiFi.reconnect();
    }

    is_connected = true;
    Serial.println("Connected to " + String(ssid) + " with IP " + WiFi.localIP().toString());
}

bool WirelessControl::connect() {
    int status = WiFi.status();
    while (status != WL_CONNECTED) {
        delay(WIFI_CONNECT_WAIT);
        Serial.print(".");
        status = WiFi.status();
    }

    if (is_error_status(status)) {
        printStatus(status);
        return false;
    }

    return true;
}

void WirelessControl::monitor() {
	uint8_t status = WiFi.status();
	bool was_connected = is_connected;

	// Return immediately if we're still connected
	if (status == WL_CONNECTED) {
		return;
	}

	// If we were previously connected, explicitly disconnect and reconnect
	if (is_connected) {
		is_connected = false;
		Serial.println("Reconnecting to WiFi...");
        WiFi.disconnect();
        WiFi.reconnect();
	}

    // Wait here long enough to get a success or error status.  Don't retry
    // immediately if error, instead, exit and wait for the next call to monitor() 
    while (is_neutral_status(status)) {
        delay(WIFI_CONNECT_WAIT);
        status = WiFi.status();
    }

    if (is_error_status(status)) {
        printStatus(status);
        return;
    }

	// Log that we lost a connection but are now reconnected
	if (was_connected) {
		LOGGER->log_error("WiFi connection was lost");
	}

    LOGGER->log("Connected to " + WiFi.SSID() + " with IP " + WiFi.localIP().toString());
	Serial.println("Connected to " + WiFi.SSID() + " with IP " + WiFi.localIP().toString());
 	is_connected = true;
}

void WirelessControl::listNetworks() {
    // scan for nearby networks:
    Serial.println("** Scanning Networks **");
    int numSsid = WiFi.scanNetworks();
    if (numSsid == -1) {
        Serial.println("Couldn't get a wifi connection");
        return;
    }

    // print the list of networks seen:
    Serial.print("Number of available networks: ");
    Serial.println(numSsid);

    // print the network number and name for each network found:
    for (int thisNet = 0; thisNet < numSsid; thisNet++) {
        Serial.print(thisNet);
        Serial.print(") ");
        Serial.print(WiFi.SSID(thisNet));
        Serial.print("\tSignal: ");
        Serial.print(WiFi.RSSI(thisNet));
        Serial.print(" dBm");
        Serial.print("\tEncryption: ");
        printEncryptionType(WiFi.encryptionType(thisNet));
    }
}

void WirelessControl::printEncryptionType(int thisType) {
    // read the encryption type and print out the name:
    switch (thisType) {
        case WIFI_AUTH_OPEN:             /**< authenticate mode : open */
            Serial.println("Open");
            break;
        case WIFI_AUTH_WEP:              /**< authenticate mode : WEP */
            Serial.println("WEP");
            break;
        case WIFI_AUTH_WPA_PSK:          /**< authenticate mode : WPA_PSK */
            Serial.println("WPA_PSK");
            break;
        case WIFI_AUTH_WPA2_PSK:         /**< authenticate mode : WPA2_PSK */
            Serial.println("WPA2_PSK");
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:     /**< authenticate mode : WPA_WPA2_PSK */
            Serial.println("WPA_WPA2_PSK");
            break;
        case WIFI_AUTH_WPA2_ENTERPRISE:  /**< authenticate mode : WPA2_ENTERPRISE */
            Serial.println("WPA2_ENTERPRISE");
            break;
        case WIFI_AUTH_WPA3_PSK:         /**< authenticate mode : WPA3_PSK */
            Serial.println("WPA3_PSK");
            break;
        case WIFI_AUTH_WPA2_WPA3_PSK:    /**< authenticate mode : WPA2_WPA3_PSK */
            Serial.println("WPA2_WPA3_PSK");
            break;
        case WIFI_AUTH_WAPI_PSK:         /**< authenticate mode : WAPI_PSK */
            Serial.println("WAPI_PSK");
            break;
        case WIFI_AUTH_MAX:
            Serial.println("MAX");
            break;
        default:
            Serial.print("unknown=");
            Serial.println(thisType);
    }
}

void WirelessControl::printStatus(uint8_t status) {
    switch (status) {
        case WL_NO_SHIELD:
            Serial.println("\tResult: No sheild");
            break;
        case WL_IDLE_STATUS:
            Serial.println("\tResult: Idle");
            break;
        case WL_NO_SSID_AVAIL:
            Serial.println("\tResult: No SSID available");
            break;
        case WL_SCAN_COMPLETED:
            Serial.println("\tResult: Scan completed");
            break;
        case WL_CONNECTED:
            Serial.println("\tResult: Connected");
            break;
        case WL_CONNECT_FAILED:
            Serial.println("\tResult: Connect failed");
            break;
        case WL_CONNECTION_LOST:
            Serial.println("\tResult: Connection lost");
            break;
        case WL_DISCONNECTED:
            Serial.println("\tResult: disconnected");
            break;
        default:
            Serial.println("\tResult: unknown return");
    }
}

bool WirelessControl::is_error_status(uint8_t status) {
    if (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED || status == WL_CONNECTION_LOST) {
        return true;
    }
    return false;
}

bool WirelessControl::is_success_status(uint8_t status) {
    if (status == WL_CONNECTED) {
        return true;
    }
    return false;
}

bool WirelessControl::is_neutral_status(uint8_t status) {
    if (status == WL_IDLE_STATUS || status == WL_SCAN_COMPLETED || WL_DISCONNECTED) {
        return true;
    }
    return false;
}

int8_t WirelessControl::get_rssi() {
    return WiFi.RSSI();
}
