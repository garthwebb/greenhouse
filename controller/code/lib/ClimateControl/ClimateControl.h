#ifndef CLIMATECONTROL_H
#define CLIMATECONTROL_H

#include <Arduino.h>

#include "Logger.h"
#include "SensorControl.h"

// Temp delta we define as "rapid" and should assume its still going up
#define RAPID_RISE_TEMP_DELTA 10
// Time period for the temp delta to be considered "rapid"
#define RAPID_RISE_TEMP_MIN 30

// Temp below which we should close the windows if its been long enough since open
#define DROPPING_TEMP_F (TARGET_TEMP_F + 5)
// Time in seconds since last open before we consider closing again
#define DROPPING_TIME_SINCE_OPEN_S (30 * 60)
#define DROPPING_TIME_SINCE_OPEN_MS (1000 * DROPPING_TIME_SINCE_OPEN_S)
// How long in minutes the temp needs to be consistently falling to be considered cooling down
#define FALLING_TEMP_MIN 120

#define TEMP_HISTORY_DEPTH 400

class ClimateControl {
    private:
	float _temp_history[TEMP_HISTORY_DEPTH];
	SensorControl *_sensor;

	// Return the detla between the temp "minutes" minutes ago and now
	float _temp_delta_over(uint16_t minutes);

    public:

    ClimateControl(SensorControl *sensor);
	void monitor();

    float current_temperature();
    float current_humidity();

	float historical_temperature(uint16_t reading);

	float rise_delta();
	float fall_delta();

	bool in_rapid_rise();
	bool in_rapid_fall();

	bool over_max_temp();
	bool under_min_temp();
};

#endif