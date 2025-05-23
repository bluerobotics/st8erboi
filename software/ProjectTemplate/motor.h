#pragma once

#include "ClearCore.h"
#include "states.h"

// Bitmasks for selecting motors
#define MOTOR_M0  (1 << 0)
#define MOTOR_M1  (1 << 1)

// Torque monitoring constants
#define EWMA_ALPHA 0.2f
#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f

extern float globalTorqueLimit;
extern float torqueOffset;
extern float smoothedTorqueValue1;
extern float smoothedTorqueValue2;
extern bool firstTorqueReading1;
extern bool firstTorqueReading2;
extern uint32_t lastTorqueTime;
extern bool motorsAreEnabled;
extern uint32_t pulsesPerRev;
extern int32_t machineStepCounter;
extern int32_t cartridgeStepCounter;

// Initializes and enables both motors for use
void SetupMotors(void);

// Enables both motors and waits for HLFB asserted
void enableMotors(const char* reason_message);

// Disables both motors and resets torque smoothing
void disableMotors(const char* reason_message);

// Moves one or both motors with independent step count and speed
void moveMotors(int stepsM0, int stepsM1, int torque_limit, SimpleSpeedState speed, uint8_t motorMask);

// Returns true if either motor is currently moving or not in-position
bool checkMoving(void);

// Returns smoothed, offset torque value (EWMA filter)
float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead);

// Checks if torque exceeds limit while motors are enabled/moving
void checkTorqueLimit(void);

// Disables, clears alerts, resets counters, and re-enables both motors
void resetMotors(void);
