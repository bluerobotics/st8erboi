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
	"HOMING_MACHINE",
	"HOMING_CARTRIDGE"
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
	feedState = FEED_NONE;
	errorState = ERROR_NONE;
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
const char* SystemStates::homingStateStr() const { return HomingStateNames[homingState]; }
const char* SystemStates::feedStateStr() const { return FeedStateNames[feedState]; }	
const char* SystemStates::errorStateStr()  const { return ErrorStateNames[errorState]; }
