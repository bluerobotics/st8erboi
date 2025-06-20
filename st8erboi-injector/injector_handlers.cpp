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

void Injector::handleEnable(Injector *injector) {
	if (injector->mainState == DISABLED_MODE) {
		enableInjectorMotors("MOTORS ENABLED (transitioned to STANDBY_MODE)");
		injector->mainState = STANDBY_MODE;
		injector->homingState = HOMING_NONE;
		injector->feedState = FEED_NONE;
		injector->errorState = ERROR_NONE;
		sendToPC("System enabled: state is now STANDBY_MODE.");
		} else {
		// If already enabled, or in a state other than DISABLED_MODE,
		// just ensure motors are enabled without changing main state.
		if (!motorsAreEnabled) {
			enableInjectorMotors("MOTORS re-enabled (state unchanged)");
			} else {
			sendToPC("Motors already enabled, system not in DISABLED_MODE.");
		}
	}
}

void Injector::handleDisable(Injector *injector)
{
	abortInjectorMove();
	Delay_ms(200);
	injector->mainState = DISABLED_MODE;
	injector->errorState = ERROR_NONE;
	disableInjectorMotors("MOTORS DISABLED (entered DISABLED state)");
	sendToPC("System disabled: must ENABLE to return to standby.");
}

void Injector::handleAbort(Injector *injector) {
	sendToPC("ABORT received. Stopping motion and resetting...");

	abortInjectorMove();  // Stop motors
	Delay_ms(200);

	handleStandbyMode(injector);  // Fully resets system
}


void Injector::handleStandbyMode(Injector *injector) {
	// This function is mostly from your uploaded messages.cpp (handleStandby), updated
	bool wasError = (injector->errorState != ERROR_NONE); // Check if error was active
	
	if (injector->mainState != STANDBY_MODE) { // Only log/act if changing state
		abortInjectorMove(); // Good practice to stop motion when changing to standby
		Delay_ms(200);
		injector->reset();

		if (wasError) {
			sendToPC("Exited previous state: State set to STANDBY_MODE and error cleared.");
			} else {
			sendToPC("State set to STANDBY_MODE.");
		}
		} else {
		if (injector->errorState != ERROR_NONE) { // If already in standby but error occurs and then standby is requested again
			injector->errorState = ERROR_NONE;
			sendToPC("Still in STANDBY_MODE, error cleared.");
			} else {
			sendToPC("Already in STANDBY_MODE.");
		}
	}
}

void Injector::handleTestMode(Injector *injector) {
	// This function is from your uploaded messages.cpp (handleTestState)
	if (injector->mainState != TEST_MODE) {
		abortInjectorMove(); // Stop other activities
		injector->mainState = TEST_MODE;
		injector->homingState = HOMING_NONE; // Reset other mode sub-injector
		injector->feedState = FEED_NONE;
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered TEST_MODE.");
		} else {
		sendToPC("Already in TEST_MODE.");
	}
}

void Injector::handleJogMode(Injector *injector) {
	// Your uploaded messages.cpp handleJogMode uses an old 'MOVING' and 'MOVING_JOG' concept.
	// This should now simply set the main state. Actual jog moves are handled by USER_CMD_JOG_MOVE.
	if (injector->mainState != JOG_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		injector->mainState = JOG_MODE;
		injector->homingState = HOMING_NONE; // Reset other mode sub-injector
		injector->feedState = FEED_NONE;
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered JOG_MODE. Ready for JOG_MOVE commands.");
		} else {
		sendToPC("Already in JOG_MODE.");
	}
}

void Injector::handleHomingMode(Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		injector->mainState = HOMING_MODE;
		injector->homingState = HOMING_NONE; // Initialize to no specific homing action
		injector->feedState = FEED_NONE;     // Reset other mode sub-injector
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered HOMING_MODE. Ready for homing operations.");
		} else {
		sendToPC("Already in HOMING_MODE.");
	}
}

void Injector::handleFeedMode(Injector *injector) {
	if (injector->mainState != FEED_MODE) {
		abortInjectorMove(); // Stop other activities
		Delay_ms(200);
		injector->mainState = FEED_MODE;
		injector->feedState = FEED_STANDBY;  // Default sub-state for feed operations
		injector->homingState = HOMING_NONE; // Reset other mode sub-injector
		injector->errorState = ERROR_NONE;   // Clear errors
		sendToPC("Entered FEED_MODE. Ready for inject/purge/retract.");
		} else {
		sendToPC("Already in FEED_MODE.");
	}
}

