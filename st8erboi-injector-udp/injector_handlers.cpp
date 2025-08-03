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
    
    if (mainState != STANDBY_MODE || homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone) {
        abortInjectorMove();
        Delay_ms(200);
        mainState = STANDBY_MODE;
        homingState = HOMING_NONE;
        currentHomingPhase = HOMING_PHASE_IDLE;
        feedState = FEED_STANDBY;
        errorState = ERROR_NONE;
        jogDone = true;
        fullyResetActiveDispenseOperation();

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

void Injector::handleSetinjectorMotorsTorqueOffset(const char *msg) {
    float newOffset = atof(msg + strlen(CMD_STR_SET_INJECTOR_TORQUE_OFFSET));
    injectorMotorsTorqueOffset = newOffset;
    char response[64];
    snprintf(response, sizeof(response), "Global torque offset set to %.2f", injectorMotorsTorqueOffset);
    sendStatus(STATUS_PREFIX_DONE, response);
}

void Injector::handleJogMove(const char* msg) {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "JOG_MOVE ignored: Another operation is in progress.");
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

        long steps1 = (long)(dist_mm1 * STEPS_PER_MM_INJECTOR);
        long steps2 = (long)(dist_mm2 * STEPS_PER_MM_INJECTOR);
        int velocity_sps = (int)(vel_mms * STEPS_PER_MM_INJECTOR);
        int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_INJECTOR);
        
        activeJogCommand = CMD_STR_JOG_MOVE;
        moveInjectorMotors(steps1, steps2, torque_percent, velocity_sps, accel_sps2_val);
        jogDone = false;
        } else {
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
        sendStatus(STATUS_PREFIX_ERROR, errorMsg);
    }
}

void Injector::handleMachineHomeMove() {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "MACHINE_HOME_MOVE ignored: Another operation is in progress.");
        return;
    }
    if (!motorsAreEnabled) {
        sendStatus(STATUS_PREFIX_ERROR, "MACHINE_HOME_MOVE blocked: Motors disabled.");
        homingState = HOMING_MACHINE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_MOTORS_DISABLED; return;
    }

    // Hardcoded parameters
    float stroke_mm = 500.0f;
    float rapid_vel_mm_s = 20.0f;
    float touch_vel_mm_s = 1.0f;
    float acceleration_mm_s2 = 100.0f;
    float retract_mm = 10.0f;
    float torque_percent = 10.0f;

    homing_torque_percent_param = torque_percent;
    homing_actual_stroke_steps = (long)(stroke_mm * STEPS_PER_MM_INJECTOR);
    homing_actual_rapid_sps = (int)(rapid_vel_mm_s * STEPS_PER_MM_INJECTOR);
    homing_actual_touch_sps = (int)(touch_vel_mm_s * STEPS_PER_MM_INJECTOR);
    homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * STEPS_PER_MM_INJECTOR);
    homing_actual_retract_steps = (long)(retract_mm * STEPS_PER_MM_INJECTOR);

    homingState = HOMING_MACHINE;
    currentHomingPhase = HOMING_PHASE_STARTING_MOVE;
    homingStartTime = Milliseconds();
    errorState = ERROR_NONE;

    sendStatus(STATUS_PREFIX_START, "MACHINE_HOME_MOVE initiated.");
    long initial_move_steps = -1 * homing_actual_stroke_steps;
    moveInjectorMotors(initial_move_steps, initial_move_steps, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);
}

