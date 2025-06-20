// ============================================================================
// injector.h
//
// Public interface for the Injector subsystem.
//
// Declares the Injector class and related enums that manage system states,
// error tracking, motion phases, and command handling for a ClearCore-driven
// fluid injector system.
//
// Exposes methods for external communication (UDP), motor control,
// operational state transitions (e.g., homing, jog, feed), and runtime telemetry.
//
// All logic implementations reside in the injector_*.cpp source files.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#pragma once

#include <cstdint>
#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

// --- State Enums ---
enum MainState : uint8_t {
	STANDBY_MODE,
	TEST_MODE,
	JOG_MODE,
	HOMING_MODE,
	FEED_MODE,
	DISABLED_MODE
};

enum HomingState : uint8_t {
	HOMING_NONE,
	HOMING_MACHINE,
	HOMING_CARTRIDGE
};

enum HomingPhase : uint8_t {
	HOMING_PHASE_IDLE,
	HOMING_PHASE_RAPID_MOVE,
	HOMING_PHASE_BACK_OFF,
	HOMING_PHASE_TOUCH_OFF,
	HOMING_PHASE_RETRACT,
	HOMING_PHASE_COMPLETE,
	HOMING_PHASE_ERROR
};

enum FeedState : uint8_t {
	FEED_NONE,
	FEED_STANDBY,
	FEED_INJECT_STARTING,
	FEED_INJECT_ACTIVE,
	FEED_INJECT_PAUSED,
	FEED_INJECT_RESUMING,
	FEED_PURGE_STARTING,
	FEED_PURGE_ACTIVE,
	FEED_PURGE_PAUSED,
	FEED_PURGE_RESUMING,
	FEED_MOVING_TO_HOME,
	FEED_MOVING_TO_RETRACT,
	FEED_INJECTION_CANCELLED,
	FEED_INJECTION_COMPLETED
};

enum ErrorState : uint8_t {
	ERROR_NONE,
	ERROR_MANUAL_ABORT,
	ERROR_TORQUE_ABORT,
	ERROR_MOTION_EXCEEDED_ABORT,
	ERROR_NO_CARTRIDGE_HOME,
	ERROR_NO_MACHINE_HOME,
	ERROR_HOMING_TIMEOUT,
	ERROR_HOMING_NO_TORQUE_RAPID,
	ERROR_HOMING_NO_TORQUE_TOUCH,
	ERROR_INVALID_INJECTION,
	ERROR_NOT_HOMED
};

// --- Constant Arrays (extern in .h) ---
extern const char* MAIN_STATE_NAMES[];
extern const char* HOMING_STATE_NAMES[];
extern const char* HOMING_PHASE_NAMES[];
extern const char* FEED_STATE_NAMES[];
extern const char* ERROR_STATE_NAMES[];

class Injector {
	public:
	Injector();
	void reset();

	void onHomingMachineDone();
	void onHomingCartridgeDone();
	void onFeedingDone();
	void onJogDone();

	const char* mainStateStr() const;
	const char* homingStateStr() const;
	const char* homingPhaseStr() const;
	const char* feedStateStr() const;
	const char* errorStateStr() const;

	// Public variables (lowerCamelCase)
	MainState mainState;
	HomingState homingState;
	HomingPhase homingPhase;
	FeedState feedState;
	ErrorState errorState;

	bool homingMachineDone;
	bool homingCartridgeDone;
	bool feedingDone;
	bool jogDone;

	bool terminalDiscovered;
	uint16_t terminalPort;

	bool motorsEnabled;

	float torqueLimit;
	float torqueOffset;
	float smoothedTorque1;
	float smoothedTorque2;
	bool firstTorque1;
	bool firstTorque2;

	uint32_t pulsesPerRev;
	int32_t stepCounterMachine;
	int32_t stepCounterCartridge;
	int32_t machineHomeSteps;
	int32_t cartridgeHomeSteps;

	int32_t velocityLimit;
	int32_t accelerationLimit;

	float homingStrokeMm;
	float homingRapidVel;
	float homingTouchVel;
	float homingAccel;
	float homingRetractMm;
	float homingTorquePercent;

	int32_t homingStrokeSteps;
	int32_t homingRetractSteps;
	int32_t homingRapidSps;
	int32_t homingTouchSps;
	int32_t homingAccelSps2;

	float activeTargetMl;
	float activeDispensedMl;
	int32_t activeTargetSteps;
	int32_t activeRemainingSteps;
	int32_t activeSegmentStartSteps;
	float stepsPerMl;
	bool injectionOngoing;
	int32_t activeVelocitySps;
	int32_t activeAccelSps2;
	int32_t activeTorquePercent;
	int32_t activeInitialSteps;
	float lastDispensedMl;
};
