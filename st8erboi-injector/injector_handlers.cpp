#include "injector.h"

void Injector::handleEnable() {
	if (mainState == DISABLED_MODE) {
		enableInjectorMotors("MOTORS ENABLED (transitioned to STANDBY_MODE)");
		mainState = STANDBY_MODE;
		homingState = HOMING_NONE;
		feedState = FEED_NONE;
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_DONE, "ENABLE complete. System in STANDBY_MODE.");
		} else {
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
	sendStatus(STATUS_PREFIX_DONE, "DISABLE complete.");
}

void Injector::handleAbort() {
	sendStatus(STATUS_PREFIX_INFO, "ABORT received. Stopping motion and resetting...");
	abortInjectorMove();
	Delay_ms(200);
	handleStandbyMode();
	sendStatus(STATUS_PREFIX_DONE, "ABORT complete.");
}

void Injector::handleClearErrors() {
	sendStatus(STATUS_PREFIX_INFO, "CLEAR_ERRORS received. Resetting system...");
	handleStandbyMode();
	sendStatus(STATUS_PREFIX_DONE, "CLEAR_ERRORS complete.");
}


void Injector::handleStandbyMode() {
	bool wasError = (errorState != ERROR_NONE);
	
	if (mainState != STANDBY_MODE) {
		abortInjectorMove();
		Delay_ms(200);
		mainState = STANDBY_MODE;
		homingState = HOMING_NONE;
		currentHomingPhase = HOMING_PHASE_IDLE;
		feedState = FEED_STANDBY;
		errorState = ERROR_NONE;

		if (wasError) {
			sendStatus(STATUS_PREFIX_DONE, "STANDBY_MODE set and error cleared.");
			} else {
			sendStatus(STATUS_PREFIX_DONE, "STANDBY_MODE set.");
		}
		} else {
		if (errorState != ERROR_NONE) {
			errorState = ERROR_NONE;
			sendStatus(STATUS_PREFIX_DONE, "Error cleared, still in STANDBY_MODE.");
			} else {
			sendStatus(STATUS_PREFIX_INFO, "Already in STANDBY_MODE.");
		}
	}
}

void Injector::handleJogMode() {
	if (mainState != JOG_MODE) {
		abortInjectorMove();
		Delay_ms(200);
		mainState = JOG_MODE;
		homingState = HOMING_NONE;
		feedState = FEED_NONE;
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_DONE, "JOG_MODE set.");
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Already in JOG_MODE.");
	}
}

void Injector::handleHomingMode() {
	if (mainState != HOMING_MODE) {
		abortInjectorMove();
		Delay_ms(200);
		mainState = HOMING_MODE;
		homingState = HOMING_NONE;
		feedState = FEED_NONE;
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_DONE, "HOMING_MODE set.");
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Already in HOMING_MODE.");
	}
}

void Injector::handleFeedMode() {
	if (mainState != FEED_MODE) {
		abortInjectorMove();
		Delay_ms(200);
		mainState = FEED_MODE;
		feedState = FEED_STANDBY;
		homingState = HOMING_NONE;
		errorState = ERROR_NONE;
		sendStatus(STATUS_PREFIX_DONE, "FEED_MODE set.");
		} else {
		sendStatus(STATUS_PREFIX_INFO, "Already in FEED_MODE.");
	}
}

void Injector::handleSetinjectorMotorsTorqueOffset(const char *msg) {
	float newOffset = atof(msg + strlen(CMD_STR_SET_INJECTOR_TORQUE_OFFSET));
	injectorMotorsTorqueOffset = newOffset;
	char response[64];
	snprintf(response, sizeof(response), "Global torque offset set to %.2f", injectorMotorsTorqueOffset);
	sendStatus(STATUS_PREFIX_DONE, response);
}

