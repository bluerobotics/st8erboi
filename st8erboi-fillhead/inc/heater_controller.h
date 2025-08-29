#pragma once

#include "config.h"
#include "comms_controller.h" // Assumes this is your comms class file

class HeaterController {
	public:
	HeaterController(CommsController* comms);
	void setup();
	void updateTemperature();
	void updatePid();
	void handleCommand(UserCommand cmd, const char* args);
	const char* getTelemetryString();

	private:
	CommsController* m_comms;
	HeaterState m_heaterState;

	// Sensor readings
	float m_temperatureCelsius;
	float m_smoothedTemperatureCelsius;
	bool m_firstTempReading;

	// PID variables
	float m_pid_setpoint;
	float m_pid_kp;
	float m_pid_ki;
	float m_pid_kd;
	float m_pid_integral;
	float m_pid_last_error;
	uint32_t m_pid_last_time;
	float m_pid_output;
	char m_telemetryBuffer[256];

	void resetPid();

	// Command Handlers
	void handleHeaterOn();
	void handleHeaterOff();
	void handleSetGains(const char* args);
	void handleSetSetpoint(const char* args);
};


