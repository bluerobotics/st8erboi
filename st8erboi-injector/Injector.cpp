// ============================================================================
// injector.cpp
//
// Core initialization and runtime state management for the Injector class.
//
// Implements constructor logic, state tracking, and string mapping utilities
// for translating internal enum states into telemetry and UI-friendly outputs.
//
// Contains basic system reset logic and internal flag transitions
// for mode entry/exit coordination.
//
// Corresponds to declarations in `injector.h`.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#include "injector.h"
#include <cmath>

const char* MAIN_STATE_NAMES[] = {
	"STANDBY_MODE",
	"TEST_MODE",
	"JOG_MODE",
	"HOMING_MODE",
	"FEED_MODE",
	"DISABLED_MODE"
};

const char* HOMING_STATE_NAMES[] = {
	"HOMING_NONE",
	"HOMING_MACHINE",
	"HOMING_CARTRIDGE"
};

const char* HOMING_PHASE_NAMES[] = {
	"HOMING_PHASE_IDLE",
	"HOMING_PHASE_RAPID_MOVE",
	"HOMING_PHASE_BACK_OFF",
	"HOMING_PHASE_TOUCH_OFF",
	"HOMING_PHASE_RETRACT",
	"HOMING_PHASE_COMPLETE",
	"HOMING_PHASE_ERROR"
};

const char* FEED_STATE_NAMES[] = {
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
	"FEED_INJECTION_CANCELLED",
	"FEED_INJECTION_COMPLETED"
};

const char* ERROR_STATE_NAMES[] = {
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
	"ERROR_NOT_HOMED"
};

Injector::Injector() {
	reset();
}

void Injector::reset() {
	mainState = STANDBY_MODE;
	homingState = HOMING_NONE;
	homingPhase = HOMING_PHASE_IDLE;
	feedState = FEED_STANDBY;
	errorState = ERROR_NONE;

	homingMachineDone = false;
	homingCartridgeDone = false;
	feedingDone = false;
	jogDone = false;

	terminalDiscovered = false;
	terminalPort = 0;

	motorsEnabled = false;

	torqueLimit = 2.0f;
	torqueOffset = -2.4f;
	smoothedTorque1 = 0.0f;
	smoothedTorque2 = 0.0f;
	firstTorque1 = true;
	firstTorque2 = true;

	pulsesPerRev = 800;
	stepCounterMachine = 0;
	stepCounterCartridge = 0;
	machineHomeSteps = 0;
	cartridgeHomeSteps = 0;

	velocityLimit = 10000;
	accelerationLimit = 100000;

	homingStrokeMm = 0.0f;
	homingRapidVel = 0.0f;
	homingTouchVel = 0.0f;
	homingAccel = 0.0f;
	homingRetractMm = 0.0f;
	homingTorquePercent = 0.0f;

	homingStrokeSteps = 0;
	homingRetractSteps = 0;
	homingRapidSps = 0;
	homingTouchSps = 0;
	homingAccelSps2 = 0;

	activeTargetMl = 0.0f;
	activeDispensedMl = 0.0f;
	activeTargetSteps = 0;
	activeRemainingSteps = 0;
	activeSegmentStartSteps = 0;
	stepsPerMl = 0.0f;
	injectionOngoing = false;
	activeVelocitySps = 0;
	activeAccelSps2 = 0;
	activeTorquePercent = 0;
	activeInitialSteps = 0;
	lastDispensedMl = 0.0f;
}

void Injector::onHomingMachineDone() {
	homingMachineDone = true;
}

void Injector::onHomingCartridgeDone() {
	homingCartridgeDone = true;
}

void Injector::onFeedingDone() {
	feedingDone = true;
}

void Injector::onJogDone() {
	jogDone = true;
}

const char* Injector::mainStateStr() const {
	return MAIN_STATE_NAMES[mainState];
}

const char* Injector::homingStateStr() const {
	return HOMING_STATE_NAMES[homingState];
}

const char* Injector::homingPhaseStr() const {
	return HOMING_PHASE_NAMES[homingPhase];
}

const char* Injector::feedStateStr() const {
	return FEED_STATE_NAMES[feedState];
}

const char* Injector::errorStateStr() const {
	return ERROR_STATE_NAMES[errorState];
}