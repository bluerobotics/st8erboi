#include "states.h"

const char *MainStateNames[MAIN_STATE_COUNT] = { /* ... same ... */
	"STANDBY_MODE", "TEST_MODE", "JOG_MODE", "HOMING_MODE", "FEED_MODE", "DISABLED_MODE"
};
const char *HomingStateNames[HOMING_STATE_COUNT] = { /* ... same ... */
	"HOMING_NONE", "HOMING_MACHINE_ACTIVE", "HOMING_CARTRIDGE_ACTIVE"
};
const char *HomingPhaseNames[HOMING_PHASE_COUNT] = { /* ... same ... */
	"IDLE", "RAPID_MOVE", "BACK_OFF", "TOUCH_OFF", "RETRACT", "COMPLETE", "ERROR"
};
const char *FeedStateNames[FEED_STATE_COUNT] = {
	"FEED_NONE", "FEED_STANDBY",
	"FEED_INJECT_STARTING", "FEED_INJECT_ACTIVE", "FEED_INJECT_PAUSED", "FEED_INJECT_RESUMING",
	"FEED_PURGE_STARTING", "FEED_PURGE_ACTIVE", "FEED_PURGE_PAUSED", "FEED_PURGE_RESUMING",
	"FEED_MOVING_TO_HOME", "FEED_MOVING_TO_RETRACT",
	"FEED_OP_CANCELLED", "FEED_OP_COMPLETED"
};
const char *ErrorStateNames[ERROR_STATE_COUNT] = { /* ... same, ensure ERROR_INVALID_OPERATION and ERROR_NOT_HOMED are there ... */
	"ERROR_NONE", "ERROR_MANUAL_ABORT", "ERROR_TORQUE_ABORT", "ERROR_MOTION_EXCEEDED_ABORT",
	"ERROR_NO_CARTRIDGE_HOME", "ERROR_NO_MACHINE_HOME", "ERROR_HOMING_TIMEOUT",
	"ERROR_HOMING_NO_TORQUE_RAPID", "ERROR_HOMING_NO_TORQUE_TOUCH",
	"ERROR_INVALID_OPERATION", "ERROR_NOT_HOMED"
};

SystemStates::SystemStates() { reset(); }

void SystemStates::reset() {
	mainState = STANDBY_MODE;
	homingState = HOMING_NONE;
	currentHomingPhase = HOMING_PHASE_IDLE;
	feedState = FEED_STANDBY;
	errorState = ERROR_NONE;
	homingMachineDone = false;
	homingCartridgeDone = false;
	feedingDone = false;
	jogDone = false;
	homingStartTime = 0;

	// Homing params
	homing_stroke_mm_param = 0.0f; homing_rapid_vel_mm_s_param = 0.0f; homing_touch_vel_mm_s_param = 0.0f;
	homing_acceleration_param = 0.0f; homing_retract_mm_param = 0.0f; homing_torque_percent_param = 0.0f;
	homing_actual_stroke_steps = 0; homing_actual_retract_steps = 0; homing_actual_rapid_sps = 0;
	homing_actual_touch_sps = 0; homing_actual_accel_sps2 = 0;

	// Dispensing / Feed Operation Tracking
	active_op_target_ml = 0.0f;
	active_op_total_dispensed_ml = 0.0f;
	active_op_total_target_steps = 0;
	active_op_remaining_steps = 0;
	active_op_segment_initial_axis_steps = 0;
	active_op_steps_per_ml = 0.0f;
	active_dispense_operation_ongoing = false;
	active_op_velocity_sps = 0;
	active_op_accel_sps2 = 0;
	active_op_torque_percent = 0;
	active_op_initial_axis_steps = 0;
	last_completed_dispense_ml = 0;
}

void SystemStates::onHomingMachineDone()   { homingMachineDone = true; }
void SystemStates::onHomingCartridgeDone() { homingCartridgeDone = true; }
void SystemStates::onFeedingDone() {
	feedingDone = true;
	// When an entire feed op (like full inject volume) is done, reset tracking for the *next* one
	// The actual dispensed amount is now in active_op_total_dispensed_ml
	// active_dispense_operation_ongoing will be set to false by main.cpp
}
void SystemStates::onJogDone()             { jogDone = true; }

const char* SystemStates::mainStateStr()   const { return MainStateNames[mainState]; }
const char* SystemStates::homingStateStr() const { return HomingStateNames[homingState]; }
const char* SystemStates::homingPhaseStr() const { return HomingPhaseNames[currentHomingPhase]; }
const char* SystemStates::feedStateStr()   const { return FeedStateNames[feedState]; }
const char* SystemStates::errorStateStr()  const { return ErrorStateNames[errorState]; }