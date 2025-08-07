#pragma once

#include "injector_config.h"
#include "motor_controller.h"
#include "heater_controller.h"
#include "vacuum_controller.h"
#include "injector_comms.h"

class InjectorController {
	public:
	InjectorController();
	void setup();
	void loop();

	private:
	void updateState();
	void handleMessage(const Message& msg);
	void sendGuiTelemetry();

	// Sub-controllers
	MotorController m_motorController;
	HeaterController m_heaterController;
	VacuumController m_vacuumController;
	InjectorComms m_comms;

	// Main state machine
	MainState m_mainState;
	ErrorState m_errorState;

	uint32_t m_lastGuiTelemetryTime;
	uint32_t m_lastSensorSampleTime;

	// Command Handlers
	void handleEnable();
	void handleDisable();
	void handleAbort();
	void handleClearErrors();
	void handleStandbyMode();
	void handleSetPeerIp(const char* msg);
	void handleClearPeerIp();
};
