#pragma once

#include "injector_config.h"
#include "injector_comms.h"

class HeaterController {
	public:
	HeaterController(InjectorComms* comms);
	void setup();
	void updateTemperature();
	void updatePid();
	void handleCommand(UserCommand cmd, const char* args);
	const char* getTelemetryString();

	private:
	InjectorComms* m_comms;

	HeaterState m_heaterState;
	float m_temperatureCelsius;
	float m_smoothedTemperatureCelsius;
	bool m_firstTempReading;
	
	// PID variables
	float m_pid_setpoint;
	float m_pid_kp, m_pid_ki, m_pid_kd;
	float m_pid_integral;
	float m_pid_last_error;
	uint32_t m_pid_last_time;
	float m_pid_output;

	char m_telemetryBuffer[128];

	void resetPid();

	// Command Handlers
	void handleHeaterOn();
	void handleHeaterOff();
	void handleSetGains(const char* args);
	void handleSetSetpoint(const char* args);
	void handlePidOn();
	void handlePidOff();
};
