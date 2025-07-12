#include "ClimateControl.h"

#define REASON_SHORT_RISE "Temp rise exceeds short threshold"
#define REASON_LONG_RISE "Temp rise exceeds short threshold"
#define REASON_OVER_MAX_TEMP "Temp exceeds max threshold"

#define REASON_SHORT_FALL "Temp fall exceeds short threshold"
#define REASON_LONG_FALL "Temp fall exceeds short threshold"
#define REASON_UNDER_MIN_TEMP "Temp below min threshold"

#define REASON_HUMIDITY_LOW "Humidity below target threshold"
#define REASON_HUMITIDY_OFF_PERIOD "Pausing after misting period"

extern Logger *LOGGER;

// TemperatureWindow class implementation

TemperatureWindow::TemperatureWindow(size_t maxSize) : maxSize(maxSize) {}

void TemperatureWindow::addIfReady(float temp) {
	long currentTime = millis();
	if (currentTime - _last_collection_t >= TEMPERATURE_COLLECTION_PERIOD_S * 1000) {
		addValue(temp);
		_last_collection_t = currentTime;
	}
}

void TemperatureWindow::addValue(float temp) {
	if (data.size() >= maxSize) {
		data.pop_front(); // remove oldest
	}
	data.push_back(temp); // add newest
}

const std::deque<float>& TemperatureWindow::getValues() const {
	return data;
}

float TemperatureWindow::getDeltaOver(uint16_t seconds) const {
	int minutes = seconds / 60;
	if (minutes >= maxSize) {
		//LOGGER->log_error("Requested a temp delta over a period longer than the history depth: " + String(minutes));
		return 0;
	}

	if (minutes > data.size()) {
		//LOGGER->log_error("Not enough data to calculate delta over " + String(minutes) + " minutes");
		return 0;
	}

	float start_temp = data[data.size() - 1 - minutes];
	float current_temp = data.back();
	return current_temp - start_temp;
}

// ClimateControl class implementation

ClimateControl::ClimateControl(ExternalSettings *settings, SensorObjects *sensors, ControlObjects *controls) : _settings(settings), _sensors(sensors), _controls(controls) {
  	LOGGER->log("Initializing ClimateControl");

	// Initialize the temp window object with the longest time period we need to capture.
	_temp_window = new TemperatureWindow(_settings->get_temp_long_delta_s());

  	// Set the initial temperature history
  	monitor();

	// Initialize us in the closed state
	stop_misting_period();
}

void ClimateControl::enable_influx_collection(InfluxDBHandler *influx) {
	_influx = influx;
}

void ClimateControl::disable_influx_collection() {
	_influx = nullptr;
}

void ClimateControl::report_metrics() {
	if (!_influx) {
		return;
	}	

	_influx->write_sensor_metric("DHT22", "temperature", _sensors->temphumid->current_temperature());
	_influx->write_sensor_metric("DHT22", "humidity", _sensors->temphumid->current_humidity());
	
	_sensors->temp->load_readings();
	for (auto &sensor : _sensors->temp->sensors) {
		_influx->write_sensor_metric(sensor.get_address_string().c_str(), "temperature", sensor.temp);
	}

	_sensors->light->read();
	_influx->write_sensor_metric("light", "full_luminosity", _sensors->light->getFullLuminosity());
	_influx->write_sensor_metric("light", "ir", _sensors->light->getIR());
	_influx->write_sensor_metric("light", "visible", _sensors->light->getVisible());
	_influx->write_sensor_metric("light", "lux", _sensors->light->getLux());
}

void ClimateControl::monitor() {
	// Add a new temperature reading, if its time
	_temp_window->addIfReady(_sensors->temphumid->current_temperature());

	// See if we need to toggle any of the controls
	_monitor_fan_control();
	_monitor_window_control();
	_monitor_mist_control();

	// Periodically check on the window.  It takes some time to move and we don't get any feedback
	// on whether the window is open or closed.  After a set amount of time we assume its done and
	// clear the control pins.
	_controls->window->monitor();
}

void ClimateControl::_monitor_mist_control() {
	if (_is_mist_off_timer_active() || _is_mist_on_timer_active()) {
		// One of either the "on" or "off" timer is active, so we don't need to do anything
		return;
	}

	// If we're here, no timers are active.  See if the "on" timer has just ended
	if (_mist_on_timer_just_ended()) {
		LOGGER->log_info("Misting period ended, watiing " + String(_settings->get_mist_off_ms() / 1000) + " seconds before checking humidity again");
		_influx && _influx->event_mist_off(REASON_HUMITIDY_OFF_PERIOD);

		// It has, so turn off the misting
		stop_misting_period();
		return;
	}

	if (_need_mist_on()) {
		// Turn the misting on, clear the "off" timer, and start the "on" timer
		start_misting_period();
	}
}

