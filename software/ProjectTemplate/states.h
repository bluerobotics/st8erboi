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

enum HomingPhase : uint8_t {
	HOMING_PHASE_IDLE,
	HOMING_PHASE_RAPID_MOVE,    // Step 1: Rapid move towards endstop
	HOMING_PHASE_BACK_OFF,      // Step 2: Slowly back off after first torque detection
	HOMING_PHASE_TOUCH_OFF,     // Step 3: Slowly approach again for precise zeroing
	HOMING_PHASE_RETRACT,       // Step 4: Retract to final position after zeroing
	HOMING_PHASE_COMPLETE,      // Homing sequence finished successfully
	HOMING_PHASE_ERROR,         // Homing sequence encountered an error
	HOMING_PHASE_COUNT
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
	ERROR_MOTION_EXCEEDED_ABORT, // General motion error
	ERROR_NO_CARTRIDGE_HOME,
	ERROR_NO_MACHINE_HOME,
	ERROR_HOMING_TIMEOUT,        // Specific error for homing timeout
	ERROR_HOMING_NO_TORQUE_RAPID, // Error if rapid move finishes without torque
	ERROR_HOMING_NO_TORQUE_TOUCH, // Error if touch-off move finishes without torque
	ERROR_STATE_COUNT
};


// --- State Name Arrays ---
extern const char *MainStateNames[MAIN_STATE_COUNT];
extern const char *HomingStateNames[HOMING_STATE_COUNT];
extern const char *HomingPhaseNames[HOMING_PHASE_COUNT]; // New
extern const char *FeedStateNames[FEED_STATE_COUNT];
extern const char *ErrorStateNames[ERROR_STATE_COUNT];

class SystemStates {
	public:
	// States
	MainState mainState;
	HomingState homingState;         // What is being homed (Machine/Cartridge)
	HomingPhase currentHomingPhase;  // Current step in the active homing sequence
	FeedState feedState;
	ErrorState errorState;

	// Flags (event triggers)
	bool homingMachineDone;     // True if machine has been successfully homed at least once
	bool homingCartridgeDone;   // True if cartridge has been successfully homed at least once
	bool feedingDone;
	bool jogDone;
	uint32_t homingStartTime;   // To track duration of a homing operation for timeout

	// Homing parameters (populated by homing commands)
	float homing_stroke_mm_param;
	float homing_rapid_vel_mm_s_param;
	float homing_touch_vel_mm_s_param;
	float homing_acceleration_param; // Assuming mm/s^2
	float homing_retract_mm_param;
	float homing_torque_percent_param;

	// Calculated values for homing (populated when command is received)
	long homing_actual_stroke_steps;
	long homing_actual_retract_steps;
	int homing_actual_rapid_sps;
	int homing_actual_touch_sps;
	int homing_actual_accel_sps2; // steps/s^2

	// Internal Homing Constants
	static const long HOMING_DEFAULT_BACK_OFF_STEPS = 200; // Steps to back off after first touch
	static const long HOMING_TOUCH_OFF_EXTRA_STEPS = 50; // Extra travel during touch-off beyond back_off distance

	// Constructor
	SystemStates();

	// Reset all state/flags to default
	void reset();

	// Flag "handlers"
	void onHomingMachineDone();     // Called when machine homing sequence completes successfully
	void onHomingCartridgeDone();   // Called when cartridge homing sequence completes successfully
	void onFeedingDone();
	void onJogDone();

	// State as string
	const char* mainStateStr() const;
	const char* homingStateStr() const;    // Returns string for HOMING_NONE, HOMING_MACHINE, etc.
	const char* homingPhaseStr() const;    // New: returns string for current homing phase
	const char* feedStateStr() const;
	const char* errorStateStr() const;
};

#endif // STATES_H
