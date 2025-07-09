// ============================================================================
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

void Injector::setupMotors(void) {
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);

	// Setup for M0 (Injector Axis)
	ConnectorM0.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM0.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM0.VelMax(25000);
	ConnectorM0.AccelMax(100000);
	ConnectorM0.EnableRequest(true);

	// Setup for M1 (Injector Axis)
	ConnectorM1.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM1.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM1.VelMax(25000);
	ConnectorM1.AccelMax(100000);
	ConnectorM1.EnableRequest(true);

	// Setup for M2 (Pinch Motor)
	ConnectorM2.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM2.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM2.VelMax(25000);
	ConnectorM2.AccelMax(100000);
	ConnectorM2.EnableRequest(true);

	Delay_ms(100);
	// Initial check for enabled status. Note: telemetry provides live, per-motor status.
	if (ConnectorM0.HlfbState() == MotorDriver::HLFB_ASSERTED || ConnectorM1.HlfbState() == MotorDriver::HLFB_ASSERTED) {
		motorsAreEnabled = true;
	}
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
			sendStatus(STATUS_PREFIX_INFO, "Error: Timeout waiting for motors to enable (HLFB).");
			motorsAreEnabled = false; // Failed to enable
			// Optionally set an error state in Injector
			return;
		}
		Delay_ms(10); // Brief delay while polling
	}
	
	sendStatus(STATUS_PREFIX_INFO, reason_message);
	// Reset torque smoothing on enable for all motors
	smoothedTorqueValue0 = 0.0f;
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading0 = true;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
}

void Injector::disableInjectorMotors(const char* reason_message){
	if (!motorsAreEnabled) return;
	
	ConnectorM0.EnableRequest(false);
	ConnectorM1.EnableRequest(false);
	motorsAreEnabled = false;
	sendStatus(STATUS_PREFIX_INFO, reason_message);

	// Reset torque smoothing on disable for all motors
	smoothedTorqueValue0 = 0.0f;
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading0 = true;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
	Delay_ms(50);
}

void Injector::abortInjectorMove(){
	ConnectorM0.MoveStopAbrupt();
	ConnectorM1.MoveStopAbrupt();
	ConnectorM2.MoveStopAbrupt(); // Also abort pinch motor
}

void Injector::moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit_percent, int velocity_sps, int accel_sps2) {
	if (!motorsAreEnabled) {
		sendStatus(STATUS_PREFIX_INFO, "MOVE BLOCKED: Motors are disabled.");
		return;
	}

	// Validate parameters (basic examples)
	if (torque_limit_percent < 0 || torque_limit_percent > 100) {
		sendStatus(STATUS_PREFIX_INFO, "Error: Invalid torque limit for moveInjectorMotors. Using default.");
		torque_limit_percent = (int)injectorMotorsTorqueLimit; // Fallback or a safe default
	}
	if (velocity_sps <= 0 || velocity_sps > 25000) { // Use motor's actual max
		sendStatus(STATUS_PREFIX_INFO, "Error: Invalid velocity for moveInjectorMotors. Using default.");
		velocity_sps = 1000; // Or a safe default
	}
	if (accel_sps2 <= 0 || accel_sps2 > 100000) { // Use motor's actual max
		sendStatus(STATUS_PREFIX_INFO, "Error: Invalid acceleration for moveInjectorMotors. Using default.");
		accel_sps2 = 10000; // Or a safe default
	}
	
	// Set the injectorMotorsTorqueLimit that checkInjectorTorqueLimit() will use for this operation
	injectorMotorsTorqueLimit = (float)torque_limit_percent;
	
	// Set speed and acceleration for this specific move
	ConnectorM0.VelMax(velocity_sps);
	ConnectorM1.VelMax(velocity_sps);
	ConnectorM0.AccelMax(accel_sps2);
	ConnectorM1.AccelMax(accel_sps2);
	
	char msg[128];
	snprintf(msg, sizeof(msg), "moveInjectorMotors: M0s:%d M1s:%d TqL: %d%% V:%d A:%d",
	stepsM0, stepsM1, torque_limit_percent, velocity_sps, accel_sps2);
	sendStatus(STATUS_PREFIX_INFO, msg);
	
	// Initiate moves
	if (stepsM0 != 0) ConnectorM0.Move(stepsM0);
	if (stepsM1 != 0) ConnectorM1.Move(stepsM1);
}

