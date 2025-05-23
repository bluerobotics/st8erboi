#include "states.h"

// --- State Name Arrays ---
const char *MainStateNames[MAIN_STATE_COUNT] = {
	"STANDBY",
	"MOVING",
	"ERROR",
	"DISABLED"
};

const char *MovingStateNames[MOVING_STATE_COUNT] = {
	"MOVING_NONE",
	"MOVING_JOG",
	"MOVING_HOMING",
	"MOVING_FEEDING"
};

const char *HomingStateNames[HOMING_STATE_COUNT] = {
	"MOVING_HOMING_NONE",
	"MOVING_HOMING_MACHINE",
	"MOVING_HOMING_CARTRIDGE"
};

const char *ErrorStateNames[ERROR_STATE_COUNT] = {
	"ERROR_NONE",
	"ERROR_MANUAL_ABORT",
	"ERROR_TORQUE_ABORT",
	"ERROR_MOTION_EXCEEDED_ABORT",
	"ERROR_NO_CARTRIDGE_HOME",
	"ERROR_NO_MACHINE_HOME"
};

const char *SimpleSpeedStateNames[SPEED_STATE_COUNT] = {
	"FAST",
	"SLOW"
};

// --- SystemStates Implementation ---

SystemStates::SystemStates() {
	reset();
}

void SystemStates::reset() {
	mainState = STANDBY;
	movingState = MOVING_NONE;
	homingState = MOVING_HOMING_NONE;
	errorState = ERROR_NONE;
	simpleSpeedState = FAST;
	homingMachineDone = false;
	homingCartridgeDone = false;
	feedingDone = false;
	jogDone = false;
	homingStartTime = 0;
}

void SystemStates::onHomingMachineDone()     { homingMachineDone = true; }
void SystemStates::onHomingCartridgeDone()   { homingCartridgeDone = true; }
void SystemStates::onFeedingDone()           { feedingDone = true; }
void SystemStates::onJogDone()               { jogDone = true; }

const char* SystemStates::mainStateStr()   const { return MainStateNames[mainState]; }
const char* SystemStates::movingStateStr() const { return MovingStateNames[movingState]; }
const char* SystemStates::homingStateStr() const { return HomingStateNames[homingState]; }
const char* SystemStates::errorStateStr()  const { return ErrorStateNames[errorState]; }
const char* SystemStates::speedStateStr()  const { return SimpleSpeedStateNames[simpleSpeedState]; }