void Injector::handleSetinjectorMotorsTorqueOffset(const char *msg) {
	float newOffset = atof(msg + strlen(USER_CMD_STR_SET_INJECTOR_TORQUE_OFFSET)); // Use defined string length
	injectorMotorsTorqueOffset = newOffset; // This directly updates the global from motor.cpp
	char response[64];
	snprintf(response, sizeof(response), "Global torque offset set to %.2f", injectorMotorsTorqueOffset);
	sendToPC(response);
}

void Injector::handleJogMove(const char *msg, Injector *injector) {
	if (injector->mainState != JOG_MODE) {
		sendToPC("JOG_MOVE ignored: Not in JOG_MODE.");
		return;
	}

	long s0 = 0, s1 = 0;
	int torque_percent = 0, velocity_sps = 0, accel_sps2 = 0;

	// Format: JOG_MOVE <s0_steps> <s1_steps> <torque_%> <vel_sps> <accel_sps2>
	int parsed_count = sscanf(msg + strlen(USER_CMD_STR_JOG_MOVE), "%ld %ld %d %d %d",
	&s0, &s1,
	&torque_percent,
	&velocity_sps,
	&accel_sps2);

	if (parsed_count == 5) {
		char response[150];
		snprintf(response, sizeof(response),
		"JOG_MOVE RX: s0:%ld s1:%ld TqL:%d%% Vel:%d Acc:%d",
		s0, s1, torque_percent, velocity_sps, accel_sps2);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("JOG_MOVE blocked: Motors are disabled.");
			return;
		}

		// Validate torque_percent, velocity_sps, accel_sps2 if necessary
		if (torque_percent <= 0 || torque_percent > 100) {
			torque_percent = 30; // Fallback to a sensible default like 30%
			char torqueWarn[80];
			snprintf(torqueWarn, sizeof(torqueWarn), "Warning: Invalid jog torque in command, using %d%%.", torque_percent);
			sendToPC(torqueWarn);
		}
		if (velocity_sps <= 0) {
			// extern int32_t velocityLimit; // from motor.cpp
			// velocity_sps = velocityLimit;
			velocity_sps = 800; // Fallback
			sendToPC("Warning: Invalid jog velocity, using default.");
		}
		if (accel_sps2 <= 0) {
			// extern int32_t accelerationLimit; // from motor.cpp
			// accel_sps2 = accelerationLimit;
			accel_sps2 = 5000; // Fallback
			sendToPC("Warning: Invalid jog acceleration, using default.");
		}


		// Directly call your existing moveInjectorMotors function
		moveInjectorMotors((int)s0, (int)s1, torque_percent, velocity_sps, accel_sps2);

		injector->jogDone = false; // Indicate a jog operation has started (if you use this flag)

		} else {
		char errorMsg[100];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		sendToPC(errorMsg);
	}
}

