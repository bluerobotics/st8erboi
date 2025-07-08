// ============================================================================
// injector_handlers.cpp
//
// Implementation of mode and message handler routines for the Injector class.
//
// Provides logic for executing incoming commands such as ENABLE, DISABLE,
// JOG_MOVE, HOMING_MODE, INJECT_MOVE, etc., and coordinates hardware safety,
// motion startup, and state transitions.
//
// Handlers are triggered via UDP commands and interact directly with the
// injector state machine and motor interfaces.
//
// Corresponds to declarations in `injector.h`.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#include "injector.h"

void Injector::handleEnable() {
	if (mainState == DISABLED_MODE) {
		enableInjectorMotors("MOTORS ENABLED (transitioned to STANDBY_MODE)");
		mainState = STANDBY_MODE;
		homingState = HOMING_NONE;
		feedState = FEED_NONE;
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_INFO, "System enabled: state is now STANDBY_MODE.");
		} else {
		// If already enabled, or in a state other than DISABLED_MODE,
		// just ensure motors are enabled without changing main state.
		if (!motorsAreEnabled) {
			enableInjectorMotors("MOTORS re-enabled (state unchanged)");
			} else {
			sendStatus(STATUS_PREFIX_INFO, "Motors already enabled, system not in DISABLED_MODE.");
		}
	}
}

void Injector::handleDisable()
{
	abortInjectorMove();
	Delay_ms(200);
	mainState = DISABLED_MODE;
	errorState = ERROR_NONE;
	disableInjectorMotors("MOTORS DISABLED (entered DISABLED state)");
	sendStatus(STATUS_PREFIX_INFO, "System disabled: must ENABLE to return to standby.");
}

void Injector::handleAbort() {
	sendStatus(STATUS_PREFIX_INFO, "ABORT received. Stopping motion and resetting...");

	abortInjectorMove();
	Delay_ms(200);

	handleStandbyMode();
}

void Injector::handleClearErrors() {
	sendStatus(STATUS_PREFIX_INFO, "CLEAR_ERRORS received. Resetting system...");
	handleStandbyMode();
}


void Injector::handleStandbyMode() {
	bool wasError = (errorState != ERROR_NONE); // Check if error was active
	
	if (mainState != STANDBY_MODE) { // Only log/act if changing state
		abortInjectorMove(); // Good practice to stop motion when changing to standby
		Delay_ms(200);
		mainState = STANDBY_MODE;
		homingState = HOMING_NONE;
		currentHomingPhase = HOMING_PHASE_IDLE;
		feedState = FEED_STANDBY;
		errorState = ERROR_NONE;

		if (wasError) {
			sendStatus(STATUS_PREFIX_INFO, "Exited previous state: State set to STANDBY_MODE and error cleared.");
			} else {
			sendStatus(STATUS_PREFIX_INFO, "State set to STANDBY_MODE.");
		}
		} else {
		if (errorState != ERROR_NONE) { // If already in standby but error occurs and then standby is requested again
			errorState = ERROR_NONE;
			sendStatus(STATUS_PREFIX_INFO, "Still in STANDBY_MODE, error cleared.");
			} else {
			sendStatus(STATUS_PREFIX_INFO, "Already in STANDBY_MODE.");
		}
	}
}

void Injector::handleJogMode() {
	if (mainState != JOG_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		mainState = JOG_MODE;
		homingState = HOMING_NONE; // Reset other mode sub-injector
		feedState = FEED_NONE;
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_INFO, "Entered JOG_MODE. Ready for JOG_MOVE commands.");
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Already in JOG_MODE.");
	}
}

void Injector::handleHomingMode() {
	if (mainState != HOMING_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		mainState = HOMING_MODE;
		homingState = HOMING_NONE; // Initialize to no specific homing action
		feedState = FEED_NONE;
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_INFO, "Entered HOMING_MODE. Ready for homing operations.");
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Already in HOMING_MODE.");
	}
}

