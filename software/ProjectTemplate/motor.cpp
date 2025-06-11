#include "motor.h"
#include "messages.h" // For sendToPC (though typically messages.cpp includes motor.h)
#include <math.h>     // For fabsf

// --- Module globals / Definitions for externs from motor.h ---
float globalTorqueLimit    = 2.0f;   // Default overall limit, overridden by moveMotors's param
float torqueOffset         = -2.4f;  // Global offset, settable by CMD_SET_TORQUE_OFFSET
float smoothedTorqueValue1 = 0.0f;
float smoothedTorqueValue2 = 0.0f;
bool firstTorqueReading1   = true;
bool firstTorqueReading2   = true;
bool motorsAreEnabled      = false;
uint32_t pulsesPerRev      = 800;    // Used for calculations, ensure consistency
int32_t machineStepCounter   = 0;
int32_t cartridgeStepCounter = 0;
int32_t accelerationLimit  = 100000; // pulses/sec^2
int32_t velocityLimit      = 10000;  // pulses/sec

// Define default/reference torque limits (declared extern in motor.h)
// These are not directly used by moveMotors if commands always supply a torque limit.
// They can serve as defaults for the GUI or other internal logic.
float jogTorqueLimit    = 30.0f;
float homingTorqueLimit = 20.0f;
float feedTorqueLimit   = 15.0f;

int32_t machineHomeReferenceSteps = 0;
int32_t cartridgeHomeReferenceSteps = 0;


void SetupMotors(void)
{
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);
	ConnectorM0.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM0.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM0.VelMax(velocityLimit);
	ConnectorM0.AccelMax(accelerationLimit);
	ConnectorM0.EnableRequest(true);

	ConnectorM1.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM1.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM1.VelMax(velocityLimit);
	ConnectorM1.AccelMax(accelerationLimit);
	ConnectorM1.EnableRequest(true);

	Delay_ms(100);
	if(ConnectorM0.HlfbState() == MotorDriver::HLFB_ASSERTED || ConnectorM1.HlfbState() == MotorDriver::HLFB_ASSERTED) motorsAreEnabled = true;
	Delay_ms(100);
}

void enableMotors(const char* reason_message) {
	ConnectorM0.EnableRequest(true);
	ConnectorM1.EnableRequest(true);
	motorsAreEnabled = true; // Set flag AFTER attempting to enable

	// Wait for HLFB to assert, indicating motors are ready
	uint32_t enableTimeout = Milliseconds() + 2000; // 2 second timeout
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED ||
	ConnectorM1.HlfbState() != MotorDriver::HLFB_ASSERTED) {
		if (Milliseconds() > enableTimeout) {
			sendToPC("Error: Timeout waiting for motors to enable (HLFB).");
			motorsAreEnabled = false; // Failed to enable
			// Optionally set an error state in SystemStates
			return;
		}
		Delay_ms(10); // Brief delay while polling
	}
	
	sendToPC(reason_message);
	// Reset torque smoothing on enable
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
}

void disableMotors(const char* reason_message){
	if (!motorsAreEnabled) return;
	
	ConnectorM0.EnableRequest(false);
	ConnectorM1.EnableRequest(false);
	motorsAreEnabled = false;
	sendToPC(reason_message);

	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
	Delay_ms(50);
}

void abortMove(){
	ConnectorM0.MoveStopAbrupt();
	ConnectorM1.MoveStopAbrupt();
}

void moveMotors(int stepsM0, int stepsM1, int torque_limit_percent, int velocity_sps, int accel_sps2) {
	if (!motorsAreEnabled) {
		sendToPC("MOVE BLOCKED: Motors are disabled.");
		return;
	}

	// Validate parameters (basic examples)
	if (torque_limit_percent < 0 || torque_limit_percent > 100) {
		sendToPC("Error: Invalid torque limit for moveMotors. Using default.");
		torque_limit_percent = (int)globalTorqueLimit; // Fallback or a safe default
	}
	if (velocity_sps <= 0 || velocity_sps > 10000) {
		sendToPC("Error: Invalid velocity for moveMotors. Using default max.");
		velocity_sps = velocityLimit; // Or a safe default
	}
	if (accel_sps2 <= 0 || velocity_sps > 100000) {
		sendToPC("Error: Invalid acceleration for moveMotors. Using default max.");
		accel_sps2 = accelerationLimit; // Or a safe default
	}
	
	// Set the globalTorqueLimit that checkTorqueLimit() will use for this operation
	globalTorqueLimit = (float)torque_limit_percent;
	
	// Set speed and acceleration for this specific move
	// These will cap at the motor's VelMax/AccelMax if higher.
	ConnectorM0.VelMax(velocity_sps);
	ConnectorM1.VelMax(velocity_sps);
	ConnectorM0.AccelMax(accel_sps2); 
	ConnectorM1.AccelMax(accel_sps2);
	
	char msg[128]; // Reduced size, only sending essential info
	snprintf(msg, sizeof(msg), "moveMotors: M0s:%d M1s:%d TqL: %d%% V:%d A:%d",
	stepsM0, stepsM1, torque_limit_percent, velocity_sps, accel_sps2);
	sendToPC(msg);
	
	// Initiate moves
	if (stepsM0 != 0) ConnectorM0.Move(stepsM0);
	if (stepsM1 != 0) ConnectorM1.Move(stepsM1);
}

bool checkMoving(void)
{
	if ((ConnectorM0.StepsComplete() && ConnectorM0.HlfbState() == MotorDriver::HLFB_ASSERTED) &&
	(ConnectorM1.StepsComplete() && ConnectorM1.HlfbState() == MotorDriver::HLFB_ASSERTED)) {
		return false; // Both are NOT moving
	}
	return true; // At least one is still moving
}

float getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead)
{
	float currentRawTorque = motor->HlfbPercent();
	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return TORQUE_SENTINEL_INVALID_VALUE;
	}
	if (*firstRead) {
		*smoothedValue = currentRawTorque;
		*firstRead = false;
		} else {
		*smoothedValue = EWMA_ALPHA * currentRawTorque + (1.0f - EWMA_ALPHA) * (*smoothedValue);
	}
	float adjusted = *smoothedValue + torqueOffset;
	return adjusted;
}

bool checkTorqueLimit(void){
	if (motorsAreEnabled && checkMoving()) {
		float currentSmoothedTorque1 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue1, &firstTorqueReading1);
		float currentSmoothedTorque2 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue2, &firstTorqueReading2);

		// The operational_offset from commands like JOG_MOVE, HOME_MOVE etc. is not yet used here.

		bool m0_over_limit = (currentSmoothedTorque1 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(currentSmoothedTorque1) > globalTorqueLimit);
		bool m1_over_limit = (currentSmoothedTorque2 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(currentSmoothedTorque2) > globalTorqueLimit);

		if (m0_over_limit || m1_over_limit) {
			abortMove();
			Delay_ms(200);
			sendToPC("TORQUE LIMIT REACHED, MOTION ABORTED");
			return true; // Limit exceeded
		}
	}
	return false; // Limit not exceeded or not applicable
}