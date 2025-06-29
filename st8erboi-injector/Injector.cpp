#include "injector.h"
#include <math.h>

const char *Injector::MainStateNames[MAIN_STATE_COUNT] = {
	"STANDBY_MODE",
	"JOG_MODE",
	"HOMING_MODE",
	"FEED_MODE",
	"DISABLED_MODE"
};

const char *Injector::HomingStateNames[HOMING_STATE_COUNT] = {
	"HOMING_NONE",
	"HOMING_MACHINE_ACTIVE",
	"HOMING_CARTRIDGE_ACTIVE"
};

const char *Injector::HomingPhaseNames[HOMING_PHASE_COUNT] = {
	"IDLE",
	"STARTING_MOVE",
	"RAPID_MOVE",
	"BACK_OFF",
	"TOUCH_OFF",
	"RETRACT",
	"COMPLETE",
	"ERROR"
};

const char *Injector::FeedStateNames[FEED_STATE_COUNT] = {
	"FEED_NONE",
	"FEED_STANDBY",
	"FEED_INJECT_STARTING",
	"FEED_INJECT_ACTIVE",
	"FEED_INJECT_PAUSED",
	"FEED_INJECT_RESUMING",
	"FEED_PURGE_STARTING",
	"FEED_PURGE_ACTIVE",
	"FEED_PURGE_PAUSED",
	"FEED_PURGE_RESUMING",
	"FEED_MOVING_TO_HOME",
	"FEED_MOVING_TO_RETRACT",
	"FEED_OP_CANCELLED",
	"FEED_OP_COMPLETED"
};

// --- MODIFIED NAME ARRAY ---
const char *Injector::ErrorStateNames[ERROR_STATE_COUNT] = {
	"ERROR_NONE",
	"ERROR_MANUAL_ABORT",
	"ERROR_TORQUE_ABORT",
	"ERROR_MOTION_EXCEEDED_ABORT",
	"ERROR_NO_CARTRIDGE_HOME",
	"ERROR_NO_MACHINE_HOME",
	"ERROR_HOMING_TIMEOUT",
	"ERROR_HOMING_NO_TORQUE_RAPID",
	"ERROR_HOMING_NO_TORQUE_TOUCH",
	"ERROR_INVALID_INJECTION",
	"ERROR_NOT_HOMED",
	"ERROR_INVALID_PARAMETERS", // ADDED
	"ERROR_MOTORS_DISABLED"     // ADDED
};

Injector::Injector(){
	reset();
}

void Injector::reset() {
	mainState = STANDBY_MODE;
	homingState = HOMING_NONE;
	currentHomingPhase = HOMING_PHASE_IDLE;
	feedState = FEED_STANDBY;
	errorState = ERROR_NONE;
	homingMachineDone = false;
	homingCartridgeDone = false;
	feedingDone = false;
	jogDone = false;
	homingPinchDone = false;
	homingStartTime = 0;
	PITCH_MM_PER_REV = 5;
	
	terminalPort = 0;
	terminalDiscovered = false;
	telemInterval = 50;
	
	injectorMotorsTorqueLimit	= 2.0f;
	injectorMotorsTorqueOffset = -2.4f;
	smoothedTorqueValue0 = 0.0f;
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading0 = true;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
	motorsAreEnabled = false;
	pulsesPerRev = 800;
	machineStepCounter = 0;
	cartridgeStepCounter = 0;
	velocityLimit = 10000;
	accelerationLimit = 100000;
	homingDefaultBackoffSteps = 200;
	feedDefaultTorquePercent = 30;
	feedDefaultVelocitySPS = 1000;
	feedDefaultAccelSPS2 = 10000;
	machineHomeReferenceSteps = 0;
	cartridgeHomeReferenceSteps = 0;

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

	active_op_target_ml = 0.0f;
	active_op_total_dispensed_ml = 0.0f;
	active_op_total_target_steps = 0;
	active_op_remaining_steps = 0;
	active_op_segment_initial_axis_steps = 0;
	active_op_steps_per_ml = 0.0f;
	active_dispense_INJECTION_ongoing = false;
	active_op_velocity_sps = 0;
	active_op_accel_sps2 = 0;
	active_op_torque_percent = 0;
	active_op_initial_axis_steps = 0;
	last_completed_dispense_ml = 0;
}

void Injector::onHomingMachineDone(){
	homingMachineDone = true;
}

void Injector::onHomingCartridgeDone(){
	homingCartridgeDone = true;
}

void Injector::onFeedingDone(){
	feedingDone = true;
}

void Injector::onJogDone(){
	jogDone = true;
}

const char* Injector::mainStateStr()   const { return MainStateNames[mainState]; }
const char* Injector::homingStateStr() const { return HomingStateNames[homingState]; }
const char* Injector::homingPhaseStr() const { return HomingPhaseNames[currentHomingPhase]; }
const char* Injector::feedStateStr()   const { return FeedStateNames[feedState]; }
const char* Injector::errorStateStr()  const { return ErrorStateNames[errorState]; }