void Injector::handleFeedMode() {
	if (mainState != FEED_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		mainState = FEED_MODE;
		feedState = FEED_STANDBY;
		homingState = HOMING_NONE; // Reset other mode sub-injector
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_INFO, "Entered FEED_MODE. Ready for inject/purge/retract.");
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Already in FEED_MODE.");
	}
}

void Injector::handleSetinjectorMotorsTorqueOffset(const char *msg) {
	float newOffset = atof(msg + strlen(CMD_STR_SET_INJECTOR_TORQUE_OFFSET));
	injectorMotorsTorqueOffset = newOffset;
	char response[64];
	snprintf(response, sizeof(response), "Global torque offset set to %.2f", injectorMotorsTorqueOffset);
	sendStatus(STATUS_PREFIX_INFO, response);
}

void Injector::handleJogMove(const char* msg) {
	if (mainState != JOG_MODE) {
		sendStatus(STATUS_PREFIX_INFO, "JOG_MOVE ignored: Not in JOG_MODE.");
		return;
	}

	float dist_mm1 = 0, dist_mm2 = 0, vel_mms = 0, accel_mms2 = 0;
	int torque_percent = 0;

	int parsed_count = sscanf(msg + strlen(CMD_STR_JOG_MOVE), "%f %f %f %f %d",
	&dist_mm1, &dist_mm2, &vel_mms, &accel_mms2, &torque_percent);

	if (parsed_count == 5) {
		char response[200];
		snprintf(response, sizeof(response), "JOG_MOVE RX: M0:%.2fmm M1:%.2fmm Vel:%.1fmm/s Acc:%.1fmm/s2 Tq:%d%%",
		dist_mm1, dist_mm2, vel_mms, accel_mms2, torque_percent);
		sendStatus(STATUS_PREFIX_INFO, response);

		if (!motorsAreEnabled) {
			sendStatus(STATUS_PREFIX_INFO, "JOG_MOVE blocked: Motors are disabled.");
			return;
		}

		// Parameter validation
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 30;
		if (vel_mms <= 0) vel_mms = 2.0f; // Default 2.0 mm/s
		if (accel_mms2 <= 0) accel_mms2 = 10.0f; // Default 10.0 mm/s^2

		// Convert metric units to step-based units for the motor driver
		long steps1 = (long)(dist_mm1 * STEPS_PER_MM_M0);
		long steps2 = (long)(dist_mm2 * STEPS_PER_MM_M1);
		int velocity_sps = (int)(vel_mms * STEPS_PER_MM_M0); // Assuming M0/M1 have same mechanics
		int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_M0);

		moveInjectorMotors(steps1, steps2, torque_percent, velocity_sps, accel_sps2_val);
		jogDone = false;
		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		sendStatus(STATUS_PREFIX_INFO, errorMsg);
	}
}

