#ifndef STATES_H
#define STATES_H

#include <cstdint>

// --- State Enums ---
enum MainState : uint8_t {
	STANDBY_MODE,
	TEST_MODE,
	JOG_MODE,
	HOMING_MODE,
	FEED_MODE,
	DISABLED_MODE,
	MAIN_STATE_COUNT
};

enum HomingState : uint8_t {
	HOMING_NONE,
	HOMING_MACHINE,
	HOMING_CARTRIDGE,
	HOMING_STATE_COUNT
};

enum FeedState : uint8_t {
	FEED_NONE,
	FEED_STANDBY,
	FEED_INJECT,
	FEED_PURGE,
	FEED_RETRACT,
	FEED_STATE_COUNT
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


// --- State Name Arrays ---
extern const char *MainStateNames[MAIN_STATE_COUNT];
extern const char *HomingStateNames[HOMING_STATE_COUNT];
extern const char *FeedStateNames[FEED_STATE_COUNT];
extern const char *ErrorStateNames[ERROR_STATE_COUNT];

// --- State Data Class ---
class SystemStates {
	public:
	// States
	MainState mainState;
	HomingState homingState;
	FeedState feedState;
	ErrorState errorState;

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
	const char* homingStateStr() const;
	const char* errorStateStr() const;
};

#endif // STATES_H