void ClimateControl::start_misting_period() {
	// Turn the misting on, clear the "off" timer, and start the "on" timer
	_controls->mist->turn_on();
	_mist_activate_on_timer();
}

void ClimateControl::stop_misting_period() {
	// Turn the misting off, clear the "on" timer, and start the "off" timer
	_controls->mist->turn_off();
	_mist_activate_off_timer();
}

bool ClimateControl::_mist_on_timer_just_ended() {
	// If the mist "on" timer is not active, and the _mist_end_ms has not been set, we know
	// that the "on" timer has just ended and we need to start the "off" timer.
	return !_is_mist_on_timer_active() && _mist_end_ms == 0;
}

void ClimateControl::_mist_activate_on_timer() {
	_mist_end_ms = 0;
	_mist_start_ms = millis();
}
void ClimateControl::_mist_activate_off_timer() {
	_mist_end_ms = millis();
	_mist_start_ms = 0;
}

bool ClimateControl::_is_mist_on_timer_active() {
	// If mist is off, there's no active "on" timer
	if (_controls->mist->is_off()) {
		return false;
	}

	// The timer is active if the current time is still less than the start time plus our "on" duration
	return  millis() < _mist_start_ms + _settings->get_mist_on_ms();
}


bool ClimateControl::_is_mist_off_timer_active() {
	// If mist is omn, there's no active "off" timer
	if (_controls->mist->is_on()) {
		return false;
	}

	// The timer is active if the current time is still less than the start time plus our "on" duration
	return  millis() < _mist_end_ms + _settings->get_mist_off_ms();
}

void ClimateControl::_monitor_fan_control() {
    if (_need_fan_on()) {
        _controls->fan->turn_on();
	} else if (_controls->fan->is_on() && _need_fan_off()) {
		_controls->fan->turn_off();
    } else if (_need_fan_off()) {
        _controls->fan->turn_off();
    }
}

void ClimateControl::_monitor_window_control() {
    if (_need_window_opened()) {
        _controls->window->open();
    } else if (_need_window_closed()) {
        _controls->window->close();
    }
}

bool ClimateControl::_need_mist_on() {
	// If the mist is already on, don't do anything
	if (_controls->mist->is_on()) {
		return false;
	}

	// If the humidity is low, turn on the mist
	if (_sensors->temphumid->current_humidity() < _settings->get_target_humidity()) {
		LOGGER->log_info("Turning mist on (humidity): " + String(_sensors->temphumid->current_humidity()) + "% < " + String(_settings->get_target_humidity()) + "%");
		_influx && _influx->event_mist_on(REASON_HUMIDITY_LOW);
		return true;
	}

	if (over_max_temp()) {
		LOGGER->log_info("Turning mist on (absolute): " + String(_sensors->temphumid->current_temperature()) + "F >= " + String(_settings->get_max_temp_f()) + "F");
		_influx && _influx->event_mist_on(REASON_OVER_MAX_TEMP);
		return true;
	}

	return false;
}

bool ClimateControl::_need_fan_on() {
    // If the fan is already on, don't do anything
    if (_controls->fan->is_on()) {
        return false;
    }

    // If the window is not open, don't turn the fan on
    if (_controls->window->is_closed()) {
        return false;
    }

	// If the temp is rising rapidly, or we're over our max temp, turn the fan on
    if (at_short_temp_rise_limit()) {
		LOGGER->log_info("Turning fan on (short rise limit): rise of " + String(get_short_temp_delta()) + "F will exceed target in " + String(_settings->get_temp_short_delta_s()) + "s");
		_influx && _influx->event_fan_on(REASON_SHORT_RISE);
		return true;
	}

	if (over_max_temp()) {
		LOGGER->log_info("Turning fan on (absolute): " + String(_sensors->temphumid->current_temperature()) + "F >= " + String(_settings->get_max_temp_f()) + "F");
		_influx && _influx->event_fan_on(REASON_OVER_MAX_TEMP);
        return true;
    }

    return false;
}

