#include "states.h"

// --- State Name Arrays ---
const char *MainStateNames[MAIN_STATE_COUNT] = {
	"STANDBY_MODE",
	"TEST_MODE",
	"JOG_MODE",
	"HOMING_MODE",
	"FEED_MODE",
	"DISABLED_MODE"
};

const char *HomingStateNames[HOMING_STATE_COUNT] = {
	"HOMING_NONE",
	"HOMING_MACHINE_ACTIVE", // Clarified name for when sequence is running for machine
	"HOMING_CARTRIDGE_ACTIVE" // Clarified name for when sequence is running for cartridge
};

const char *HomingPhaseNames[HOMING_PHASE_COUNT] = {
	"IDLE",
	"RAPID_MOVE",
	"BACK_OFF",
	"TOUCH_OFF",
	"RETRACT",
	"COMPLETE",
	"ERROR"
};

const char *FeedStateNames[FEED_STATE_COUNT] = {
	"FEED_NONE",
	"FEED_STANDBY",
	"FEED_INJECT",
	"FEED_PURGE",
	"FEED_RETRACT"
};


const char *ErrorStateNames[ERROR_STATE_COUNT] = {
	"ERROR_NONE",
	"ERROR_MANUAL_ABORT",
	"ERROR_TORQUE_ABORT",
	"ERROR_MOTION_EXCEEDED_ABORT",
	"ERROR_NO_CARTRIDGE_HOME",
	"ERROR_NO_MACHINE_HOME"
};

// --- SystemStates Implementation ---

SystemStates::SystemStates() {
	reset();
}

void SystemStates::reset() {
	mainState = STANDBY_MODE;
	homingState = HOMING_NONE;
	currentHomingPhase = HOMING_PHASE_IDLE; // Initialize new state
	feedState = FEED_NONE;
	errorState = ERROR_NONE;

	homingMachineDone = false;      // Indicates if a successful machine homing has occurred at least once
	homingCartridgeDone = false;    // Indicates if a successful cartridge homing has occurred at least once
	feedingDone = false;
	jogDone = false;
	homingStartTime = 0;

	// Initialize homing parameters
	homing_stroke_mm_param = 0.0f;
	homing_rapid_vel_mm_s_param = 0.0f;
	homing_touch_vel_mm_s_param = 0.0f;
	homing_acceleration_param = 0.0f;
	homing_retract_mm_param = 0.0f;
	homing_torque_percent_param = 0.0f;

	homing_actual_stroke_steps = 0;
	homing_actual_retract_steps = 0;
	homing_actual_rapid_sps = 0;
	homing_actual_touch_sps = 0;
	homing_actual_accel_sps2 = 0;
}

// These functions are called when a full homing sequence (machine or cartridge) completes successfully.
// They set the flags that indicate that the respective home position is known.
void SystemStates::onHomingMachineDone()     { homingMachineDone = true; }
void SystemStates::onHomingCartridgeDone()   { homingCartridgeDone = true; }

void SystemStates::onFeedingDone()           { feedingDone = true; }
void SystemStates::onJogDone()               { jogDone = true; }

const char* SystemStates::mainStateStr()   const { return MainStateNames[mainState]; }
const char* SystemStates::homingStateStr() const { return HomingStateNames[homingState]; }
const char* SystemStates::homingPhaseStr() const { return HomingPhaseNames[currentHomingPhase]; } // New
const char* SystemStates::feedStateStr() const { return FeedStateNames[feedState]; }
const char* SystemStates::errorStateStr()  const { return ErrorStateNames[errorState]; }
