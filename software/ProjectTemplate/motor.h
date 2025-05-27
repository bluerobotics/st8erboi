#include "ClearCore.h"
#include "states.h" // Included in your uploaded file

// Torque monitoring constants
#define EWMA_ALPHA 0.2f
#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f

// --- Extern Global Variables ---
// These are defined in motor.cpp

extern float globalTorqueLimit;    // General torque limit, set by moveMotors
extern float torqueOffset;         // Global torque offset, set by CMD_SET_TORQUE_OFFSET

extern float smoothedTorqueValue1;
extern float smoothedTorqueValue2;
extern bool firstTorqueReading1;
extern bool firstTorqueReading2;
extern bool motorsAreEnabled;
extern uint32_t pulsesPerRev;      // Should be MOTOR_STEPS_PER_REV for consistency
extern int32_t machineStepCounter;
extern int32_t cartridgeStepCounter;

extern float jogTorqueLimit;
extern float homingTorqueLimit;
extern float feedTorqueLimit;

// --- Function Prototypes ---

// Initializes and enables both motors for use
void SetupMotors(void);

// Enables both motors and waits for HLFB asserted
void enableMotors(const char* reason_message);

// Disables both motors and resets torque smoothing
void disableMotors(const char* reason_message);

// Main function to command motor movements.
// The 'torque_limit' parameter will set the globalTorqueLimit for checkTorqueLimit.
void moveMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel);

// Returns true if either motor is currently moving or not in-position
bool checkMoving(void);

// Returns smoothed, offset torque value (EWMA filter)
// Considers the global torqueOffset.
float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);

// Checks if the output of getSmoothedTorqueEWMA exceeds globalTorqueLimit
// while motors are enabled/moving.
bool checkTorqueLimit(void);

// Abruptly stops all motor motion
void abortMove(void);

// Disables, clears alerts, resets counters, and re-enables both motors
void resetMotors(void);