void Injector::handleCartridgeHomeMove() {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "CARTRIDGE_HOME_MOVE ignored: Another operation is in progress.");
        return;
    }
    if (!motorsAreEnabled) {
        sendStatus(STATUS_PREFIX_ERROR, "CARTRIDGE_HOME_MOVE blocked: Motors disabled.");
        homingState = HOMING_CARTRIDGE; currentHomingPhase = HOMING_PHASE_ERROR; errorState = ERROR_MOTORS_DISABLED; return;
    }

    // Hardcoded parameters
    float stroke_mm = 500.0f;
    float rapid_vel_mm_s = 20.0f;
    float touch_vel_mm_s = 1.0f;
    float acceleration_mm_s2 = 100.0f;
    float retract_mm = 10.0f;
    float torque_percent = 10.0f;

    homing_torque_percent_param = torque_percent;
    homing_actual_stroke_steps = (long)(stroke_mm * STEPS_PER_MM_INJECTOR);
    homing_actual_rapid_sps = (int)(rapid_vel_mm_s * STEPS_PER_MM_INJECTOR);
    homing_actual_touch_sps = (int)(touch_vel_mm_s * STEPS_PER_MM_INJECTOR);
    homing_actual_accel_sps2 = (int)(acceleration_mm_s2 * STEPS_PER_MM_INJECTOR);
    homing_actual_retract_steps = (long)(retract_mm * STEPS_PER_MM_INJECTOR);

    homingState = HOMING_CARTRIDGE;
    currentHomingPhase = HOMING_PHASE_STARTING_MOVE;
    homingStartTime = Milliseconds();
    errorState = ERROR_NONE;

    sendStatus(STATUS_PREFIX_START, "CARTRIDGE_HOME_MOVE initiated.");
    long initial_move_steps = 1 * homing_actual_stroke_steps;
    moveInjectorMotors(initial_move_steps, initial_move_steps, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);
}

void Injector::handlePinchHomeMove() {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "PINCH_HOME_MOVE ignored: Another operation is in progress.");
        return;
    }
    if (ConnectorM2.HlfbState() != MotorDriver::HLFB_ASSERTED || ConnectorM3.HlfbState() != MotorDriver::HLFB_ASSERTED) {
        sendStatus(STATUS_PREFIX_ERROR, "PINCH_HOME_MOVE blocked: One or both pinch motors are disabled.");
        return;
    }

    // --- Reset independent state flags ---
    homingPinch2Done = false;
    homingPinch3Done = false;
    
    homingState = HOMING_PINCH;
    currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
    homingStartTime = Milliseconds();
    errorState = ERROR_NONE;

    // Hardcoded parameters for the simple "ram" home
    const float stroke_mm = 20.0f;
    const float vel_mms = 5.0f;
    const float accel_mms2 = 100.0f;
    const int torque_percent = 20;

    const int velocity_sps = (int)(vel_mms * STEPS_PER_MM_PINCH);
    const int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_PINCH);
    const long steps_to_move = (long)(stroke_mm * STEPS_PER_MM_PINCH);
    
    // Set the torque limit for this operation
    injectorMotorsTorqueLimit = (float)torque_percent;

    sendStatus(STATUS_PREFIX_START, "PINCH_HOME_MOVE initiated for both pinch motors.");

    // Command the move on both pinch motors (M2 and M3)
    ConnectorM2.VelMax(velocity_sps);
    ConnectorM2.AccelMax(accel_sps2_val);
    ConnectorM2.Move(steps_to_move);

    ConnectorM3.VelMax(velocity_sps);
    ConnectorM3.AccelMax(accel_sps2_val);
    ConnectorM3.Move(steps_to_move);
}

void Injector::handlePinchJogMove(const char *msg) {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "PINCH_JOG_MOVE ignored: Another operation is in progress.");
        return;
    }

    float dist_mm = 0, vel_mms = 0, accel_mms2 = 0;
    int torque_percent = 0;

    int parsed_count = sscanf(msg + strlen(CMD_STR_PINCH_JOG_MOVE), "%f %f %f %d",
                              &dist_mm, &vel_mms, &accel_mms2, &torque_percent);

    if (parsed_count == 4) {
        if (torque_percent <= 0 || torque_percent > 100) torque_percent = 30;
        if (vel_mms <= 0) vel_mms = 5.0f;
        if (accel_mms2 <= 0) accel_mms2 = 25.0f;

        long steps_to_move = (long)(dist_mm * STEPS_PER_MM_PINCH);
        int velocity_sps = (int)(vel_mms * STEPS_PER_MM_PINCH);
        int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_PINCH);
        
        activeJogCommand = CMD_STR_PINCH_JOG_MOVE;
        movePinchMotor(steps_to_move, torque_percent, velocity_sps, accel_sps2_val);
        jogDone = false;
    } else {
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "Invalid PINCH_JOG_MOVE format. Expected 4 params, got %d.", parsed_count);
        sendStatus(STATUS_PREFIX_ERROR, errorMsg);
    }
}