void Injector::handleJogMove(const char* msg) {
	if (mainState != JOG_MODE) {
		sendStatus(STATUS_PREFIX_ERROR, "JOG_MOVE ignored: Not in JOG_MODE.");
		return;
	}

	float dist_mm1 = 0, dist_mm2 = 0, vel_mms = 0, accel_mms2 = 0;
	int torque_percent = 0;

	int parsed_count = sscanf(msg + strlen(CMD_STR_JOG_MOVE), "%f %f %f %f %d",
	&dist_mm1, &dist_mm2, &vel_mms, &accel_mms2, &torque_percent);

	if (parsed_count == 5) {
		if (!motorsAreEnabled) {
			sendStatus(STATUS_PREFIX_ERROR, "JOG_MOVE blocked: Motors are disabled.");
			return;
		}

		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 30;
		if (vel_mms <= 0) vel_mms = 2.0f;
		if (accel_mms2 <= 0) accel_mms2 = 10.0f;

		long steps1 = (long)(dist_mm1 * STEPS_PER_MM_M0);
		long steps2 = (long)(dist_mm2 * STEPS_PER_MM_M1);
		int velocity_sps = (int)(vel_mms * STEPS_PER_MM_M0);
		int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_M0);
		
		activeJogCommand = CMD_STR_JOG_MOVE; // FIX: Track active command
		moveInjectorMotors(steps1, steps2, torque_percent, velocity_sps, accel_sps2_val);
		jogDone = false;
		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		sendStatus(STATUS_PREFIX_ERROR, errorMsg);
	}
}

