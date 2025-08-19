#pragma once

#include "config.h"
#include "comms_controller.h"
#include "ClearCore.h" // Include for MotorDriver type

enum PinchValveState {
	VALVE_IDLE,
	VALVE_HOMING,
	VALVE_MOVING,
	VALVE_OPEN,
	VALVE_CLOSED,
	VALVE_ERROR
};

class PinchValve {
	public:
	PinchValve(const char* name, MotorDriver* motor, CommsController* comms);

	void setup();
	void update();
	void handleCommand(UserCommand cmd, const char* args);

	// Commands
	void home();
	void open();
	void close();
	void jog(const char* args);
	void enable();
	void disable();
	void abort();
	void reset();

	// Status
	bool isHomed() const { return m_isHomed; }
	const char* getTelemetryString();

	private:
	void moveSteps(long steps, int velocity_sps, int accel_sps2, int torque_percent);
	float getSmoothedTorque();

	const char* m_name;
	MotorDriver* m_motor;
	CommsController* m_comms;

	PinchValveState m_state;
	bool m_isHomed;
	uint32_t m_homingStartTime;
	float m_torqueLimit;

	// Torque Smoothing
	float m_smoothedTorque;
	bool m_firstTorqueReading;

	char m_telemetryBuffer[128];
};
