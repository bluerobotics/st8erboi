#include "injector.h"

Injector injector;

void Injector::updateState() {		
	switch (mainState)
	{
		case STANDBY_MODE:
		// No active operations expected here.
		break;

		case HOMING_MODE: {
			// This top-level check handles the end-of-sequence for success or error
			if (currentHomingPhase == HOMING_PHASE_COMPLETE || currentHomingPhase == HOMING_PHASE_ERROR) {
				if (currentHomingPhase == HOMING_PHASE_COMPLETE) {
					sendStatus(STATUS_PREFIX_DONE, "Homing sequence complete. Returning to STANDBY_MODE.");
				} else {
					sendStatus(STATUS_PREFIX_ERROR,"Homing sequence ended with error. Returning to STANDBY_MODE.");
				}
				mainState = STANDBY_MODE;
				homingState = HOMING_NONE;
				currentHomingPhase = HOMING_PHASE_IDLE;
				break;
			}

			if (homingState != HOMING_NONE) {
                // --- FIX: Re-sample the current time right before using it for comparison ---
                uint32_t current_time = Milliseconds();
				if (current_time - homingStartTime > MAX_HOMING_DURATION_MS) {
					sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Timeout. Aborting.");
					abortInjectorMove();
					errorState = ERROR_HOMING_TIMEOUT;
					currentHomingPhase = HOMING_PHASE_ERROR;
					break;
				}
			}

			bool is_machine_homing = (homingState == HOMING_MACHINE);
			int direction = is_machine_homing ? -1 : 1;

			// Use a switch to handle each specific phase of the homing operation
			switch (currentHomingPhase) {
				case HOMING_PHASE_IDLE:
					// Do nothing, waiting for a homing command
					break;
						
				case HOMING_PHASE_STARTING_MOVE: {
					// --- FIX: Use a freshly sampled time for this check as well ---
                    uint32_t current_time = Milliseconds();
					if (current_time - homingStartTime > 50) {
						if (checkInjectorMoving()) {
							sendStatus(STATUS_PREFIX_START, "Homing: Move started. Entering RAPID_MOVE phase.");
							currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
						} else {
							sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Motor failed to start moving.");
							errorState = ERROR_HOMING_NO_TORQUE_RAPID;
							currentHomingPhase = HOMING_PHASE_ERROR;
						}
					}
					break;
				}

				case HOMING_PHASE_RAPID_MOVE:
				case HOMING_PHASE_TOUCH_OFF: {
					if (checkInjectorTorqueLimit()) {
						if (currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
							sendStatus(is_machine_homing ? STATUS_PREFIX_INFO, "Machine Homing: Torque (RAPID). Backing off." : STATUS_PREFIX_INFO, "Cartridge Homing: Torque (RAPID). Backing off.");
							currentHomingPhase = HOMING_PHASE_BACK_OFF;
							long back_off_s = -direction * homingDefaultBackoffSteps;
							moveInjectorMotors(back_off_s, back_off_s, (int)homing_torque_percent_param, homing_actual_touch_sps, homing_actual_accel_sps2);
						} else { 
							sendStatus(is_machine_homing ? STATUS_PREFIX_INFO, "Machine Homing: Torque (TOUCH_OFF). Zeroing & Retracting." : STATUS_PREFIX_INFO, "Cartridge Homing: Torque (TOUCH_OFF). Zeroing & Retracting.");
							if (is_machine_homing) {
								machineHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
								sendStatus(STATUS_PREFIX_INFO, "Machine home reference point set.");
							} else {
								cartridgeHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
								sendStatus(STATUS_PREFIX_INFO, "Cartridge home reference point set.");
							}
							currentHomingPhase = HOMING_PHASE_RETRACT;
							long retract_s = -direction * homing_actual_retract_steps;
							moveInjectorMotors(retract_s, retract_s, (int)homing_torque_percent_param, homing_actual_rapid_sps, homing_actual_accel_sps2);
						}
					} else if (!checkInjectorMoving()) {
						// If motors stopped but torque limit wasn't hit, it's an error.
						sendStatus(currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? STATUS_PREFIX_ERROR, "Machine Homing Err: No torque in RAPID." : STATUS_PREFIX_ERROR, "Machine Homing Err: No torque in TOUCH_OFF.");
						errorState = currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? ERROR_HOMING_NO_TORQUE_RAPID : ERROR_HOMING_NO_TORQUE_TOUCH;
						currentHomingPhase = HOMING_PHASE_ERROR;
					}
					break;
				}

				case HOMING_PHASE_BACK_OFF: {
					if (!checkInjectorMoving()) {
						sendStatus(is_machine_homing ? STATUS_PREFIX_INFO, "Machine Homing: Back-off done. Touching off." : STATUS_PREFIX_INFO, "Cartridge Homing: Back-off done. Touching off.");
						currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
						long touch_off_move_length = homingDefaultBackoffSteps * 2;
						long final_touch_off_move_steps = direction * touch_off_move_length;
						moveInjectorMotors(final_touch_off_move_steps, final_touch_off_move_steps, (int)homing_torque_percent_param, homing_actual_touch_sps, homing_actual_accel_sps2);
					}
					break;
				}
						
				case HOMING_PHASE_RETRACT: {
					if (!checkInjectorMoving()) {
						sendStatus(is_machine_homing ? STATUS_PREFIX_DONE, "Machine Homing: RETRACT complete. Success." : STATUS_PREFIX_DONE, "Cartridge Homing: RETRACT complete. Success.");
						if (is_machine_homing) {
							onHomingMachineDone();
						} else {
							onHomingCartridgeDone();
						}
						errorState = ERROR_NONE;
						currentHomingPhase = HOMING_PHASE_COMPLETE;
					}
					break;
				}

				default:
					break;
			}
		}
		break;
			
		case FEED_MODE: {
			if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE ||
			feedState == FEED_MOVING_TO_HOME || feedState == FEED_MOVING_TO_RETRACT ||
			feedState == FEED_INJECT_RESUMING || feedState == FEED_PURGE_RESUMING ) {
				if (checkInjectorTorqueLimit()) {
					errorState = ERROR_TORQUE_ABORT;
					sendStatus(STATUS_PREFIX_ERROR,"FEED_MODE: Torque limit! Operation stopped.");
					finalizeAndResetActiveDispenseOperation(false);
					feedState = FEED_STANDBY;
					feedingDone = true;
				}
			}

			if (!checkInjectorMoving()) {
				if (!feedingDone) {
						
					if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE) {
						if (active_dispense_INJECTION_ongoing) {
							if (active_op_steps_per_ml > 0.0001f) {
								long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - active_op_segment_initial_axis_steps;
								float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / active_op_steps_per_ml;
								active_op_total_dispensed_ml += segment_dispensed_ml;
							}
							last_completed_dispense_ml = active_op_total_dispensed_ml;
						}
						sendStatus(STATUS_PREFIX_DONE, "Feed Op: Inject/Purge segment/operation completed.");
						feedState = FEED_INJECTION_COMPLETED;
					}
					else if (feedState == FEED_MOVING_TO_HOME || feedState == FEED_MOVING_TO_RETRACT) {
						sendStatus(STATUS_PREFIX_DONE, "Feed Op: Positioning move completed.");
						feedState = FEED_INJECTION_COMPLETED;
					}
						
					if (feedState == FEED_INJECTION_COMPLETED) {
						feedingDone = true;
						onFeedingDone();
						finalizeAndResetActiveDispenseOperation(true);
						feedState = FEED_STANDBY;
						sendStatus(STATUS_PREFIX_DONE, "Feed Op: Finalized. Returning to Feed Standby.");
					}
				}
				} else { // Motors are moving
				if (feedState == FEED_INJECT_STARTING) {
					feedState = FEED_INJECT_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Inject Active.");
					} else if (feedState == FEED_PURGE_STARTING) {
					feedState = FEED_PURGE_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Purge Active.");
					} else if (feedState == FEED_INJECT_RESUMING) {
					feedState = FEED_INJECT_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Inject Resumed, Active.");
					} else if (feedState == FEED_PURGE_RESUMING) {
					feedState = FEED_PURGE_ACTIVE;
					active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
					sendStatus(STATUS_PREFIX_INFO, "Feed Op: Purge Resumed, Active.");
				}
			}

			if ((feedState == FEED_INJECT_PAUSED || feedState == FEED_PURGE_PAUSED) &&
			!feedingDone && !checkInjectorMoving()) {

				long current_pos = ConnectorM0.PositionRefCommanded();
				long steps_moved_this_segment = current_pos - active_op_segment_initial_axis_steps;

				if (active_op_steps_per_ml > 0.0001f) {
					float segment_dispensed_ml = fabs(steps_moved_this_segment) / active_op_steps_per_ml;
					active_op_total_dispensed_ml += segment_dispensed_ml;

					long total_steps_dispensed = (long)(active_op_total_dispensed_ml * active_op_steps_per_ml);
					active_op_remaining_steps = active_op_total_target_steps - total_steps_dispensed;

					if (active_op_remaining_steps < 0) active_op_remaining_steps = 0;
				}

				feedingDone = true;
				sendStatus(STATUS_PREFIX_INFO, "Feed Op: Operation Paused. Waiting for Resume/Cancel.");
			}
		} break;

		case JOG_MODE:
		if (checkInjectorTorqueLimit()) {
			mainState = STANDBY_MODE;
			errorState = ERROR_TORQUE_ABORT;
			sendStatus(STATUS_PREFIX_INFO, "JOG_MODE: Torque limit, returning to STANDBY.");
			} else if (!checkInjectorMoving() && !jogDone) {
				jogDone = true;
		}
		break;

		case DISABLED_MODE:
		// Only ENABLE command should change state from here
		break;
            
        default:
        // This case handles any unhandled enum values (like ..._COUNT) and prevents a compiler warning.
        break;
	}
}

void Injector::loop() {
    processUdp();
    updateState();
}

int main(void) {
    // Perform one-time setup
    injector.setup();

    // Main non-blocking application loop
    while (true) {
        // Just call the main loop handler for our fillhead object.
        injector.loop();
    }
}
		
		