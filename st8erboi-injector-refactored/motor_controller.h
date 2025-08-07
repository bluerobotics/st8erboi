#pragma once

#include "injector_config.h"
#include "injector_comms.h"
#include "pinch_valve.h"

class MotorController {
	public:
	MotorController(InjectorComms* comms);
	void setup();
	void updateState();
	void handleCommand(UserCommand cmd, const char* args);
	const char* getTelemetryString();
	
	void enableMotors(const char* reason);
	void disableMotors(const char* reason);
	bool areMotorsEnabled() const;
	void abortMove();
	void resetState();

	private:
	InjectorComms* m_comms;

	// State Machines
	HomingState m_homingState;
	HomingPhase m_homingPhase;
	FeedState m_feedState;
	
	bool m_homingMachineDone;
	bool m_homingCartridgeDone;
	bool m_feedingDone;
	bool m_jogDone;
	uint32_t m_homingStartTime;
	bool m_motorsAreEnabled;

	// Pinch Valves
	PinchValve m_injectionValve;
	PinchValve m_vacuumValve;

	// Motor parameters
	float m_injectorMotorsTorqueLimit;
	float m_injectorMotorsTorqueOffset;
	float m_smoothedTorqueValue0, m_smoothedTorqueValue1;
	bool m_firstTorqueReading0, m_firstTorqueReading1;
	int32_t m_machineHomeReferenceSteps, m_cartridgeHomeReferenceSteps;
	
	// Operation-specific variables
	const char* m_activeFeedCommand;
	const char* m_activeJogCommand;

	float m_active_op_target_ml, m_active_op_total_dispensed_ml, m_active_op_steps_per_ml, m_last_completed_dispense_ml;
	long m_active_op_total_target_steps, m_active_op_remaining_steps, m_active_op_segment_initial_axis_steps, m_active_op_initial_axis_steps;
	bool m_active_dispense_INJECTION_ongoing;
	int m_active_op_velocity_sps, m_active_op_accel_sps2, m_active_op_torque_percent;
	
	long m_homingDefaultBackoffSteps;
	int m_feedDefaultTorquePercent;
	long m_feedDefaultVelocitySPS;
	long m_feedDefaultAccelSPS2;

	char m_telemetryBuffer[512];

	// Private methods
	void moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel);
	bool checkMoving();
	float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);
	bool checkTorqueLimit();
	void finalizeAndResetActiveDispenseOperation(bool success);
	void fullyResetActiveDispenseOperation();

	// Command Handlers
	void handleJogMove(const char* args);
	void handleMachineHomeMove();
	void handleCartridgeHomeMove();
	void handleMoveToCartridgeHome();
	void handleMoveToCartridgeRetract(const char* args);
	void handleInjectMove(const char* args);
	void handlePauseOperation();
	void handleResumeOperation();
	void handleCancelOperation();
	void handleSetTorqueOffset(const char* args);
};