void Injector::handleMachineHomeMove(const char *msg, Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		sendToPC("MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (injector->currentHomingPhase != HOMING_PHASE_IDLE && injector->currentHomingPhase != HOMING_PHASE_COMPLETE && injector->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("MACHINE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}

	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	// Format: <USER_CMD_STR> <stroke_mm> <rapid_vel_mm_s> <touch_vel_mm_s> <acceleration_mm_s2> <retract_dist_mm> <torque_%>
	
	int parsed_count = sscanf(msg + strlen(USER_CMD_STR_MACHINE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250]; // Increased for acceleration
		snprintf(response, sizeof(response), "MACHINE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("MACHINE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_MACHINE; // Indicate it was attempted
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Machine Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Machine Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_MACHINE;
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		// Store parameters in Injector
		injector->homing_stroke_mm_param = stroke_mm;
		injector->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		injector->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		injector->homing_acceleration_param = acceleration_mm_s2;
		injector->homing_retract_mm_param = retract_mm;
		injector->homing_torque_percent_param = torque_percent;

		// Calculate and store step/sps values
		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		injector->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		injector->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		injector->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		// Initialize homing state machine
		injector->homingState = HOMING_MACHINE;
		injector->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		injector->homingStartTime = Milliseconds(); // For timeout tracking
		injector->errorState = ERROR_NONE; // Clear previous errors

		sendToPC("Initiating Machine Homing: RAPID_MOVE phase.");
		// Machine homing is typically in the negative direction
		long initial_move_steps = -1 * injector->homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)injector->homing_torque_percent_param, injector->homing_actual_rapid_sps, injector->homing_actual_accel_sps2);
		
		} else {
		sendToPC("Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
		injector->homingState = HOMING_MACHINE; // Indicate it was attempted
		injector->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void Injector::handleCartridgeHomeMove(const char *msg, Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (injector->currentHomingPhase != HOMING_PHASE_IDLE && injector->currentHomingPhase != HOMING_PHASE_COMPLETE && injector->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("CARTRIDGE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}
	
	float stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent;
	int parsed_count = sscanf(msg + strlen(USER_CMD_STR_CARTRIDGE_HOME_MOVE), "%f %f %f %f %f %f",
	&stroke_mm, &rapid_vel_mm_s, &touch_vel_mm_s, &acceleration_mm_s2, &retract_mm, &torque_percent);

	if (parsed_count == 6) {
		char response[250];
		snprintf(response, sizeof(response), "CARTRIDGE_HOME_MOVE RX: Strk:%.1f RpdV:%.1f TchV:%.1f Acc:%.1f Ret:%.1f Tq:%.1f%%",
		stroke_mm, rapid_vel_mm_s, touch_vel_mm_s, acceleration_mm_s2, retract_mm, torque_percent);
		sendToPC(response);

		if (!motorsAreEnabled) {
			sendToPC("CARTRIDGE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_CARTRIDGE;
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
			return;
		}
		if (torque_percent <= 0 || torque_percent > 100) {
			sendToPC("Warning: Invalid torque for Cartridge Home. Using default 20%.");
			torque_percent = 20.0f;
		}
		if (rapid_vel_mm_s <= 0 || touch_vel_mm_s <= 0 || acceleration_mm_s2 <=0 || stroke_mm <= 0 || retract_mm < 0) {
			sendToPC("Error: Invalid parameters for Cartridge Home (must be positive, retract >= 0). Set to HOMING_PHASE_ERROR.");
			injector->homingState = HOMING_CARTRIDGE;
			injector->currentHomingPhase = HOMING_PHASE_ERROR;
			injector->errorState = ERROR_MANUAL_ABORT; // Or specific param error
			return;
		}


		injector->homing_stroke_mm_param = stroke_mm;
		injector->homing_rapid_vel_mm_s_param = rapid_vel_mm_s;
		injector->homing_touch_vel_mm_s_param = touch_vel_mm_s;
		injector->homing_acceleration_param = acceleration_mm_s2;
		injector->homing_retract_mm_param = retract_mm;
		injector->homing_torque_percent_param = torque_percent;

		const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
		injector->homing_actual_stroke_steps = (long)(stroke_mm * current_steps_per_mm);
		injector->homing_actual_rapid_sps = (int)(rapid_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_touch_sps = (int)(touch_vel_mm_s * current_steps_per_mm);
		injector->homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * current_steps_per_mm);
		injector->homing_actual_retract_steps = (long)(retract_mm * current_steps_per_mm);

		injector->homingState = HOMING_CARTRIDGE;
		injector->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
		injector->homingStartTime = Milliseconds();
		injector->errorState = ERROR_NONE;

		sendToPC("Initiating Cartridge Homing: RAPID_MOVE phase.");
		// Cartridge homing is typically in the positive direction
		long initial_move_steps = 1 * injector->homing_actual_stroke_steps;
		moveInjectorMotors(initial_move_steps, initial_move_steps, (int)injector->homing_torque_percent_param, injector->homing_actual_rapid_sps, injector->homing_actual_accel_sps2);

		} else {
		sendToPC("Invalid CARTRIDGE_HOME_MOVE format. Expected 6 parameters.");
		injector->homingState = HOMING_CARTRIDGE;
		injector->currentHomingPhase = HOMING_PHASE_ERROR;
	}
}

void Injector::handlePinchHomeMove(Injector *injector) {
	if (injector->mainState != HOMING_MODE) {
		sendToPC("MACHINE_HOME_MOVE ignored: Not in HOMING_MODE.");
		return;
	}
	if (injector->currentHomingPhase != HOMING_PHASE_IDLE && injector->currentHomingPhase != HOMING_PHASE_COMPLETE && injector->currentHomingPhase != HOMING_PHASE_ERROR) {
		sendToPC("MACHINE_HOME_MOVE ignored: Homing operation already in progress.");
		return;
	}

	float velocity = 200; //steps per second
	float acceleration = 5000; //steps per second squared
	float torque_percent = 30;
	
	if (!motorsAreEnabled) {
		sendToPC("MACHINE_HOME_MOVE blocked: Motors disabled. Set to HOMING_PHASE_ERROR.");
		injector->homingState = HOMING_MACHINE; // Indicate it was attempted
		injector->currentHomingPhase = HOMING_PHASE_ERROR;
		injector->errorState = ERROR_MANUAL_ABORT; // Or a more specific error
		return;
	}

	// Initialize homing state machine
	injector->homingState = HOMING_MACHINE;
	injector->currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
	injector->homingStartTime = Milliseconds(); // For timeout tracking
	injector->errorState = ERROR_NONE; // Clear previous errors

	sendToPC("Initiating Machine Homing: RAPID_MOVE phase.");
	// Machine homing is typically in the negative direction
	long initial_move_steps = -1 * injector->homing_actual_stroke_steps;
	movePinchMotor(800, initial_move_steps, (int)injector->homing_torque_percent_param, injector->homing_actual_rapid_sps, injector->homing_actual_accel_sps2);
	
	} else {
	sendToPC("Invalid MACHINE_HOME_MOVE format. Expected 6 parameters.");
	injector->homingState = HOMING_MACHINE; // Indicate it was attempted
	injector->currentHomingPhase = HOMING_PHASE_ERROR;
}

void Injector::handlePinchOpen(Injector *injector){
	
}

void Injector::handlePinchClose(Injector *injector){
	
}

void Injector::resetActiveDispenseOp(Injector *injector) {
	injector->active_op_target_ml = 0.0f;
	injector->active_op_total_dispensed_ml = 0.0f;
	injector->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
	injector->active_op_steps_per_ml = 0.0f;
	injector->active_dispense_INJECTION_ongoing = false;
}

void Injector::handleClearErrors(Injector *injector) {
	sendToPC("CLEAR_ERRORS received. Resetting system...");
	handleStandbyMode(injector);  // Goes to standby and clears errors
}

void Injector::handleMoveToCartridgeHome(Injector *injector) {
	if (injector->mainState != FEED_MODE) { /* ... */ return; }
	if (!injector->homingCartridgeDone) { injector->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { sendToPC("Err: Motors disabled."); return; }
	if (checkInjectorMoving() || injector->feedState == FEED_INJECT_ACTIVE || injector->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart home now.");
		injector->errorState = ERROR_INVALID_INJECTION; return;
	}

	sendToPC("Cmd: Move to Cartridge Home...");
	fullyResetActiveDispenseOperation(injector); // Reset any prior dispense op
	injector->feedState = FEED_MOVING_TO_HOME;
	injector->feedingDone = false;
	
	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = cartridgeHomeReferenceSteps - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	FEED_GOTO_TORQUE_PERCENT, FEED_GOTO_VELOCITY_SPS, FEED_GOTO_ACCEL_SPS2);
}

void Injector::handleMoveToCartridgeRetract(const char *msg, Injector *injector) {
	if (injector->mainState != FEED_MODE) { /* ... */ return; }
	if (!injector->homingCartridgeDone) { /* ... */ injector->errorState = ERROR_NO_CARTRIDGE_HOME; sendToPC("Err: Cartridge not homed."); return; }
	if (!motorsAreEnabled) { /* ... */ sendToPC("Err: Motors disabled."); return; }
	if (checkInjectorMoving() || injector->feedState == FEED_INJECT_ACTIVE || injector->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Err: Busy. Cannot move to cart retract now.");
		injector->errorState = ERROR_INVALID_INJECTION; return;
	}

	float offset_mm = 0.0f;
	if (sscanf(msg + strlen(USER_CMD_STR_MOVE_TO_CARTRIDGE_RETRACT), "%f", &offset_mm) != 1 || offset_mm < 0) {
		sendToPC("Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
	}
	
	fullyResetActiveDispenseOperation(injector);
	injector->feedState = FEED_MOVING_TO_RETRACT;
	injector->feedingDone = false;

	const float current_steps_per_mm = (float)pulsesPerRev / PITCH_MM_PER_REV_CONST;
	long offset_steps = (long)(offset_mm * current_steps_per_mm);
	long target_absolute_axis_position = cartridgeHomeReferenceSteps + offset_steps;

	char response[150];
	snprintf(response, sizeof(response), "Cmd: Move to Cart Retract (Offset: %.1fmm, Target: %ld steps)", offset_mm, target_absolute_axis_position);
	sendToPC(response);

	long current_axis_pos = ConnectorM0.PositionRefCommanded();
	long steps_to_move_axis = target_absolute_axis_position - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	FEED_GOTO_TORQUE_PERCENT, FEED_GOTO_VELOCITY_SPS, FEED_GOTO_ACCEL_SPS2);
}


void Injector::handleInjectMove(const char *msg, Injector *injector) {
	if (injector->mainState != FEED_MODE) {
		sendToPC("INJECT ignored: Not in FEED_MODE.");
		return;
	}
	if (checkInjectorMoving() || injector->active_dispense_INJECTION_ongoing) {
		sendToPC("Error: Operation already in progress or motors busy.");
		injector->errorState = ERROR_INVALID_INJECTION; // Set an error state
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	// Format: INJECT_MOVE <volume_ml> <speed_ml_s> <feed_accel_sps2> <feed_steps_per_ml> <feed_torque_percent>
	if (sscanf(msg + strlen(USER_CMD_STR_INJECT_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendToPC("INJECT blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendToPC("Error: Steps/ml must be positive and non-zero."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f; // Default
		if (volume_ml <= 0) { sendToPC("Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Inject speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendToPC("Error: Inject acceleration must be positive."); return; }

		// Clear previous operation's trackers and set last completed to 0 for this new context
		fullyResetActiveDispenseOperation(injector);
		injector->last_completed_dispense_ml = 0.0f;

		injector->feedState = FEED_INJECT_STARTING; // Will transition to ACTIVE in main loop
		injector->feedingDone = false; // Mark that a feed operation is starting
		injector->active_dispense_INJECTION_ongoing = true;
		injector->active_op_target_ml = volume_ml;
		injector->active_op_steps_per_ml = steps_per_ml_val;
		injector->active_op_total_target_steps = (long)(volume_ml * injector->active_op_steps_per_ml);
		injector->active_op_remaining_steps = injector->active_op_total_target_steps;
		
		// Capture motor position at the very start of the entire multi-segment operation
		injector->active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		// Also set for the first segment
		injector->active_op_segment_initial_axis_steps = injector->active_op_initial_axis_steps;
		
		injector->active_op_total_dispensed_ml = 0.0f; // Explicitly reset for the new operation

		injector->active_op_velocity_sps = (int)(speed_ml_s * injector->active_op_steps_per_ml);
		injector->active_op_accel_sps2 = (int)acceleration_sps2_param; // Use param directly
		injector->active_op_torque_percent = (int)torque_percent;
		if (injector->active_op_velocity_sps <= 0) injector->active_op_velocity_sps = 100; // Fallback velocity

		char response[200];
		snprintf(response, sizeof(response), "RX INJECT: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, injector->active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Inject operation...");

		// Positive steps for inject/purge (assuming forward motion dispenses)
		moveInjectorMotors(injector->active_op_remaining_steps, injector->active_op_remaining_steps,
		injector->active_op_torque_percent, injector->active_op_velocity_sps, injector->active_op_accel_sps2);
		// injector->feedState will be set to FEED_INJECT_ACTIVE in main.cpp loop once motion starts
		} else {
		sendToPC("Invalid INJECT_MOVE format. Expected 5 params.");
	}
}

void Injector::handlePurgeMove(const char *msg, Injector *injector) {
	if (injector->mainState != FEED_MODE) { sendToPC("PURGE ignored: Not in FEED_MODE."); return; }
	if (checkInjectorMoving() || injector->active_dispense_INJECTION_ongoing) {
		sendToPC("Error: Operation already in progress or motors busy.");
		injector->errorState = ERROR_INVALID_INJECTION;
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	// Format: PURGE_MOVE <volume_ml> <speed_ml_s> <feed_accel_sps2> <feed_steps_per_ml> <feed_torque_percent>
	if (sscanf(msg + strlen(USER_CMD_STR_PURGE_MOVE), "%f %f %f %f %f",
	&volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!motorsAreEnabled) { sendToPC("PURGE blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { sendToPC("Error: Steps/ml must be positive and non-zero."); return;}
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f; // Default
		if (volume_ml <= 0) { sendToPC("Error: Purge volume must be positive."); return; }
		if (speed_ml_s <= 0) { sendToPC("Error: Purge speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { sendToPC("Error: Purge acceleration must be positive."); return; }

		// Clear previous operation's trackers and set last completed to 0 for this new context
		fullyResetActiveDispenseOperation(injector);
		injector->last_completed_dispense_ml = 0.0f;

		injector->feedState = FEED_PURGE_STARTING; // Will transition to ACTIVE in main loop
		injector->feedingDone = false;
		injector->active_dispense_INJECTION_ongoing = true;
		injector->active_op_target_ml = volume_ml;
		injector->active_op_steps_per_ml = steps_per_ml_val;
		injector->active_op_total_target_steps = (long)(volume_ml * injector->active_op_steps_per_ml);
		injector->active_op_remaining_steps = injector->active_op_total_target_steps;
		
		injector->active_op_initial_axis_steps = ConnectorM0.PositionRefCommanded();
		injector->active_op_segment_initial_axis_steps = injector->active_op_initial_axis_steps;
		
		injector->active_op_total_dispensed_ml = 0.0f; // Explicitly reset

		injector->active_op_velocity_sps = (int)(speed_ml_s * injector->active_op_steps_per_ml);
		injector->active_op_accel_sps2 = (int)acceleration_sps2_param;
		injector->active_op_torque_percent = (int)torque_percent;
		if (injector->active_op_velocity_sps <= 0) injector->active_op_velocity_sps = 200; // Fallback

		char response[200];
		snprintf(response, sizeof(response), "RX PURGE: Vol:%.2fml, Speed:%.2fml/s (calc_vel_sps:%d), Acc:%.0f, Steps/ml:%.2f, Tq:%.0f%%",
		volume_ml, speed_ml_s, injector->active_op_velocity_sps, acceleration_sps2_param, steps_per_ml_val, torque_percent);
		sendToPC(response);
		sendToPC("Starting Purge operation...");
		
		moveInjectorMotors(injector->active_op_remaining_steps, injector->active_op_remaining_steps,
		injector->active_op_torque_percent, injector->active_op_velocity_sps, injector->active_op_accel_sps2);
		// injector->feedState will be set to FEED_PURGE_ACTIVE in main.cpp loop once motion starts
		} else {
		sendToPC("Invalid PURGE_MOVE format. Expected 5 params.");
	}
}

void Injector::handlePauseOperation(Injector *injector) {
	if (!injector->active_dispense_INJECTION_ongoing) {
		sendToPC("PAUSE ignored: No active inject/purge operation.");
		return;
	}

	if (injector->feedState == FEED_INJECT_ACTIVE || injector->feedState == FEED_PURGE_ACTIVE) {
		sendToPC("Cmd: Pausing operation...");
		ConnectorM0.MoveStopDecel();
		ConnectorM1.MoveStopDecel();

		// Immediately set paused state, main loop will finalize steps
		if (injector->feedState == FEED_INJECT_ACTIVE) injector->feedState = FEED_INJECT_PAUSED;
		if (injector->feedState == FEED_PURGE_ACTIVE) injector->feedState = FEED_PURGE_PAUSED;
		} else {
		sendToPC("PAUSE ignored: Not in an active inject/purge state.");
	}
}


void Injector::handleResumeOperation(Injector *injector) {
	if (!injector->active_dispense_INJECTION_ongoing) {
		sendToPC("RESUME ignored: No operation was ongoing or paused.");
		return;
	}
	if (injector->feedState == FEED_INJECT_PAUSED || injector->feedState == FEED_PURGE_PAUSED) {
		if (injector->active_op_remaining_steps <= 0) {
			sendToPC("RESUME ignored: No remaining volume/steps to dispense.");
			fullyResetActiveDispenseOperation(injector);
			injector->feedState = FEED_STANDBY;
			return;
		}
		sendToPC("Cmd: Resuming operation...");
		injector->active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded(); // Update base for this segment
		injector->feedingDone = false; // New segment starting

		moveInjectorMotors(injector->active_op_remaining_steps, injector->active_op_remaining_steps,
		injector->active_op_torque_percent, injector->active_op_velocity_sps, injector->active_op_accel_sps2);
		
		if(injector->feedState == FEED_INJECT_PAUSED) injector->feedState = FEED_INJECT_RESUMING; // Or directly to _ACTIVE
		if(injector->feedState == FEED_PURGE_PAUSED) injector->feedState = FEED_PURGE_RESUMING;  // Or directly to _ACTIVE
		// main.cpp will transition from RESUMING to ACTIVE once motors are moving, or directly here.
		// For simplicity let's assume it goes active if move command is successful.
		if(injector->feedState == FEED_INJECT_RESUMING) injector->feedState = FEED_INJECT_ACTIVE;
		if(injector->feedState == FEED_PURGE_RESUMING) injector->feedState = FEED_PURGE_ACTIVE;


		} else {
		sendToPC("RESUME ignored: Operation not paused.");
	}
}

void Injector::handleCancelOperation(Injector *injector) {
	if (!injector->active_dispense_INJECTION_ongoing) {
		sendToPC("CANCEL ignored: No active inject/purge operation to cancel.");
		return;
	}

	sendToPC("Cmd: Cancelling current feed operation...");
	abortInjectorMove(); // Stop motors abruptly
	Delay_ms(100); // Give motors a moment to fully stop if needed

	// Even though cancelled, calculate what was dispensed before reset for logging/internal use if needed.
	// However, for the purpose of telemetry display when idle, a cancelled op means 0 "completed" dispense.
	if (injector->active_op_steps_per_ml > 0.0001f && injector->active_dispense_INJECTION_ongoing) {
		long steps_moved_on_axis = ConnectorM0.PositionRefCommanded() - injector->active_op_segment_initial_axis_steps;
		float segment_dispensed_ml = (float)fabs(steps_moved_on_axis) / injector->active_op_steps_per_ml;
		injector->active_op_total_dispensed_ml += segment_dispensed_ml;
		// This active_op_total_dispensed_ml will be reset by fullyResetActiveDispenseOperation shortly.
		// If you needed to log the actual amount dispensed before cancel, do it here.
	}
	
	injector->last_completed_dispense_ml = 0.0f; // Set "completed" amount to 0 for a cancelled operation

	fullyResetActiveDispenseOperation(injector); // Clears all active_op_ trackers
	
	injector->feedState = FEED_INJECTION_CANCELLED; // A specific state for cancelled
	// Main loop should transition this to FEED_STANDBY
	injector->feedingDone = true; // Mark as "done" (via cancellation)
	injector->errorState = ERROR_NONE; // Typically, a user cancel is not an "error" condition.
	// If it should be, set injector->errorState = ERROR_MANUAL_ABORT;

	sendToPC("Operation Cancelled. Dispensed value for this attempt will show as 0 ml in idle telemetry.");
}

void Injector::finalizeAndResetActiveDispenseOperation(Injector *injector, bool operationCompletedSuccessfully) {
	if (injector->active_dispense_INJECTION_ongoing) {
		// Calculate final dispensed amount for this operation segment
		if (injector->active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - injector->active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / injector->active_op_steps_per_ml;
			injector->active_op_total_dispensed_ml += segment_dispensed_ml;
		}
		injector->last_completed_dispense_ml = injector->active_op_total_dispensed_ml; // Store final total
	}
	
	// This part is similar to fullyReset, but called after a successful/finalized op.
	injector->active_dispense_INJECTION_ongoing = false;
	injector->active_op_target_ml = 0.0f;
	// injector->active_op_total_dispensed_ml is now in last_completed_dispense_ml. If you want it zero for next op,
	// fullyReset should be called by the START of the next op.
	injector->active_op_remaining_steps = 0;
}

void Injector::fullyResetActiveDispenseOperation(Injector *injector) {
	injector->active_dispense_INJECTION_ongoing = false;
	injector->active_op_target_ml = 0.0f;
	injector->active_op_total_dispensed_ml = 0.0f; // Crucial for starting fresh
	injector->active_op_total_target_steps = 0;
	injector->active_op_remaining_steps = 0;
	injector->active_op_segment_initial_axis_steps = 0; // Reset for next segment start
	injector->active_op_initial_axis_steps = 0;       // Reset for the very start of an operation
	injector->active_op_steps_per_ml = 0.0f;
	// Note: injector->last_completed_dispense_ml is now explicitly managed by callers (start, cancel, complete)
	// to reflect the outcome of the operation for idle telemetry.
}