void Injector::movePinchMotor(int stepsM2, int torque_limit_percent, int velocity_sps, int accel_sps2) {
	if (ConnectorM2.HlfbState() != MotorDriver::HLFB_ASSERTED) {
		sendStatus(STATUS_PREFIX_INFO, "MOVE BLOCKED: Pinch motor is disabled.");
		return;
	}

	// Validate parameters
	if (torque_limit_percent < 0 || torque_limit_percent > 100) {
		sendStatus(STATUS_PREFIX_INFO, "Error: Invalid torque limit for movePinchMotor. Using default.");
		torque_limit_percent = 30; // Safe default
	}
	if (velocity_sps <= 0) {
		sendStatus(STATUS_PREFIX_INFO, "Error: Invalid velocity for movePinchMotor. Using default.");
		velocity_sps = 1000; // Safe default
	}
	if (accel_sps2 <= 0) {
		sendStatus(STATUS_PREFIX_INFO, "Error: Invalid acceleration for movePinchMotor. Using default.");
		accel_sps2 = 10000; // Safe default
	}
	
	// Set the torque limit that checkInjectorTorqueLimit() will use for this operation
	injectorMotorsTorqueLimit = (float)torque_limit_percent;
	
	// Set speed and acceleration for this specific move
	ConnectorM2.VelMax(velocity_sps);
	ConnectorM2.AccelMax(accel_sps2);
	
	char msg[128];
	snprintf(msg, sizeof(msg), "movePinchMotor: M2s:%d TqL:%d%% V:%d A:%d",
	stepsM2, torque_limit_percent, velocity_sps, accel_sps2);
	sendStatus(STATUS_PREFIX_INFO, msg);
	
	// Initiate the move on the correct motor
	if (stepsM2 != 0) {
		ConnectorM2.Move(stepsM2);
	}
}

bool Injector::checkInjectorMoving(void)
{
	// Return true if ANY of the three motors are currently moving.
	bool m0_moving = !ConnectorM0.StepsComplete() && ConnectorM0.StatusReg().bit.Enabled;
	bool m1_moving = !ConnectorM1.StepsComplete() && ConnectorM1.StatusReg().bit.Enabled;
	bool m2_moving = !ConnectorM2.StepsComplete() && ConnectorM2.StatusReg().bit.Enabled;
	
	return m0_moving || m1_moving || m2_moving;
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
		*smoothedValue = EWMA_ALPHA_TORQUE * currentRawTorque + (1.0f - EWMA_ALPHA_TORQUE) * (*smoothedValue);
	}
	float adjusted = *smoothedValue + injectorMotorsTorqueOffset;
	return adjusted;
}

bool Injector::checkInjectorTorqueLimit(void){
	if (checkInjectorMoving()) {
		// Correctly get smoothed torque for all three motors
		float torque0 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue0, &firstTorqueReading0);
		float torque1 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue1, &firstTorqueReading1);
		float torque2 = getSmoothedTorqueEWMA(&ConnectorM2, &smoothedTorqueValue2, &firstTorqueReading2);

		// Check if any moving motor is over the current torque limit for the operation
		bool m0_over_limit = !ConnectorM0.StepsComplete() && (torque0 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(torque0) > injectorMotorsTorqueLimit);
		bool m1_over_limit = !ConnectorM1.StepsComplete() && (torque1 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(torque1) > injectorMotorsTorqueLimit);
		bool m2_over_limit = !ConnectorM2.StepsComplete() && (torque2 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(torque2) > injectorMotorsTorqueLimit);

		if (m0_over_limit || m1_over_limit || m2_over_limit) {
			abortInjectorMove();
			Delay_ms(200);
			
			char torque_msg[128];
			snprintf(torque_msg, sizeof(torque_msg), "TORQUE LIMIT REACHED (%.1f%%). M0:%.1f, M1:%.1f, M2:%.1f",
			injectorMotorsTorqueLimit, torque0, torque1, torque2);
			sendStatus(STATUS_PREFIX_INFO, torque_msg);

			return true; // Limit exceeded
		}
	}
	return false; // Limit not exceeded or not applicable
}
