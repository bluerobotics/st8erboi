#pragma once

#include "injector_config.h"
#include "injector_comms.h"

enum PinchValveState {
	VALVE_IDLE,
	VALVE_HOMING,
	VALVE_MOVING
};

class PinchValve {
	public:
	PinchValve(const char* name, MotorDriver* motor, InjectorComms* comms);

	void setup();
	void update();

	// Commands
	void home();
	void open();
	void close();
	void jog(float dist_mm, float vel_mms, float accel_mms2, int torque_percent);
	void enable();
	void disable();

	// Status
	bool isHomed() const { return m_isHomed; }
	const char* getTelemetryString();

	private:
	void move(long steps, int velocity_sps, int accel_sps2, int torque_percent);
	float getSmoothedTorque();

	const char* m_name;
	MotorDriver* m_motor;
	InjectorComms* m_comms;

	PinchValveState m_state;
	bool m_isHomed;
	uint32_t m_homingStartTime;
	float m_torqueLimit;

	// Torque Smoothing
	float m_smoothedTorque;
	bool m_firstTorqueReading;

	char m_telemetryBuffer[128];
};