void Injector::handleEnablePinch() {
    ConnectorM2.EnableRequest(true);
    ConnectorM3.EnableRequest(true);
    sendStatus(STATUS_PREFIX_DONE, "ENABLE_PINCH complete for both motors.");
}

void Injector::handleDisablePinch() {
    ConnectorM2.EnableRequest(false);
    ConnectorM3.EnableRequest(false);
    sendStatus(STATUS_PREFIX_DONE, "DISABLE_PINCH complete for both motors.");
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
    PIN_VACUUM_VALVE_RELAY.State(true);
    vacuumValveOn = true;
    sendStatus(STATUS_PREFIX_DONE, "VACUUM_ON complete.");
}

void Injector::handleVacuumOff() {
    PIN_VACUUM_RELAY.State(false);
    vacuumOn = false;
    PIN_VACUUM_VALVE_RELAY.State(false);
    vacuumValveOn = false;
    sendStatus(STATUS_PREFIX_DONE, "VACUUM_OFF complete.");
}

void Injector::handleMoveToCartridgeHome() {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "Err: Busy. Cannot move to cart home now.");
        return;
    }
    if (!homingCartridgeDone) { errorState = ERROR_NO_CARTRIDGE_HOME; sendStatus(STATUS_PREFIX_ERROR, "Err: Cartridge not homed."); return; }
    if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_ERROR, "Err: Motors disabled."); return; }
    
    fullyResetActiveDispenseOperation();
    feedState = FEED_MOVING_TO_HOME;
    feedingDone = false;
    activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_HOME;
    
    long current_axis_pos = ConnectorM0.PositionRefCommanded();
    long steps_to_move_axis = cartridgeHomeReferenceSteps - current_axis_pos;

    moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
    feedDefaultTorquePercent, feedDefaultVelocitySPS, feedDefaultAccelSPS2);
}

void Injector::handleMoveToCartridgeRetract(const char *msg) {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "Err: Busy. Cannot move to cart retract now.");
        return;
    }
    if (!homingCartridgeDone) { errorState = ERROR_NO_CARTRIDGE_HOME; sendStatus(STATUS_PREFIX_ERROR, "Err: Cartridge not homed."); return; }
    if (!motorsAreEnabled) { sendStatus(STATUS_PREFIX_ERROR, "Err: Motors disabled."); return; }
    
    float offset_mm = 0.0f;
    if (sscanf(msg + strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT), "%f", &offset_mm) != 1 || offset_mm < 0) {
        sendStatus(STATUS_PREFIX_ERROR, "Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
    }
    
    fullyResetActiveDispenseOperation();
    feedState = FEED_MOVING_TO_RETRACT;
    feedingDone = false;
    activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_RETRACT;

    const float current_steps_per_mm = (float)PULSES_PER_REV / INJECTOR_PITCH_MM_PER_REV;
    long offset_steps = (long)(offset_mm * current_steps_per_mm);
    long target_absolute_axis_position = cartridgeHomeReferenceSteps + offset_steps;

    long current_axis_pos = ConnectorM0.PositionRefCommanded();
    long steps_to_move_axis = target_absolute_axis_position - current_axis_pos;

    moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
    feedDefaultTorquePercent, feedDefaultVelocitySPS, feedDefaultAccelSPS2);
}


void Injector::handleInjectMove(const char *msg) {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "Error: Operation already in progress or motors busy.");
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
        
        activeFeedCommand = CMD_STR_INJECT_MOVE;
        
        sendStatus(STATUS_PREFIX_START, "INJECT_MOVE initiated.");
        moveInjectorMotors(active_op_remaining_steps, active_op_remaining_steps,
        active_op_torque_percent, active_op_velocity_sps, active_op_accel_sps2);
        } else {
        sendStatus(STATUS_PREFIX_ERROR, "Invalid INJECT_MOVE format. Expected 5 params.");
    }
}

void Injector::handlePurgeMove(const char *msg) {
    if (homingState != HOMING_NONE || feedState != FEED_STANDBY || !jogDone || active_dispense_INJECTION_ongoing) {
        sendStatus(STATUS_PREFIX_ERROR, "Error: Operation already in progress or motors busy.");
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
        
        activeFeedCommand = CMD_STR_PURGE_MOVE;
        
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
    activeFeedCommand = nullptr;
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
    activeFeedCommand = nullptr;
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