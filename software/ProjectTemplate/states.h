#ifndef STATES_H
#define STATES_H

#include <cstdint>

// --- State Enums ---
enum MainState : uint8_t {
	STANDBY,
	MOVING,
	ERROR,
	DISABLED,
	MAIN_STATE_COUNT
};

enum MovingState : uint8_t {
	MOVING_NONE,
	MOVING_JOG,
	MOVING_HOMING,
	MOVING_FEEDING,
	MOVING_STATE_COUNT
};

enum HomingState : uint8_t {
	MOVING_HOMING_NONE,
	MOVING_HOMING_MACHINE,
	MOVING_HOMING_CARTRIDGE,
	HOMING_STATE_COUNT
};

enum ErrorState : uint8_t {
	ERROR_NONE,
	ERROR_MANUAL_ABORT,
	ERROR_TORQUE_ABORT,
	ERROR_MOTION_EXCEEDED_ABORT,
	ERROR_NO_CARTRIDGE_HOME,
	ERROR_NO_MACHINE_HOME,
	ERROR_STATE_COUNT
};

enum SimpleSpeedState : uint8_t {
	FAST,
	SLOW,
	SPEED_STATE_COUNT
};

// --- State Name Arrays ---
extern const char *MainStateNames[MAIN_STATE_COUNT];
extern const char *MovingStateNames[MOVING_STATE_COUNT];
extern const char *HomingStateNames[HOMING_STATE_COUNT];
extern const char *ErrorStateNames[ERROR_STATE_COUNT];
extern const char *SimpleSpeedStateNames[SPEED_STATE_COUNT];

// --- State Data Class ---
class SystemStates {
	public:
	// States
	MainState mainState;
	MovingState movingState;
	HomingState homingState;
	ErrorState errorState;
	SimpleSpeedState simpleSpeedState;

	// Flags (event triggers)
	bool homingMachineDone;
	bool homingCartridgeDone;
	bool feedingDone;
	bool jogDone;
	uint32_t homingStartTime;

	// Constructor
	SystemStates();

	// Reset all state/flags to default
	void reset();

	// Flag "handlers"
	void onHomingMachineDone();
	void onHomingCartridgeDone();
	void onFeedingDone();
	void onJogDone();

	// State as string
	const char* mainStateStr() const;
	const char* movingStateStr() const;
	const char* homingStateStr() const;
	const char* errorStateStr() const;
	const char* speedStateStr() const;
};

#endif // STATES_H
