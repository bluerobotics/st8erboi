#pragma once

#include "config.h"
#include "comms_controller.h"
#include "ClearCore.h" // Include for MotorDriver type

// Defines the overall state of the pinch valve.
enum PinchValveState {
	VALVE_IDLE,
	VALVE_HOMING,
	VALVE_MOVING,
	VALVE_CLOSING_ON_TORQUE, // NEW: State for the torque-based close operation
	VALVE_OPEN,
	VALVE_CLOSED,
	VALVE_ERROR
};

class PinchValve {
public:
	/**
	 * @brief Constructs a new PinchValve controller.
	 * @param name A descriptive name for the valve (e.g., "inj_valve").
	 * @param motor A pointer to the MotorDriver for this valve.
	 * @param comms A pointer to the shared CommsController for sending status messages.
	 */
	PinchValve(const char* name, MotorDriver* motor, CommsController* comms);

	/**
	 * @brief Initializes the motor hardware for the pinch valve.
	 */
	void setup();

	/**
	 * @brief The main update loop for the valve's state machine. Called continuously.
	 */
	void update();

	/**
	 * @brief The entry point for all commands related to this valve.
	 * @param cmd The command to be executed.
	 * @param args The arguments associated with the command, if any.
	 */
	void handleCommand(UserCommand cmd, const char* args);

	// --- Public Command Methods ---
	void home();
	void open();
	void close();
	void jog(const char* args);
	void enable();
	void disable();
	void abort();
	void reset();

	// --- Status Methods ---
	bool isHomed() const { return m_isHomed; }
	const char* getTelemetryString();

private:
	/**
	 * @brief Commands the motor to move a specified number of steps.
	 * @param steps The number of steps to move.
	 * @param velocity_sps The target velocity in steps per second.
	 * @param accel_sps2 The acceleration in steps per second squared.
	 */
	void moveSteps(long steps, int velocity_sps, int accel_sps2);

	/**
	 * @brief Calculates a smoothed torque value using an EWMA filter.
	 * @return The filtered torque value as a percentage.
	 */
	float getSmoothedTorque();

	// State machine for the multi-stage homing process
	typedef enum {
		HOMING_PHASE_IDLE,
		INITIAL_BACKOFF_START,
		INITIAL_BACKOFF_WAIT_TO_START,
		INITIAL_BACKOFF_MOVING,
		RAPID_SEARCH_START,
		RAPID_SEARCH_WAIT_TO_START,
		RAPID_SEARCH_MOVING,
		BACKOFF_START,
		BACKOFF_WAIT_TO_START,
		BACKOFF_MOVING,
		SLOW_SEARCH_START,
		SLOW_SEARCH_WAIT_TO_START,
		SLOW_SEARCH_MOVING,
		SET_OFFSET_START,
		SET_OFFSET_WAIT_TO_START,
		SET_OFFSET_MOVING,
		SET_ZERO
	} HomingPhase;

	// --- Member Variables ---
	const char* m_name;
	MotorDriver* m_motor;
	CommsController* m_comms;

	PinchValveState m_state;
	HomingPhase m_homingPhase;
	bool m_isHomed;
	uint32_t m_homingStartTime;
	float m_torqueLimit;

	// Torque Smoothing
	float m_smoothedTorque;
	bool m_firstTorqueReading;

	// Buffer for telemetry
	char m_telemetryBuffer[128];

	// Homing parameters loaded from config.h
	long m_homingDistanceSteps;
	long m_homingBackoffSteps;
	long m_homingFinalOffsetSteps;
	int m_homingUnifiedSps;
	int m_homingAccelSps2;
};
