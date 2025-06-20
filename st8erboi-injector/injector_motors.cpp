// ===========================// ============================================================================
// injector_motors.cpp
//
// Motor control routines for the Injector class.
//
// Provides motion primitives and safety wrappers for dual-axis injector motors,
// including torque-limited moves, abrupt stops, pinch valve control,
// and real-time torque monitoring using an EWMA filter.
//
// Includes HLFB monitoring, runtime enable/disable logic, and
// safe configuration for velocity and acceleration bounds.
//
// Corresponds to declarations in `injector.h`.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#include "injector.h"

#define EWMA_ALPHA 0.2f
#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f

void Injector::setupInjectorMotors(void){
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);
	ConnectorM0.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM0.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM0.VelMax(Injector.velocityLimit);
	ConnectorM0.AccelMax(Injector.acc);
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

void Injector::enableInjectorMotors(const char* reason_message) {
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
			// Optionally set an error state in Injector
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

void Injector::disableInjectorMotors(const char* reason_message){
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

void Injector::abortInjectorMove(){
	ConnectorM0.MoveStopAbrupt();
	ConnectorM1.MoveStopAbrupt();
}

void Injector::moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit_percent, int velocity_sps, int accel_sps2) {
	if (!motorsAreEnabled) {
		sendToPC("MOVE BLOCKED: Motors are disabled.");
		return;
	}

	// Validate parameters (basic examples)
	if (torque_limit_percent < 0 || torque_limit_percent > 100) {
		sendToPC("Error: Invalid torque limit for moveInjectorMotors. Using default.");
		torque_limit_percent = (int)injectorMotorsTorqueLimit; // Fallback or a safe default
	}
	if (velocity_sps <= 0 || velocity_sps > 10000) {
		sendToPC("Error: Invalid velocity for moveInjectorMotors. Using default max.");
		velocity_sps = velocityLimit; // Or a safe default
	}
	if (accel_sps2 <= 0 || velocity_sps > 100000) {
		sendToPC("Error: Invalid acceleration for moveInjectorMotors. Using default max.");
		accel_sps2 = accelerationLimit; // Or a safe default
	}
	
	// Set the injectorMotorsTorqueLimit that checkInjectorTorqueLimit() will use for this operation
	injectorMotorsTorqueLimit = (float)torque_limit_percent;
	
	// Set speed and acceleration for this specific move
	// These will cap at the motor's VelMax/AccelMax if higher.
	ConnectorM0.VelMax(velocity_sps);
	ConnectorM1.VelMax(velocity_sps);
	ConnectorM0.AccelMax(accel_sps2);
	ConnectorM1.AccelMax(accel_sps2);
	
	char msg[128]; // Reduced size, only sending essential info
	snprintf(msg, sizeof(msg), "moveInjectorMotors: M0s:%d M1s:%d TqL: %d%% V:%d A:%d",
	stepsM0, stepsM1, torque_limit_percent, velocity_sps, accel_sps2);
	sendToPC(msg);
	
	// Initiate moves
	if (stepsM0 != 0) ConnectorM0.Move(stepsM0);
	if (stepsM1 != 0) ConnectorM1.Move(stepsM1);
}

void Injector::movePinchMotor(int stepsM3, int stepsM1, int torque_limit_percent, int velocity_sps, int accel_sps2) {
	if (!motorsAreEnabled) {
		sendToPC("MOVE BLOCKED: Motors are disabled.");
		return;
	}

	// Validate parameters (basic examples)
	if (torque_limit_percent < 0 || torque_limit_percent > 100) {
		sendToPC("Error: Invalid torque limit for moveInjectorMotors. Using default.");
		torque_limit_percent = (int)injectorMotorsTorqueLimit; // Fallback or a safe default
	}
	if (velocity_sps <= 0 || velocity_sps > 10000) {
		sendToPC("Error: Invalid velocity for moveInjectorMotors. Using default max.");
		velocity_sps = velocityLimit; // Or a safe default
	}
	if (accel_sps2 <= 0 || velocity_sps > 100000) {
		sendToPC("Error: Invalid acceleration for moveInjectorMotors. Using default max.");
		accel_sps2 = accelerationLimit; // Or a safe default
	}
	
	// Set the injectorMotorsTorqueLimit that checkInjectorTorqueLimit() will use for this operation
	injectorMotorsTorqueLimit = (float)torque_limit_percent;
	
	// Set speed and acceleration for this specific move
	// These will cap at the motor's VelMax/AccelMax if higher.
	ConnectorM3.VelMax(velocity_sps);
	ConnectorM3.AccelMax(accel_sps2);
	
	char msg[128]; // Reduced size, only sending essential info
	snprintf(msg, sizeof(msg), "moveInjectorMotors: M3s:%d TqL: %d%% V:%d A:%d",
	stepsM3, torque_limit_percent, velocity_sps, accel_sps2);
	sendToPC(msg);
	
	// Initiate moves
	if (stepsM3 != 0) ConnectorM0.Move(stepsM3);
}

bool Injector::checkInjectorMoving(void)
{
	if ((ConnectorM0.StepsComplete() && ConnectorM0.HlfbState() == MotorDriver::HLFB_ASSERTED) &&
	(ConnectorM1.StepsComplete() && ConnectorM1.HlfbState() == MotorDriver::HLFB_ASSERTED)) {
		return false; // Both are NOT moving
	}
	return true; // At least one is still moving
}

float Injector::getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead)
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
	float adjusted = *smoothedValue + injectorMotorsTorqueOffset;
	return adjusted;
}

bool Injector::checkInjectorTorqueLimit(void){
	if (motorsAreEnabled && checkInjectorMoving()) {
		float currentSmoothedTorque1 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue1, &firstTorqueReading1);
		float currentSmoothedTorque2 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue2, &firstTorqueReading2);

		// The operational_offset from commands like JOG_MOVE, HOME_MOVE etc. is not yet used here.

		bool m0_over_limit = (currentSmoothedTorque1 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(currentSmoothedTorque1) > injectorMotorsTorqueLimit);
		bool m1_over_limit = (currentSmoothedTorque2 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(currentSmoothedTorque2) > injectorMotorsTorqueLimit);

		if (m0_over_limit || m1_over_limit) {
			abortInjectorMove();
			Delay_ms(200);
			sendToPC("TORQUE LIMIT REACHED, MOTION ABORTED");
			return true; // Limit exceeded
		}
	}
	return false; // Limit not exceeded or not applicable
}