#pragma once

#include "injector_config.h"
#include "injector_comms.h"

class VacuumController {
	public:
	VacuumController(InjectorComms* comms);
	void setup();
	void updateVacuum();
	void handleCommand(UserCommand cmd, const char* args);
	const char* getTelemetryString();

	private:
	InjectorComms* m_comms;

	bool m_vacuumOn;
	bool m_vacuumValveOn;
	float m_vacuumPressurePsig;
	float m_smoothedVacuumPsig;
	bool m_firstVacuumReading;
	
	char m_telemetryBuffer[64];

	// Command Handlers
	void handleVacuumOn();
	void handleVacuumOff();
};
