#ifndef CLIMATECONTROL_H
#define CLIMATECONTROL_H

#include <Arduino.h>
#include <deque>

#include "Logger.h"
#include "TempHumiditySensor.h"
#include "ExternalSettings.h"
#include "monitor.h"
#include "InfluxDBHandler.h"

// How often to collect temperature data, e.g. once every TEMPERATURE_COLLECTION_PERIOD_S
#define TEMPERATURE_COLLECTION_PERIOD_S 60

class TemperatureWindow {
public:
    TemperatureWindow(size_t maxSize);

	void addIfReady(float temp);

    void addValue(float temp);

    const std::deque<float>& getValues() const;

	float getDeltaOver(uint16_t seconds) const;

private:
    std::deque<float> data;
    size_t maxSize;
	long _last_collection_t = 0;
};


class ClimateControl {
    private:
	bool _at_temp_rise_limit(float delta);
	bool _at_temp_fall_limit(float delta);

	TemperatureWindow *_temp_window;

	// Keep track of how long the mist has been on and off
	long _mist_start_ms = 0;
	long _mist_end_ms = 0;

	ExternalSettings *_settings;
	SensorObjects *_sensors;
	ControlObjects *_controls;

	InfluxDBHandler *_influx = nullptr;

	void _monitor_fan_control();
	void _monitor_window_control();
	void _monitor_mist_control();

	// Return the detla between the temp "minutes" minutes ago and now
	float _temp_delta_over(uint16_t minutes);

	bool _need_fan_on();
	bool _need_fan_off();
	bool _need_window_opened();
	bool _need_window_closed();
	bool _need_mist_on();
	bool _need_mist_off();

	bool _is_mist_on_timer_active();
	bool _is_mist_off_timer_active();
	void _mist_activate_on_timer();
	void _mist_activate_off_timer();
	bool _mist_off_timer_is_clear();
	bool _mist_on_timer_just_ended();

    public:

    ClimateControl(ExternalSettings *settings, SensorObjects *sensors, ControlObjects *controls);
	void enable_influx_collection(InfluxDBHandler *influx);
	void disable_influx_collection();

	// Monitor the current temperature and humidity, and make decisions about fan and window control
	// based on the current temperature and humidity

	void report_metrics();
	void monitor();

    float current_temperature();
    float current_humidity();

	float get_short_temp_delta();
	float get_long_temp_delta();

	bool at_short_temp_rise_limit();
	bool at_short_temp_fall_limit();
	bool at_long_temp_rise_limit();
	bool at_long_temp_fall_limit();

	bool over_max_temp();
	bool under_min_temp();

	void start_misting_period();
	void stop_misting_period();
};


#endif