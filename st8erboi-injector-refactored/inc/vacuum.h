#pragma once

#include "fillhead_config.h"
#include "injector_comms.h"

class VacuumController {
	public:
	VacuumController(InjectorComms* comms);
	void setup();
	void updateVacuum();
	void updateState();
	void handleCommand(UserCommand cmd, const char* args);
	const char* getTelemetryString();
	void resetState();

	private:
	InjectorComms* m_comms;
	VacuumState m_state;

	// Sensor readings
	float m_vacuumPressurePsig;
	float m_smoothedVacuumPsig;
	bool m_firstVacuumReading;

	// --- Configurable Parameters ---
	float m_targetPsig;
	float m_rampTimeoutSec;       // <-- Changed to float seconds
	float m_leakTestDeltaPsig;
	float m_leakTestDurationSec;  // <-- Changed to float seconds

	// --- State Machine Variables ---
	uint32_t m_stateStartTimeMs; // Timer start point, always in milliseconds
	float m_leakTestStartPressure;

	char m_telemetryBuffer[256];

	// --- Command Handlers ---
	void handleVacuumOn();
	void handleVacuumOff();
	void handleLeakTest();
	void handleSetTarget(const char* args);
	void handleSetTimeout(const char* args);
	void handleSetLeakDelta(const char* args);
	void handleSetLeakDuration(const char* args);
};