bool ClimateControl::_need_fan_off() {
    // If the fan is already off, don't do anything
    if (_controls->fan->is_off()) {
        return false;
    }
    
	// If the temp is falling rapidly, or we're under our min temp, turn the fan off
    if (at_short_temp_fall_limit()) {
		LOGGER->log_info("Turning fan off (short fall limit): fall of " + String(get_short_temp_delta()) + "F will fall below target in " + String(_settings->get_temp_short_delta_s()) + "s");
		_influx && _influx->event_fan_off(REASON_SHORT_FALL);
		return true;
	}

	if (under_min_temp()) {
		LOGGER->log_info("Turning fan off (absolute): " + String(_sensors->temphumid->current_temperature()) + "F <= " + String(_settings->get_min_temp_f()) + "F");
		_influx && _influx->event_fan_off(REASON_UNDER_MIN_TEMP);
        return true;
    }

    return false;
}

bool ClimateControl::_need_window_opened() {
    // Don't continue if window is already open
    if (_controls->window->is_open()) {
      return false;
    }

    // Open the windows if we're at a long rise limit or over the max temp
    if (at_long_temp_rise_limit()) {
		LOGGER->log_info("Opening window (long rise limit): " + String(get_long_temp_delta()) + "F will exceed target in " + String(_settings->get_temp_long_delta_s()) + "s");
		_influx && _influx->event_window_open(REASON_LONG_RISE);
		return true;
	}

	if (over_max_temp()) {
        LOGGER->log_info("Opening window (absolute): " + String(_sensors->temphumid->current_temperature()) + "F >= " + String(_settings->get_max_temp_f()) + "F");
		_influx && _influx->event_window_open(REASON_OVER_MAX_TEMP);
        return true;
    }

    return false;
}

bool ClimateControl::_need_window_closed() {
    // Don't continue if window is already closed
    if (_controls->window->is_closed()) {
        return false;
    }

    // Close unconditionally if temp is low enough
    if (at_long_temp_fall_limit()) {
		LOGGER->log_info("Closing window (long fall limit): " + String(get_long_temp_delta()) + "F will fall below target in " + String(_settings->get_temp_long_delta_s()) + "s");
		_influx && _influx->event_window_closed(REASON_LONG_FALL);
        return true;
    }

    // After dropping below a threshold temp, check to see if we've been consistently falling before closing
    if (under_min_temp()) {
		LOGGER->log_info("Closing window (absolute): " + String(_sensors->temphumid->current_temperature()) + "F <= " + String(_settings->get_min_temp_f()) + "F");
		_influx && _influx->event_window_closed(REASON_UNDER_MIN_TEMP);
        return true;
    }

    return false;
}

float ClimateControl::current_temperature() {
	return _sensors->temphumid->current_temperature();
}

float ClimateControl::current_humidity() {
	return _sensors->temphumid->current_humidity();
}

float ClimateControl::get_short_temp_delta() {
	return _temp_window->getDeltaOver(_settings->get_temp_short_delta_s());
}

float ClimateControl::get_long_temp_delta() {
	return _temp_window->getDeltaOver(_settings->get_temp_long_delta_s());
}

bool ClimateControl::at_short_temp_rise_limit() {
	return _at_temp_rise_limit(get_short_temp_delta());
}

bool ClimateControl::at_long_temp_rise_limit() {
	return _at_temp_rise_limit(get_long_temp_delta());
}

bool ClimateControl::_at_temp_rise_limit(float delta) {
	// If the temperature is falling, then we don't need to check the rise limit
	if (delta <= 0) {
		return false;
	}

	// Check to see if the temperature will rise above the target temp in the next collection period at the current rate of rise.
	if (_sensors->temphumid->current_temperature() + delta < _settings->get_target_temp_f()) {
		// It won't hit our target temp, so we don't need to take action
		return false;
	}

	return true;
}

bool ClimateControl::at_short_temp_fall_limit() {
	return _at_temp_fall_limit(get_short_temp_delta());
}

bool ClimateControl::at_long_temp_fall_limit() {
	return _at_temp_fall_limit(get_long_temp_delta());
}

bool ClimateControl::_at_temp_fall_limit(float delta) {
	// If the temperature is rising, then we don't need to check the fall limit
	if (delta >= 0) {
		return false;
	}

	// Check to see if the temperature will drop below the target temp in the next collection period at the current rate of fall.
	if (_sensors->temphumid->current_temperature() + delta > _settings->get_target_temp_f()) {
		return false;
	}

	return true;
}

bool ClimateControl::over_max_temp() {
	return _sensors->temphumid->current_temperature() > _settings->get_max_temp_f();
}

bool ClimateControl::under_min_temp() {
	return _sensors->temphumid->current_temperature() < _settings->get_min_temp_f();
}
