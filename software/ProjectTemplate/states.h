#ifndef STATES_H
#define STATES_H

#include <cstdint>

// --- State Enums ---
enum MainState : uint8_t { /* ... same ... */
	STANDBY_MODE, TEST_MODE, JOG_MODE, HOMING_MODE, FEED_MODE, DISABLED_MODE, MAIN_STATE_COUNT
};
enum HomingState : uint8_t { /* ... same ... */
	HOMING_NONE, HOMING_MACHINE, HOMING_CARTRIDGE, HOMING_STATE_COUNT
};
enum HomingPhase : uint8_t { /* ... same ... */
	HOMING_PHASE_IDLE, HOMING_PHASE_RAPID_MOVE, HOMING_PHASE_BACK_OFF, HOMING_PHASE_TOUCH_OFF,
	HOMING_PHASE_RETRACT, HOMING_PHASE_COMPLETE, HOMING_PHASE_ERROR, HOMING_PHASE_COUNT
};

enum FeedState : uint8_t {
	FEED_NONE,
	FEED_STANDBY,
	FEED_INJECT_STARTING,   // Brief state while initiating inject
	FEED_INJECT_ACTIVE,     // Actively injecting
	FEED_INJECT_PAUSED,     // Injection is paused
	FEED_INJECT_RESUMING,   // Brief state while initiating resume
	FEED_PURGE_STARTING,    // Brief state while initiating purge
	FEED_PURGE_ACTIVE,      // Actively purging
	FEED_PURGE_PAUSED,      // Purge is paused
	FEED_PURGE_RESUMING,    // Brief state while initiating resume
	FEED_MOVING_TO_HOME,
	FEED_MOVING_TO_RETRACT,
	FEED_OPERATION_CANCELLED,
	FEED_OPERATION_COMPLETED, // Intermediate state before going to STANDBY
	FEED_STATE_COUNT
};

enum ErrorState : uint8_t { /* ... same, ensure ERROR_INVALID_OPERATION is there ... */
	ERROR_NONE, ERROR_MANUAL_ABORT, ERROR_TORQUE_ABORT, ERROR_MOTION_EXCEEDED_ABORT,
	ERROR_NO_CARTRIDGE_HOME, ERROR_NO_MACHINE_HOME, ERROR_HOMING_TIMEOUT,
	ERROR_HOMING_NO_TORQUE_RAPID, ERROR_HOMING_NO_TORQUE_TOUCH,
	ERROR_INVALID_OPERATION, ERROR_NOT_HOMED, ERROR_STATE_COUNT
};

extern const char *MainStateNames[MAIN_STATE_COUNT];
extern const char *HomingStateNames[HOMING_STATE_COUNT];
extern const char *HomingPhaseNames[HOMING_PHASE_COUNT];
extern const char *FeedStateNames[FEED_STATE_COUNT];
extern const char *ErrorStateNames[ERROR_STATE_COUNT];

class SystemStates {
	public:
	MainState mainState;
	HomingState homingState;
	HomingPhase currentHomingPhase;
	FeedState feedState;
	ErrorState errorState;

	bool homingMachineDone;
	bool homingCartridgeDone;
	bool feedingDone; // True when an entire feed sequence (e.g. full inject volume) completes
	bool jogDone;
	uint32_t homingStartTime;

	// Homing parameters ... (same as before)
	float homing_stroke_mm_param;
	float homing_rapid_vel_mm_s_param;
	float homing_touch_vel_mm_s_param;
	float homing_acceleration_param;
	float homing_retract_mm_param;
	float homing_torque_percent_param;
	long homing_actual_stroke_steps;
	long homing_actual_retract_steps;
	int homing_actual_rapid_sps;
	int homing_actual_touch_sps;
	int homing_actual_accel_sps2;
	static const long HOMING_DEFAULT_BACK_OFF_STEPS = 200;

	// Dispensing / Feed Operation Tracking
	float active_op_target_ml;
	float active_op_total_dispensed_ml; // Total dispensed for the current multi-segment operation
	long active_op_total_target_steps;  // Original total steps for the operation
	long active_op_remaining_steps;     // Remaining steps when paused
	long active_op_segment_initial_axis_steps; // Motor steps at start of current segment (after resume)
	float active_op_steps_per_ml;
	bool active_dispense_operation_ongoing; // Overall flag for inject/purge sequence
	
	// Store original parameters for resume
	int active_op_velocity_sps;
	int active_op_accel_sps2;
	int active_op_torque_percent;


	SystemStates();
	void reset();
	void onHomingMachineDone();
	void onHomingCartridgeDone();
	void onFeedingDone(); // This might be better named or re-purposed
	void onJogDone();

	const char* mainStateStr() const;
	const char* homingStateStr() const;
	const char* homingPhaseStr() const;
	const char* feedStateStr() const;
	const char* errorStateStr() const;
};
#endif // STATES_H