void Injector::handleMachineHomeMove(const char *msg) {
	if (mainState != HOMING_MODE) {
		sendStatus(STATUS_PREFIX_INFO, "MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (currentHomingPhase != HOMING_PHASE_IDLE && currentHomingPhase != HOMING_PHASE_COMPLETE && currentHomingPhase != HOMING_PHASE_ERROR) {
		sendStatus(STATUS_PREFIX_INFO, "MACHINE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}

	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	
	int parsed_count = sscanf(msg + strlen(CMD_STR_MACHINE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250];
		snprintf(response, sizeof(response), "MACHINE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendStatus(STATUS_PREFIX_INFO, response);

		if (PITCH_MM_PER_REV <= 0.0f) {
			sendStatus(STATUS_PREFIX_INFO, "FATAL HOMING ERROR: PITCH_MM_PER_REV is not set or is zero. Cannot calculate steps.");
			homingState = HOMING_MACHINE;
			currentHomingPhase = HOMING_PHASE_ERROR;
			errorState = ERROR_INVALID_PARAMETERS;
			return;
		}

		if (!motorsAreEnabled) {
			sendStatus(STATUS_PREFIX_INFO, "MACHINE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			homingState = HOMING_MACHINE;
			currentHomingPhase = HOMING_PHASE_ERROR;
			errorState = ERROR_MOTORS_DISABLED;
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendStatus(STATUS_PREFIX_INFO, "Warning: Invalid torque for Machine Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendStatus(STATUS_PREFIX_INFO, "Error: Invalid parameters for Machine Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			homingState = HOMING_MACHINE;
			currentHomingPhase = HOMING_PHASE_ERROR;
			errorState = ERROR_INVALID_PARAMETERS;
			return;
		}

		// Store parameters in Injector
		homing_stroke_mm_param = stroke_mm;
		homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		homing_touch_vel_mm_s_param = touch_vel_mm_s;
		homing_acceleration_param = acceleration_mm_s2;
		homing_retract_mm_param = retract_mm;
		homing_torque_percent_param = torque_percent;

		// Calculate and store step/sps values
		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV;
		homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		// Initialize homing state machine
		homingState = HOMING_MACHINE;
		currentHomingPhase = HOMING_PHASE_STARTING_MOVE;
		homingStartTime = Milliseconds(); // For timeout tracking
		errorState = ERROR_NONE; // Clear previous errors

		sendStatus(STATUS_PREFIX_INFO, "Initiating Machine Homing: STARTING_MOVE phase.");
		long initial_move_steps = -1 * homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);
		
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
		homingState = HOMING_MACHINE;
		currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void Injector::handleCartridgeHomeMove(const char *msg) {
	if (mainState != HOMING_MODE) {
		sendStatus(STATUS_PREFIX_INFO, "CARTRIDGE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (currentHomingPhase != HOMING_PHASE_IDLE && currentHomingPhase != HOMING_PHASE_COMPLETE && currentHomingPhase != HOMING_PHASE_ERROR) {
		sendStatus(STATUS_PREFIX_INFO, "CARTRIDGE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}
	
	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	int parsed_count = sscanf(msg + strlen(CMD_STR_CARTRIDGE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250];
		snprintf(response, sizeof(response), "CARTRIDGE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendStatus(STATUS_PREFIX_INFO, response);

		if (PITCH_MM_PER_REV <= 0.0f) {
			sendStatus(STATUS_PREFIX_INFO, "FATAL HOMING ERROR: PITCH_MM_PER_REV is not set or is zero. Cannot calculate steps.");
			homingState = HOMING_CARTRIDGE;
			currentHomingPhase = HOMING_PHASE_ERROR;
			errorState = ERROR_INVALID_PARAMETERS;
			return;
		}

		if (!motorsAreEnabled) {
			sendStatus(STATUS_PREFIX_INFO, "CARTRIDGE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			homingState = HOMING_CARTRIDGE;
			currentHomingPhase = HOMING_PHASE_ERROR;
			errorState = ERROR_MOTORS_DISABLED;
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendStatus(STATUS_PREFIX_INFO, "Warning: Invalid torque for Cartridge Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendStatus(STATUS_PREFIX_INFO, "Error: Invalid parameters for Cartridge Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			homingState = HOMING_CARTRIDGE;
			currentHomingPhase = HOMING_PHASE_ERROR;
			errorState = ERROR_INVALID_PARAMETERS;
			return;
		}

		homing_stroke_mm_param = stroke_mm;
		homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		homing_touch_vel_mm_s_param = touch_vel_mm_s;
		homing_acceleration_param = acceleration_mm_s2;
		homing_retract_mm_param = retract_mm;
		homing_torque_percent_param = torque_percent;

		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV;
		homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		homingState = HOMING_CARTRIDGE;
		currentHomingPhase = HOMING_PHASE_STARTING_MOVE;
		homingStartTime = Milliseconds();
		errorState = ERROR_NONE;

		sendStatus(STATUS_PREFIX_INFO, "Initiating Cartridge Homing: STARTING_MOVE phase.");
		long initial_move_steps = 1 * homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);

		} else {
		sendStatus(STATUS_PREFIX_INFO, "Invalid CARTRIDGE_HOME_MOVE format. Expected 6 parameters.");
		homingState = HOMING_CARTRIDGE;
		currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void Injector::handlePinchHomeMove() {
	if (mainState != HOMING_MODE) {
		sendStatus(STATUS_PREFIX_INFO, "PINCH_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (currentHomingPhase != HOMING_PHASE_IDLE && currentHomingPhase != HOMING_PHASE_COMPLETE && currentHomingPhase != HOMING_PHASE_ERROR) {
		sendStatus(STATUS_PREFIX_INFO, "PINCH_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}
	if (ConnectorM2.HlfbState() != MotorDriver::HLFB_ASSERTED) {
		sendStatus(STATUS_PREFIX_INFO, "PINCH_HOME_MOVE blocked: Pinch motor is disabled.");
		return;
	}

	// Hardcoded parameters for pinch homing
	long homing_steps = -800; // Move up to 1 revolution
	int velocity = 400;
	int acceleration = 5000;
	int torque_percent = 10;
	
	// Set up the state machine for pinch homing
	homingState = HOMING_PINCH;
	currentHomingPhase = HOMING_PHASE_STARTING_MOVE;
	homingStartTime = Milliseconds();
	errorState = ERROR_NONE;

	sendStatus(STATUS_PREFIX_INFO, "Initiating Pinch Homing...");
	movePinchMotor(homing_steps, torque_percent, velocity, acceleration);
}

void Injector::handlePinchJogMove(const char *msg) {
	if (mainState != JOG_MODE) {
		sendStatus(STATUS_PREFIX_INFO, "PINCH_JOG_MOVE ignored: Not in JOG_MODE.");
		return;
	}

	float degrees = 0;
	int torque_percent = 0, velocity_sps = 0, accel_sps2 = 0;

	int parsed_count = sscanf(msg + strlen(CMD_STR_PINCH_JOG_MOVE), "%f %d %d %d",
	&degrees, &torque_percent, &velocity_sps, &accel_sps2);

	if (parsed_count == 4) {
		char response[150];
		snprintf(response, sizeof(response), "PINCH_JOG_MOVE RX: deg:%.1f TqL:%d%% Vel:%d Acc:%d",
		degrees, torque_percent, velocity_sps, accel_sps2);
		sendStatus(STATUS_PREFIX_INFO, response);

		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 30;
		if (velocity_sps <= 0) velocity_sps = 800;
		if (accel_sps2 <= 0) accel_sps2 = 5000;

		const float steps_per_degree = 800.0f / 360.0f;
		long steps_to_move = (long)(degrees * steps_per_degree);

		movePinchMotor(steps_to_move, torque_percent, velocity_sps, accel_sps2);
		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid PINCH_JOG_MOVE format. Expected 4 params, got %d.", parsed_count);
		sendStatus(STATUS_PREFIX_ERROR, errorMsg);
	}
}

void Injector::handleEnablePinch() {
	sendStatus(STATUS_PREFIX_INFO, "Enabling Pinch Motor (M2)...");
	ConnectorM2.EnableRequest(true);
}

void Injector::handleDisablePinch() {
	sendStatus(STATUS_PREFIX_INFO, "Disabling Pinch Motor (M2)...");
	ConnectorM2.EnableRequest(false);
}

void Injector::handlePinchOpen(){
	// Implementation needed
}

void Injector::handlePinchClose(){
	// Implementation needed
}

// --- NEW: Handlers for heater and vacuum relays ---
void Injector::handleHeaterOn() {
	PIN_HEATER_RELAY.State(true); // CORRECTED from StateSet
	heaterOn = true;
	sendStatus(STATUS_PREFIX_INFO, "Heater relay turned ON.");
}

void Injector::handleHeaterOff() {
	PIN_HEATER_RELAY.State(false); // CORRECTED from StateSet
	heaterOn = false;
	sendStatus(STATUS_PREFIX_INFO, "Heater relay turned OFF.");
}

void Injector::handleVacuumOn() {
	PIN_VACUUM_RELAY.State(true); // CORRECTED from StateSet
	vacuumOn = true;
	sendStatus(STATUS_PREFIX_INFO, "Vacuum relay turned ON.");
}

void Injector::handleVacuumOff() {
	PIN_VACUUM_RELAY.State(false); // CORRECTED from StateSet
	vacuumOn = false;
	sendStatus(STATUS_PREFIX_INFO, "Vacuum relay turned OFF.");
}

void Injector::handleMoveToCartridgeHome() {
	if (mainState != FEED_MODE) { return; }
	if (!homingCartridgeDone) { errorState = ERROR_NO_CARTRIDGE_HOME; sendStatus(STATUS_PREFIX_INFO, "Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_INFO, "Err: Motors disabled."); return; }
	if (checkInjectorMoving() || feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE) {
		sendStatus(STATUS_PREFIX_INFO, "Err: Busy. Cannot move to cart home now.");
		errorState = ERROR_INVALID_INJECTION; return;
	}

	sendStatus(STATUS_PREFIX_INFO, "Cmd: Move to Cartridge Home...");
	fullyResetActiveDispenseOperation();
	feedState = FEED_MOVING_TO_HOME;
	feedingDone = false;
	
	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = cartridgeHomeReferenceSteps - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	feedDefaultTorquePercent, feedDefaultVelocitySPS, feedDefaultAccelSPS2);
}

void Injector::handleMoveToCartridgeRetract(const char *msg) {
	if (mainState != FEED_MODE) { return; }
	if (!homingCartridgeDone) { errorState = ERROR_NO_CARTRIDGE_HOME; sendStatus(STATUS_PREFIX_INFO, "Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_INFO, "Err: Motors disabled."); return; }
	if (checkInjectorMoving() || feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE) {
		sendStatus(STATUS_PREFIX_INFO, "Err: Busy. Cannot move to cart retract now.");
		errorState = ERROR_INVALID_INJECTION; return;
	}

	float offset_mm = 0.0f;
	if (sscanf(msg + strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT), "%f", &offset_mm) != 1 || offset_mm < 0) {
		sendStatus(STATUS_PREFIX_INFO, "Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
	}
	
	fullyResetActiveDispenseOperation();
	feedState = FEED_MOVING_TO_RETRACT;
	feedingDone = false;

	const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV;
	long offset_steps = (long)(offset_mm * current_steps_per_mm);
	long target_absolute_axis_position = cartridgeHomeReferenceSteps + offset_steps;

	char response[150];
	snprintf(response, sizeof(response), "Cmd: Move to Cart Retract (Offset: %.1fmm, Target: %ld steps)", offset_mm, target_absolute_axis_position);
	sendStatus(STATUS_PREFIX_INFO, response);

	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = target_absolute_axis_position - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	feedDefaultTorquePercent, feedDefaultVelocitySPS, feedDefaultAccelSPS2);
}


void Injector::handleInjectMove(const char *msg) {
	if (mainState != FEED_MODE) {
		sendStatus(STATUS_PREFIX_INFO, "INJECT ignored: Not in FEED_MODE.");
		return;
	}
	if (checkInjectorMoving() || active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_INFO, "Error: Operation already in progress or motors busy.");
		errorState = ERROR_INVALID_INJECTION;
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	if (sscanf(msg + strlen(CMD_STR_INJECT_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_INFO, "INJECT blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendStatus(STATUS_PREFIX_INFO, "Error: Steps/ml must be positive and non-zero."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f;
		if (volume_ml <= 0) { sendStatus(STATUS_PREFIX_INFO, "Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendStatus(STATUS_PREFIX_INFO, "Error: Inject speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendStatus(STATUS_PREFIX_INFO, "Error: Inject acceleration must be positive."); return; }

		fullyResetActiveDispenseOperation();
		last_completed_dispense_ml = 0.0f;

		feedState = FEED_INJECT_STARTING;
		feedingDone = false;
		active_dispense_INJECTION_ongoing = true;
		active_op_target_ml = volume_ml;
		active_op_steps_per_ml = steps_per_ml_val;
		active_op_total_target_steps = (long)(volume_ml * active_op_steps_per_ml);
		active_op_remaining_steps = active_op_total_target_steps;
		
		active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		active_op_segment_initial_axis_steps = active_op_initial_axis_steps;
		
		active_op_total_dispensed_ml = 0.0f;

		active_op_velocity_sps = (int)(speed_ml_s * active_op_steps_per_ml);
		active_op_accel_sps2 = (int)acceleration_sps2_param;
		active_op_torque_percent = (int)torque_percent;
		if (active_op_velocity_sps <= 0) active_op_velocity_sps = 100;

		char response[200];
		snprintf(response, sizeof(response), "RX INJECT: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendStatus(STATUS_PREFIX_INFO, response);
		sendStatus(STATUS_PREFIX_INFO, "Starting Inject operation...");

		moveInjectorMotors(active_op_remaining_steps, active_op_remaining_steps,
		active_op_torque_percent, active_op_velocity_sps, active_op_accel_sps2);
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Invalid INJECT_MOVE format. Expected 5 params.");
	}
}

void Injector::handlePurgeMove(const char *msg) {
	if (mainState != FEED_MODE) { sendStatus(STATUS_PREFIX_INFO, "PURGE ignored: Not in FEED_MODE."); return; }
	if (checkInjectorMoving() || active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_INFO, "Error: Operation already in progress or motors busy.");
		errorState = ERROR_INVALID_INJECTION;
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	if (sscanf(msg + strlen(CMD_STR_PURGE_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_INFO, "PURGE blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendStatus(STATUS_PREFIX_INFO, "Error: Steps/ml must be positive and non-zero."); return;}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f;
		if (volume_ml <= 0) { sendStatus(STATUS_PREFIX_INFO, "Error: Purge volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendStatus(STATUS_PREFIX_INFO, "Error: Purge speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendStatus(STATUS_PREFIX_INFO, "Error: Purge acceleration must be positive."); return; }

		fullyResetActiveDispenseOperation();
		last_completed_dispense_ml = 0.0f;

		feedState = FEED_PURGE_STARTING;
		feedingDone = false;
		active_dispense_INJECTION_ongoing = true;
		active_op_target_ml = volume_ml;
		active_op_steps_per_ml = steps_per_ml_val;
		active_op_total_target_steps = (long)(volume_ml * active_op_steps_per_ml);
		active_op_remaining_steps = active_op_total_target_steps;
		
		active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		active_op_segment_initial_axis_steps = active_op_initial_axis_steps;
		
		active_op_total_dispensed_ml = 0.0f;

		active_op_velocity_sps = (int)(speed_ml_s * active_op_steps_per_ml);
		active_op_accel_sps2 = (int)acceleration_sps2_param;
		active_op_torque_percent = (int)torque_percent;
		if (active_op_velocity_sps <= 0) active_op_velocity_sps = 200;

		char response[200];
		snprintf(response, sizeof(response), "RX PURGE: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendStatus(STATUS_PREFIX_INFO, response);
		sendStatus(STATUS_PREFIX_INFO, "Starting Purge operation...");
		
		moveInjectorMotors(active_op_remaining_steps, active_op_remaining_steps,
		active_op_torque_percent, active_op_velocity_sps, active_op_accel_sps2);
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Invalid PURGE_MOVE format. Expected 5 params.");
	}
}

void Injector::handlePauseOperation() {
	if (!active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_INFO, "PAUSE ignored: No active inject/purge operation.");
		return;
	}

	if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE) {
		sendStatus(STATUS_PREFIX_INFO, "Cmd: Pausing operation...");
		ConnectorM0.MoveStopDecel();
		ConnectorM1.MoveStopDecel();

		if (feedState == FEED_INJECT_ACTIVE) feedState = FEED_INJECT_PAUSED;
		if (feedState == FEED_PURGE_ACTIVE) feedState = FEED_PURGE_PAUSED;
		} else {
		sendStatus(STATUS_PREFIX_INFO, "PAUSE ignored: Not in an active inject/purge state.");
	}
}

void Injector::handleResumeOperation() {
	if (!active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: No operation was ongoing or paused.");
		return;
	}
	if (feedState == FEED_INJECT_PAUSED || feedState == FEED_PURGE_PAUSED) {
		if (active_op_remaining_steps <= 0) {
			sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: No remaining volume/steps to dispense.");
			fullyResetActiveDispenseOperation();
			feedState = FEED_STANDBY;
			return;
		}
		sendStatus(STATUS_PREFIX_INFO, "Cmd: Resuming operation...");
		active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		feedingDone = false;

		moveInjectorMotors(active_op_remaining_steps, active_op_remaining_steps,
		active_op_torque_percent, active_op_velocity_sps, active_op_accel_sps2);
		
		if(feedState == FEED_INJECT_PAUSED) feedState = FEED_INJECT_RESUMING;
		if(feedState == FEED_PURGE_PAUSED) feedState = FEED_PURGE_RESUMING;
		
		if(feedState == FEED_INJECT_RESUMING) feedState = FEED_INJECT_ACTIVE;
		if(feedState == FEED_PURGE_RESUMING) feedState = FEED_PURGE_ACTIVE;


		} else {
		sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: Operation not paused.");
	}
}

void Injector::handleCancelOperation() {
	if (!active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_INFO, "CANCEL ignored: No active inject/purge operation to cancel.");
		return;
	}

	sendStatus(STATUS_PREFIX_INFO, "Cmd: Cancelling current feed operation...");
	abortInjectorMove();
	Delay_ms(100);

	if (active_op_steps_per_ml > 0.0001f && active_dispense_INJECTION_ongoing) {
		long steps_moved_on_axis = ConnectorM0.PositionRefCommanded() - active_op_segment_initial_axis_steps;
		float segment_dispensed_ml = (float)fabs(steps_moved_on_axis) / active_op_steps_per_ml;
		active_op_total_dispensed_ml += segment_dispensed_ml;
	}
	
	last_completed_dispense_ml = 0.0f;

	fullyResetActiveDispenseOperation();
	
	feedState = FEED_INJECTION_CANCELLED;
	feedingDone = true;
	errorState = ERROR_NONE;

	sendStatus(STATUS_PREFIX_INFO, "Operation Cancelled. Dispensed value for this attempt will show as 0 ml in idle telemetry.");
}

void Injector::finalizeAndResetActiveDispenseOperation(bool operationCompletedSuccessfully) {
	if (active_dispense_INJECTION_ongoing) {
		if (active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / active_op_steps_per_ml;
			active_op_total_dispensed_ml += segment_dispensed_ml;
		}
		last_completed_dispense_ml = active_op_total_dispensed_ml;
	}
	
	active_dispense_INJECTION_ongoing = false;
	active_op_target_ml = 0.0f;
	active_op_remaining_steps = 0;
}

void Injector::fullyResetActiveDispenseOperation() {
	active_dispense_INJECTION_ongoing = false;
	active_op_target_ml = 0.0f;
	active_op_total_dispensed_ml = 0.0f;
	active_op_total_target_steps = 0;
	active_op_remaining_steps = 0;
	active_op_segment_initial_axis_steps = 0;
	active_op_initial_axis_steps = 0;
	active_op_steps_per_ml = 0.0f;
}
