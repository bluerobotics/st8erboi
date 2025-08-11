#pragma once

#include "ClearCore.h"
#include "config.h"

class Gantry;

class Axis {
	public:
	typedef enum {
		ABSOLUTE,
		INCREMENTAL
	} MoveType;

	Axis(Gantry* controller, const char* name, MotorDriver* motor1, MotorDriver* motor2,
	float stepsPerMm, float minPosMm, float maxPosMm,
	Connector* homingSensor1, Connector* homingSensor2, Connector* limitSensor, Connector* zBrake);

	void setupMotors();
	void updateState();
	void handleMove(const char* args);
	void handleHome(const char* args);
	void abort();
	void enable();
	void disable();
	bool isMoving();
	bool isHomed() { return m_homed; }
	const char* getStateString();
	float getPositionMm() const;
	float getSmoothedTorque();
	bool isEnabled();
	
	void startMove(float target_mm, float vel_mms, float accel_mms2, int torque, MoveType moveType);

	private:
	void moveSteps(long steps, int velSps, int accelSps2, int torque);
	bool checkTorqueLimit(MotorDriver* motor);
	float getRawTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead);
	void sendStatus(const char* statusType, const char* message);

	Gantry* m_controller;
	const char* m_name;
	MotorDriver* m_motor1;
	MotorDriver* m_motor2;
	Connector* m_homingSensor1;
	Connector* m_homingSensor2;
	Connector* m_limitSensor;
	Connector* m_zBrake;
	const char* m_activeCommand;

	typedef enum {
		STATE_STANDBY,
		STATE_STARTING_MOVE,
		STATE_MOVING,
		STATE_HOMING
	} AxisState;
	AxisState m_state;

	typedef enum {
		HOMING_NONE,
		RAPID_SEARCH_START,
		RAPID_SEARCH_WAIT_TO_START,
		RAPID_SEARCH_MOVING,
		BACKOFF_START,
		BACKOFF_WAIT_TO_START,
		BACKOFF_MOVING,
		SLOW_SEARCH_START,
		SLOW_SEARCH_WAIT_TO_START,
		SLOW_SEARCH_MOVING,
		SET_OFFSET_START, // Renamed from RETRACT
		SET_OFFSET_WAIT_TO_START,
		SET_OFFSET_MOVING,
		SET_ZERO
	} HomingPhase;
	
	HomingPhase homingPhase;

	float m_stepsPerMm;
	bool m_homed;
	float m_torqueLimit;
	
	float m_minPosMm;
	float m_maxPosMm;

	long m_homingDistanceSteps;
	long m_homingBackoffSteps;
	int m_homingRapidSps;
	int m_homingTouchSps;
	int m_homingBackoffSps;
	int m_homingAccelSps2;

	bool m_motor1_homed;
	bool m_motor2_homed;

	float m_smoothedTorqueM1;
	float m_smoothedTorqueM2;
	bool m_firstTorqueReadM1;
	bool m_firstTorqueReadM2;
};