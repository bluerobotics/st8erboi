#include "ClearCore.h"
#include "injector.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_HOMING_DURATION_MS 30000 // 30 seconds

int main(void)
{
	Injector injector;
	
	injector.setupEthernet();
	injector.setupInjectorMotors();
	
	uint32_t now = Milliseconds();

	while (true)
	{
		now = Milliseconds();
		injector.checkUdpBuffer();
		injector.sendTelem();
		
		switch (injector.mainState)
		{
			case STANDBY_MODE:
			// No active operations expected here.
			break;

			case HOMING_MODE: {
				// This top-level check handles the end-of-sequence for success or error
				if (injector.currentHomingPhase == HOMING_PHASE_COMPLETE || injector.currentHomingPhase == HOMING_PHASE_ERROR) {
					if (injector.currentHomingPhase == HOMING_PHASE_COMPLETE) {
						injector.sendToPC("Homing sequence complete. Returning to STANDBY_MODE.");
					} else {
						injector.sendToPC("Homing sequence ended with error. Returning to STANDBY_MODE.");
					}
					injector.mainState = STANDBY_MODE;
					injector.homingState = HOMING_NONE;
					injector.currentHomingPhase = HOMING_PHASE_IDLE;
					break;
				}

				if (injector.homingState != HOMING_NONE) {
                    // --- FIX: Re-sample the current time right before using it for comparison ---
                    uint32_t current_time = Milliseconds();
					if (current_time - injector.homingStartTime > MAX_HOMING_DURATION_MS) {
						injector.sendToPC("Homing Error: Timeout. Aborting.");
						injector.abortInjectorMove();
						injector.errorState = ERROR_HOMING_TIMEOUT;
						injector.currentHomingPhase = HOMING_PHASE_ERROR;
						break;
					}
				}

				bool is_machine_homing = (injector.homingState == HOMING_MACHINE);
				int direction = is_machine_homing ? -1 : 1;

				// Use a switch to handle each specific phase of the homing operation
				switch (injector.currentHomingPhase) {
					case HOMING_PHASE_IDLE:
						// Do nothing, waiting for a homing command
						break;
						
					case HOMING_PHASE_STARTING_MOVE: {
						// --- FIX: Use a freshly sampled time for this check as well ---
                        uint32_t current_time = Milliseconds();
						if (current_time - injector.homingStartTime > 50) {
							if (injector.checkInjectorMoving()) {
								injector.sendToPC("Homing: Move started. Entering RAPID_MOVE phase.");
								injector.currentHomingPhase = HOMING_PHASE_RAPID_MOVE;
							} else {
								injector.sendToPC("Homing Error: Motor failed to start moving.");
								injector.errorState = ERROR_HOMING_NO_TORQUE_RAPID;
								injector.currentHomingPhase = HOMING_PHASE_ERROR;
							}
						}
						break;
					}

					case HOMING_PHASE_RAPID_MOVE:
					case HOMING_PHASE_TOUCH_OFF: {
						if (injector.checkInjectorTorqueLimit()) {
							if (injector.currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
								injector.sendToPC(is_machine_homing ? "Machine Homing: Torque (RAPID). Backing off." : "Cartridge Homing: Torque (RAPID). Backing off.");
								injector.currentHomingPhase = HOMING_PHASE_BACK_OFF;
								long back_off_s = -direction * injector.homingDefaultBackoffSteps;
								injector.moveInjectorMotors(back_off_s, back_off_s, (int)injector.homing_torque_percent_param, injector.homing_actual_touch_sps, injector.homing_actual_accel_sps2);
							} else { 
								injector.sendToPC(is_machine_homing ? "Machine Homing: Torque (TOUCH_OFF). Zeroing & Retracting." : "Cartridge Homing: Torque (TOUCH_OFF). Zeroing & Retracting.");
								if (is_machine_homing) {
									injector.machineHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
									injector.sendToPC("Machine home reference point set.");
								} else {
									injector.cartridgeHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
									injector.sendToPC("Cartridge home reference point set.");
								}
								injector.currentHomingPhase = HOMING_PHASE_RETRACT;
								long retract_s = -direction * injector.homing_actual_retract_steps;
								injector.moveInjectorMotors(retract_s, retract_s, (int)injector.homing_torque_percent_param, injector.homing_actual_rapid_sps, injector.homing_actual_accel_sps2);
							}
						} else if (!injector.checkInjectorMoving()) {
							// If motors stopped but torque limit wasn't hit, it's an error.
							injector.sendToPC(injector.currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? "Machine Homing Err: No torque in RAPID." : "Machine Homing Err: No torque in TOUCH_OFF.");
							injector.errorState = injector.currentHomingPhase == HOMING_PHASE_RAPID_MOVE ? ERROR_HOMING_NO_TORQUE_RAPID : ERROR_HOMING_NO_TORQUE_TOUCH;
							injector.currentHomingPhase = HOMING_PHASE_ERROR;
						}
						break;
					}

					case HOMING_PHASE_BACK_OFF: {
						if (!injector.checkInjectorMoving()) {
							injector.sendToPC(is_machine_homing ? "Machine Homing: Back-off done. Touching off." : "Cartridge Homing: Back-off done. Touching off.");
							injector.currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
							long touch_off_move_length = injector.homingDefaultBackoffSteps * 2;
							long final_touch_off_move_steps = direction * touch_off_move_length;
							injector.moveInjectorMotors(final_touch_off_move_steps, final_touch_off_move_steps, (int)injector.homing_torque_percent_param, injector.homing_actual_touch_sps, injector.homing_actual_accel_sps2);
						}
						break;
					}
						
					case HOMING_PHASE_RETRACT: {
						if (!injector.checkInjectorMoving()) {
							injector.sendToPC(is_machine_homing ? "Machine Homing: RETRACT complete. Success." : "Cartridge Homing: RETRACT complete. Success.");
							if (is_machine_homing) {
								injector.onHomingMachineDone();
							} else {
								injector.onHomingCartridgeDone();
							}
							injector.errorState = ERROR_NONE;
							injector.currentHomingPhase = HOMING_PHASE_COMPLETE;
						}
						break;
					}

					default:
						break;
				}
			}
			break;
			
			case FEED_MODE: {
				if (injector.feedState == FEED_INJECT_ACTIVE || injector.feedState == FEED_PURGE_ACTIVE ||
				injector.feedState == FEED_MOVING_TO_HOME || injector.feedState == FEED_MOVING_TO_RETRACT ||
				injector.feedState == FEED_INJECT_RESUMING || injector.feedState == FEED_PURGE_RESUMING ) {
					if (injector.checkInjectorTorqueLimit()) {
						injector.errorState = ERROR_TORQUE_ABORT;
						injector.sendToPC("FEED_MODE: Torque limit! Operation stopped.");
						injector.finalizeAndResetActiveDispenseOperation(false);
						injector.feedState = FEED_STANDBY;
						injector.feedingDone = true;
					}
				}

				if (!injector.checkInjectorMoving()) {
					if (!injector.feedingDone) {
						
						if (injector.feedState == FEED_INJECT_ACTIVE || injector.feedState == FEED_PURGE_ACTIVE) {
							if (injector.active_dispense_INJECTION_ongoing) {
								if (injector.active_op_steps_per_ml > 0.0001f) {
									long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - injector.active_op_segment_initial_axis_steps;
									float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / injector.active_op_steps_per_ml;
									injector.active_op_total_dispensed_ml += segment_dispensed_ml;
								}
								injector.last_completed_dispense_ml = injector.active_op_total_dispensed_ml;
							}
							injector.sendToPC("Feed Op: Inject/Purge segment/operation completed.");
							injector.feedState = FEED_INJECTION_COMPLETED;
						}
						else if (injector.feedState == FEED_MOVING_TO_HOME || injector.feedState == FEED_MOVING_TO_RETRACT) {
							injector.sendToPC("Feed Op: Positioning move completed.");
							injector.feedState = FEED_INJECTION_COMPLETED;
						}
						
						if (injector.feedState == FEED_INJECTION_COMPLETED) {
							injector.feedingDone = true;
							injector.onFeedingDone();
							injector.finalizeAndResetActiveDispenseOperation(true);
							injector.feedState = FEED_STANDBY;
							injector.sendToPC("Feed Op: Finalized. Returning to Feed Standby.");
						}
					}
					} else { // Motors are moving
					if (injector.feedState == FEED_INJECT_STARTING) {
						injector.feedState = FEED_INJECT_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						injector.sendToPC("Feed Op: Inject Active.");
						} else if (injector.feedState == FEED_PURGE_STARTING) {
						injector.feedState = FEED_PURGE_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						injector.sendToPC("Feed Op: Purge Active.");
						} else if (injector.feedState == FEED_INJECT_RESUMING) {
						injector.feedState = FEED_INJECT_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						injector.sendToPC("Feed Op: Inject Resumed, Active.");
						} else if (injector.feedState == FEED_PURGE_RESUMING) {
						injector.feedState = FEED_PURGE_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						injector.sendToPC("Feed Op: Purge Resumed, Active.");
					}
				}

				if ((injector.feedState == FEED_INJECT_PAUSED || injector.feedState == FEED_PURGE_PAUSED) &&
				!injector.feedingDone && !injector.checkInjectorMoving()) {

					long current_pos = ConnectorM0.PositionRefCommanded();
					long steps_moved_this_segment = current_pos - injector.active_op_segment_initial_axis_steps;

					if (injector.active_op_steps_per_ml > 0.0001f) {
						float segment_dispensed_ml = fabs(steps_moved_this_segment) / injector.active_op_steps_per_ml;
						injector.active_op_total_dispensed_ml += segment_dispensed_ml;

						long total_steps_dispensed = (long)(injector.active_op_total_dispensed_ml * injector.active_op_steps_per_ml);
						injector.active_op_remaining_steps = injector.active_op_total_target_steps - total_steps_dispensed;

						if (injector.active_op_remaining_steps < 0) injector.active_op_remaining_steps = 0;

						char debugMsg[200];
						snprintf(debugMsg, sizeof(debugMsg),
						"Paused Calculation: stepsMoved=%ld, totalDispensed=%.3fml, remainingSteps=%ld",
						steps_moved_this_segment, injector.active_op_total_dispensed_ml,
						injector.active_op_remaining_steps);
						injector.sendToPC(debugMsg);
					}

					injector.feedingDone = true;
					injector.sendToPC("Feed Op: Operation Paused. Waiting for Resume/Cancel.");
				}
			} break;

			case JOG_MODE:
			if (injector.checkInjectorTorqueLimit()) {
				injector.mainState = STANDBY_MODE;
				injector.errorState = ERROR_TORQUE_ABORT;
				injector.sendToPC("JOG_MODE: Torque limit, returning to STANDBY.");
				} else if (!injector.checkInjectorMoving() && !injector.jogDone) {
				 injector.jogDone = true;
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
}