void Injector::handleMachineHomeMove(const char *msg) {
	if (mainState != HOMING_MODE) {
		sendStatus(STATUS_PREFIX_ERROR, "MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (currentHomingPhase != HOMING_PHASE_IDLE && currentHomingPhase != HOMING_PHASE_COMPLETE && currentHomingPhase != HOMING_PHASE_ERROR) {
		sendStatus(STATUS_PREFIX_ERROR, "MACHINE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}

	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	
	int parsed_count = sscanf(msg + strlen(CMD_STR_MACHINE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		if (PITCH_MM_PER_REV <= 0.0f) {
			sendStatus(STATUS_PREFIX_ERROR, "FATAL HOMING ERROR: PITCH_MM_PER_REV is not set or is zero.");
			homingState = HOMING_MACHINE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_INVALID_PARAMETERS; return;
		}
		if (!motorsAreEnabled) {
			sendStatus(STATUS_PREFIX_ERROR, "MACHINE_HOME_MOVE blocked: Motors disabled.");
			homingState = HOMING_MACHINE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_MOTORS_DISABLED; return;
		}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 20.0f;
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendStatus(STATUS_PREFIX_ERROR, "Error: Invalid parameters for Machine Home.");
			homingState = HOMING_MACHINE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_INVALID_PARAMETERS; return;
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

		homingState = HOMING_MACHINE;
		currentHomingPhase = HOMING_PHASE_STARTING_MOVE;
		homingStartTime = Milliseconds();
		errorState = ERROR_NONE;

		sendStatus(STATUS_PREFIX_START, "MACHINE_HOME_MOVE initiated.");
		long initial_move_steps = -1 * homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);
		
		} else {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
		homingState = HOMING_MACHINE;
		currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void Injector::handleCartridgeHomeMove(const char *msg) {
	if (mainState != HOMING_MODE) {
		sendStatus(STATUS_PREFIX_ERROR, "CARTRIDGE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (currentHomingPhase != HOMING_PHASE_IDLE && currentHomingPhase != HOMING_PHASE_COMPLETE && currentHomingPhase != HOMING_PHASE_ERROR) {
		sendStatus(STATUS_PREFIX_ERROR, "CARTRIDGE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}
	
	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	int parsed_count = sscanf(msg + strlen(CMD_STR_CARTRIDGE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		if (PITCH_MM_PER_REV <= 0.0f) {
			sendStatus(STATUS_PREFIX_ERROR, "FATAL HOMING ERROR: PITCH_MM_PER_REV is not set or is zero.");
			homingState = HOMING_CARTRIDGE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_INVALID_PARAMETERS; return;
		}
		if (!motorsAreEnabled) {
			sendStatus(STATUS_PREFIX_ERROR, "CARTRIDGE_HOME_MOVE blocked: Motors disabled.");
			homingState = HOMING_CARTRIDGE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_MOTORS_DISABLED; return;
		}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 20.0f;
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendStatus(STATUS_PREFIX_ERROR, "Error: Invalid parameters for Cartridge Home.");
			homingState = HOMING_CARTRIDGE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_INVALID_PARAMETERS; return;
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

		sendStatus(STATUS_PREFIX_START, "CARTRIDGE_HOME_MOVE initiated.");
		long initial_move_steps = 1 * homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);

		} else {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid CARTRIDGE_HOME_MOVE format. Expected 6 parameters.");
		homingState = HOMING_CARTRIDGE;
		currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void Injector::handlePinchHomeMove() {
	if (mainState != HOMING_MODE) {
		sendStatus(STATUS_PREFIX_ERROR, "PINCH_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (currentHomingPhase != HOMING_PHASE_IDLE && currentHomingPhase != HOMING_PHASE_COMPLETE && currentHomingPhase != HOMING_PHASE_ERROR) {
		sendStatus(STATUS_PREFIX_ERROR, "PINCH_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}
	if (ConnectorM2.HlfbState() != MotorDriver::HLFB_ASSERTED) {
		sendStatus(STATUS_PREFIX_ERROR, "PINCH_HOME_MOVE blocked: Pinch motor is disabled.");
		return;
	}

	long homing_steps = -800;
	int velocity = 400;
	int acceleration = 5000;
	int torque_percent = 10;
	
	homingState = HOMING_PINCH;
	currentHomingPhase = HOMING_PHASE_STARTING_MOVE;
	homingStartTime = Milliseconds();
	errorState = ERROR_NONE;

	sendStatus(STATUS_PREFIX_START, "PINCH_HOME_MOVE initiated.");
	movePinchMotor(homing_steps, torque_percent, velocity, acceleration);
}

void Injector::handlePinchJogMove(const char *msg) {
	if (mainState != JOG_MODE) {
		sendStatus(STATUS_PREFIX_ERROR, "PINCH_JOG_MOVE ignored: Not in JOG_MODE.");
		return;
	}

	float degrees = 0;
	int torque_percent = 0, velocity_sps = 0, accel_sps2 = 0;

	int parsed_count = sscanf(msg + strlen(CMD_STR_PINCH_JOG_MOVE), "%f %d %d %d",
	&degrees, &torque_percent, &velocity_sps, &accel_sps2);

	if (parsed_count == 4) {
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 30;
		if (velocity_sps <= 0) velocity_sps = 800;
		if (accel_sps2 <= 0) accel_sps2 = 5000;

		const float steps_per_degree = 800.0f / 360.0f;
		long steps_to_move = (long)(degrees * steps_per_degree);
		
		activeJogCommand = CMD_STR_PINCH_JOG_MOVE; // FIX: Track active command
		movePinchMotor(steps_to_move, torque_percent, velocity_sps, accel_sps2);
		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid PINCH_JOG_MOVE format. Expected 4 params, got %d.", parsed_count);
		sendStatus(STATUS_PREFIX_ERROR, errorMsg);
	}
}

void Injector::handleEnablePinch() {
	ConnectorM2.EnableRequest(true);
	sendStatus(STATUS_PREFIX_DONE, "ENABLE_PINCH complete.");
}

void Injector::handleDisablePinch() {
	ConnectorM2.EnableRequest(false);
	sendStatus(STATUS_PREFIX_DONE, "DISABLE_PINCH complete.");
}

void Injector::handlePinchOpen(){
}

void Injector::handlePinchClose(){
}

void Injector::handleHeaterOn() {
	heaterState = HEATER_MANUAL_ON;
	PIN_HEATER_RELAY.State(true);
	sendStatus(STATUS_PREFIX_DONE, "HEATER_ON complete.");
}

void Injector::handleHeaterOff() {
	heaterState = HEATER_OFF;
	PIN_HEATER_RELAY.State(false);
	sendStatus(STATUS_PREFIX_DONE, "HEATER_OFF complete.");
}

void Injector::handleVacuumOn() {
	PIN_VACUUM_RELAY.State(true);
	vacuumOn = true;
	sendStatus(STATUS_PREFIX_DONE, "VACUUM_ON complete.");
}

void Injector::handleVacuumOff() {
	PIN_VACUUM_RELAY.State(false);
	vacuumOn = false;
	sendStatus(STATUS_PREFIX_DONE, "VACUUM_OFF complete.");
}

void Injector::handleMoveToCartridgeHome() {
	if (mainState != FEED_MODE) { return; }
	if (!homingCartridgeDone) { errorState = ERROR_NO_CARTRIDGE_HOME; sendStatus(STATUS_PREFIX_ERROR, "Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_ERROR, "Err: Motors disabled."); return; }
	if (checkInjectorMoving() || active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_ERROR, "Err: Busy. Cannot move to cart home now.");
		errorState = ERROR_INVALID_INJECTION; return;
	}

	fullyResetActiveDispenseOperation();
	feedState = FEED_MOVING_TO_HOME;
	feedingDone = false;
	activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_HOME; // FIX: Track active command
	
	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = cartridgeHomeReferenceSteps - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	feedDefaultTorquePercent, feedDefaultVelocitySPS, feedDefaultAccelSPS2);
}

void Injector::handleMoveToCartridgeRetract(const char *msg) {
	if (mainState != FEED_MODE) { return; }
	if (!homingCartridgeDone) { errorState = ERROR_NO_CARTRIDGE_HOME; sendStatus(STATUS_PREFIX_ERROR, "Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_ERROR, "Err: Motors disabled."); return; }
	if (checkInjectorMoving() || active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_ERROR, "Err: Busy. Cannot move to cart retract now.");
		errorState = ERROR_INVALID_INJECTION; return;
	}

	float offset_mm = 0.0f;
	if (sscanf(msg + strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT), "%f", &offset_mm) != 1 || offset_mm < 0) {
		sendStatus(STATUS_PREFIX_ERROR, "Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
	}
	
	fullyResetActiveDispenseOperation();
	feedState = FEED_MOVING_TO_RETRACT;
	feedingDone = false;
	activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_RETRACT; // FIX: Track active command

	const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV;
	long offset_steps = (long)(offset_mm * current_steps_per_mm);
	long target_absolute_axis_position = cartridgeHomeReferenceSteps + offset_steps;

	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = target_absolute_axis_position - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	feedDefaultTorquePercent, feedDefaultVelocitySPS, feedDefaultAccelSPS2);
}


void Injector::handleInjectMove(const char *msg) {
	if (mainState != FEED_MODE) {
		sendStatus(STATUS_PREFIX_ERROR, "INJECT ignored: Not in FEED_MODE.");
		return;
	}
	if (checkInjectorMoving() || active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_ERROR, "Error: Operation already in progress or motors busy.");
		errorState = ERROR_INVALID_INJECTION;
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	if (sscanf(msg + strlen(CMD_STR_INJECT_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_ERROR, "INJECT blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendStatus(STATUS_PREFIX_ERROR, "Error: Steps/ml must be positive and non-zero."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f;
		if (volume_ml <= 0) { sendStatus(STATUS_PREFIX_ERROR, "Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendStatus(STATUS_PREFIX_ERROR, "Error: Inject speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendStatus(STATUS_PREFIX_ERROR, "Error: Inject acceleration must be positive."); return; }

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
		
		activeFeedCommand = CMD_STR_INJECT_MOVE; // FIX: Track active command
		
		sendStatus(STATUS_PREFIX_START, "INJECT_MOVE initiated.");
		moveInjectorMotors(active_op_remaining_steps, active_op_remaining_steps,
		active_op_torque_percent, active_op_velocity_sps, active_op_accel_sps2);
		} else {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid INJECT_MOVE format. Expected 5 params.");
	}
}

void Injector::handlePurgeMove(const char *msg) {
	if (mainState != FEED_MODE) { sendStatus(STATUS_PREFIX_ERROR, "PURGE ignored: Not in FEED_MODE."); return; }
	if (checkInjectorMoving() || active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_ERROR, "Error: Operation already in progress or motors busy.");
		errorState = ERROR_INVALID_INJECTION;
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	if (sscanf(msg + strlen(CMD_STR_PURGE_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_ERROR, "PURGE blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendStatus(STATUS_PREFIX_ERROR, "Error: Steps/ml must be positive and non-zero."); return;}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f;
		if (volume_ml <= 0) { sendStatus(STATUS_PREFIX_ERROR, "Error: Purge volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendStatus(STATUS_PREFIX_ERROR, "Error: Purge speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendStatus(STATUS_PREFIX_ERROR, "Error: Purge acceleration must be positive."); return; }

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
		
		activeFeedCommand = CMD_STR_PURGE_MOVE; // FIX: Track active command
		
		sendStatus(STATUS_PREFIX_START, "PURGE_MOVE initiated.");
		moveInjectorMotors(active_op_remaining_steps, active_op_remaining_steps,
		active_op_torque_percent, active_op_velocity_sps, active_op_accel_sps2);
		} else {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid PURGE_MOVE format. Expected 5 params.");
	}
}

void Injector::handlePauseOperation() {
	if (!active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_INFO, "PAUSE ignored: No active inject/purge operation.");
		return;
	}

	if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE) {
		ConnectorM0.MoveStopDecel();
		ConnectorM1.MoveStopDecel();

		if (feedState == FEED_INJECT_ACTIVE) feedState = FEED_INJECT_PAUSED;
		if (feedState == FEED_PURGE_ACTIVE) feedState = FEED_PURGE_PAUSED;
		sendStatus(STATUS_PREFIX_DONE, "PAUSE_INJECTION complete.");
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
		active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		feedingDone = false;

		moveInjectorMotors(active_op_remaining_steps, active_op_remaining_steps,
		active_op_torque_percent, active_op_velocity_sps, active_op_accel_sps2);
		
		if(feedState == FEED_INJECT_PAUSED) feedState = FEED_INJECT_RESUMING;
		if(feedState == FEED_PURGE_PAUSED) feedState = FEED_PURGE_RESUMING;
		
		if(feedState == FEED_INJECT_RESUMING) feedState = FEED_INJECT_ACTIVE;
		if(feedState == FEED_PURGE_RESUMING) feedState = FEED_PURGE_ACTIVE;
		
		sendStatus(STATUS_PREFIX_DONE, "RESUME_INJECTION complete.");
		} else {
		sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: Operation not paused.");
	}
}

void Injector::handleCancelOperation() {
	if (!active_dispense_INJECTION_ongoing) {
		sendStatus(STATUS_PREFIX_INFO, "CANCEL ignored: No active inject/purge operation to cancel.");
		return;
	}

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

	sendStatus(STATUS_PREFIX_DONE, "CANCEL_INJECTION complete.");
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
	activeFeedCommand = nullptr; // FIX: Clear the active command
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
	activeFeedCommand = nullptr; // FIX: Clear the active command
}

void Injector::handleSetHeaterGains(const char* msg) {
	float kp, ki, kd;
	int parsed_count = sscanf(msg + strlen(CMD_STR_SET_HEATER_GAINS), "%f %f %f", &kp, &ki, &kd);
	if (parsed_count == 3) {
		pid_kp = kp;
		pid_ki = ki;
		pid_kd = kd;
		char response[100];
		snprintf(response, sizeof(response), "SET_HEATER_GAINS complete: Kp=%.2f, Ki=%.2f, Kd=%.2f", pid_kp, pid_ki, pid_kd);
		sendStatus(STATUS_PREFIX_DONE, response);
		} else {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid SET_HEATER_GAINS format.");
	}
}

void Injector::handleSetHeaterSetpoint(const char* msg) {
	float sp = atof(msg + strlen(CMD_STR_SET_HEATER_SETPOINT));
	pid_setpoint = sp;
	char response[64];
	snprintf(response, sizeof(response), "SET_HEATER_SETPOINT complete: %.1f C", pid_setpoint);
	sendStatus(STATUS_PREFIX_DONE, response);
}

void Injector::handleHeaterPidOn() {
	if (heaterState != HEATER_PID_ACTIVE) {
		resetPid();
		heaterState = HEATER_PID_ACTIVE;
		sendStatus(STATUS_PREFIX_DONE, "HEATER_PID_ON complete.");
	}
}

void Injector::handleHeaterPidOff() {
	if (heaterState == HEATER_PID_ACTIVE) {
		heaterState = HEATER_OFF;
		PIN_HEATER_RELAY.State(false);
		sendStatus(STATUS_PREFIX_DONE, "HEATER_PID_OFF complete.");
	}
}

void Injector::handleVacuumValveOn() {
	PIN_VACUUM_VALVE_RELAY.State(true);
	vacuumValveOn = true;
	sendStatus(STATUS_PREFIX_DONE, "VACUUM_VALVE_ON complete.");
}

void Injector::handleVacuumValveOff() {
	PIN_VACUUM_VALVE_RELAY.State(false);
	vacuumValveOn = false;
	sendStatus(STATUS_PREFIX_DONE, "VACUUM_VALVE_OFF complete.");
}
