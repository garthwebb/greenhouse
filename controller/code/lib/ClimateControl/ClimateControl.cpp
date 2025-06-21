#include "ClimateControl.h"

extern Logger *LOGGER;

ClimateControl::ClimateControl(TempHumiditySensor *sensor) : _sensor(sensor) {
	// Initialize all history values to the current temperature 
  	for (int i = 0; i < TEMP_HISTORY_DEPTH; i++) {
		_temp_history[i] = _sensor->current_temperature();
  	}
}

void ClimateControl::monitor() {
	// Shift all history right, dropping earliest value
	for (int i = TEMP_HISTORY_DEPTH - 1; i > 0; i--) {
		_temp_history[i] = _temp_history[i-1];
	}
	_temp_history[0] = _sensor->current_temperature();
}

float ClimateControl::current_temperature() {
	return _sensor->current_temperature();
}

float ClimateControl::current_humidity() {
	return _sensor->current_humidity();
}

float ClimateControl::historical_temperature(uint16_t reading) {
	if (reading >= TEMP_HISTORY_DEPTH) {
		LOGGER->log_error("Requested a historical temp reading that is out of bounds: " + String(reading));
		return 0;
	}

	return _temp_history[reading];
}

// Return the detla between the temp "minutes" minutes ago and now
float ClimateControl::_temp_delta_over(uint16_t minutes) {
	uint16_t seconds = minutes * 60;
	uint16_t start_period = int(seconds/COLLECTION_PERIOD_S);

	if (start_period >= TEMP_HISTORY_DEPTH) {
		LOGGER->log_error("Requested a temp delta over a period longer than the history depth - minutes: " + String(minutes) + " , index: " + String(start_period));
		return 0;
	}

	float start_period_temp = _temp_history[start_period];
	float current_period_temp =  _temp_history[0];

	return current_period_temp - start_period_temp;
}

float ClimateControl::rise_delta() {
	return _temp_delta_over(RAPID_RISE_TEMP_MIN);
}

float ClimateControl::fall_delta() {
	return _temp_delta_over(FALLING_TEMP_MIN);
}

bool ClimateControl::in_rapid_rise() {
	if (_sensor->current_temperature() < TARGET_TEMP_F) {
		return false;
	}

	return rise_delta() > RAPID_RISE_TEMP_DELTA;
}

bool ClimateControl::in_rapid_fall() {
	if (_sensor->current_temperature() > TARGET_TEMP_F) {
		return false;
	}

	return fall_delta() < 0;
}

bool ClimateControl::over_max_temp() {
	return _sensor->current_temperature() > ALWAYS_OPEN_TEMP_F;
}

bool ClimateControl::under_min_temp() {
	return _sensor->current_temperature() < ALWAYS_CLOSE_TEMP_F;
}