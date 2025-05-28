#include "ClearCore.h"
#include "states.h"    // For SystemStates and enums
#include "motor.h"     // For motor control functions and home reference externs
#include "messages.h"  // For sendToPC, communication handling, and potentially finalizeAndReset...
#include <math.h>      // For fabs
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_HOMING_DURATION_MS 30000 // 30 seconds, adjust as needed

// These are defined in motor.cpp and externed in motor.h
// extern int32_t machineHomeReferenceSteps; // Accessed in messages.cpp
// extern int32_t cartridgeHomeReferenceSteps; // Accessed in messages.cpp


int main(void)
{
	SystemStates states;

	SetupUsbSerial();
	Delay_ms(5000);
	ConnectorUsb.SendLine("main: Delay finished. Proceeding with setup...");

	SetupEthernet();
	SetupMotors();

	
	uint32_t now = Milliseconds();
	uint32_t lastMotorTime = now;
	uint32_t motorInterval = 2000;
	int motorFlip = 1;

	while (true)
	{
		checkUdpBuffer(&states);
		now = Milliseconds();
		sendTelem(&states);
		
		switch (states.mainState)
		{
			case STANDBY_MODE:
			// If transitioning to STANDBY and an operation was active, it should have been finalized by its handler.
			// No active operations expected here.
			break;
			
			case TEST_MODE:
			if (checkTorqueLimit()) {
				states.mainState = STANDBY_MODE;
				states.errorState = ERROR_TORQUE_ABORT;
				sendToPC("TEST_MODE: Torque limit, returning to STANDBY.");
			}
			if (now - lastMotorTime > motorInterval) {
				moveMotors(motorFlip * 800, motorFlip * 800, 20, 800, 5000);
				motorFlip *= -1;
				lastMotorTime = now;
			}
			break;

			case HOMING_MODE: {
				if (states.currentHomingPhase == HOMING_PHASE_COMPLETE || states.currentHomingPhase == HOMING_PHASE_ERROR) {
					if (states.currentHomingPhase == HOMING_PHASE_COMPLETE) {
						sendToPC("Homing sequence complete. Returning to STANDBY_MODE.");
						} else {
						sendToPC("Homing sequence ended with error. Returning to STANDBY_MODE.");
					}
					states.mainState = STANDBY_MODE;
					states.homingState = HOMING_NONE;
					states.currentHomingPhase = HOMING_PHASE_IDLE;
					// homingMachineDone/homingCartridgeDone set by onHomingMachineDone/onHomingCartridgeDone
					break;
				}

				if (states.homingState == HOMING_NONE && states.currentHomingPhase == HOMING_PHASE_IDLE) {
					break;
				}

				if ((states.homingState == HOMING_MACHINE || states.homingState == HOMING_CARTRIDGE) &&
				(states.currentHomingPhase >= HOMING_PHASE_RAPID_MOVE && states.currentHomingPhase < HOMING_PHASE_COMPLETE)) {

					if (now - states.homingStartTime > MAX_HOMING_DURATION_MS) {
						sendToPC("Homing Error: Timeout. Aborting.");
						abortMove();
						states.errorState = ERROR_HOMING_TIMEOUT;
						states.currentHomingPhase = HOMING_PHASE_ERROR;
						break;
					}

					bool is_machine_homing = (states.homingState == HOMING_MACHINE);
					int direction = is_machine_homing ? 1 : -1;

					if (states.currentHomingPhase == HOMING_PHASE_RAPID_MOVE || states.currentHomingPhase == HOMING_PHASE_TOUCH_OFF) {
						if (checkTorqueLimit()) {
							if (states.currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
								{
									sendToPC(is_machine_homing ? "Machine Homing: Torque (RAPID). Backing off."
									: "Cartridge Homing: Torque (RAPID). Backing off.");
									states.currentHomingPhase = HOMING_PHASE_BACK_OFF;
									long back_off_s = -direction * SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS;
									moveMotors(back_off_s, back_off_s, (int)states.homing_torque_percent_param,
									states.homing_actual_touch_sps, states.homing_actual_accel_sps2);
								}
								} else { // Torque hit during HOMING_PHASE_TOUCH_OFF
								{
									sendToPC(is_machine_homing ? "Machine Homing: Torque (TOUCH_OFF). Zeroing & Retracting."
									: "Cartridge Homing: Torque (TOUCH_OFF). Zeroing & Retracting.");
									if (is_machine_homing) {
										machineHomeReferenceSteps = ConnectorM0.PositionRefCommanded();
										sendToPC("Machine home reference point set.");
										} else {
										cartridgeHomeReferenceSteps = ConnectorM0.PositionRefCommanded(); // Single axis
										sendToPC("Cartridge home reference point set.");
									}
									states.currentHomingPhase = HOMING_PHASE_RETRACT;
									long retract_s = -direction * states.homing_actual_retract_steps;
									moveMotors(retract_s, retract_s, (int)states.homing_torque_percent_param,
									states.homing_actual_rapid_sps, states.homing_actual_accel_sps2);
								}
							}
							break;
						}
					}

					if (!checkMoving()) {
						switch (states.currentHomingPhase) {
							case HOMING_PHASE_RAPID_MOVE: {
								sendToPC(is_machine_homing ? "Machine Homing Err: No torque in RAPID."
								: "Cartridge Homing Err: No torque in RAPID.");
								states.errorState = ERROR_HOMING_NO_TORQUE_RAPID;
								states.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;
							case HOMING_PHASE_BACK_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing: Back-off done. Touching off."
								: "Cartridge Homing: Back-off done. Touching off.");
								states.currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
								long touch_off_move_length = SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS * 2;
								if (SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS == 0) {
									touch_off_move_length = 200;
									sendToPC("Homing Wrn: Default back_off_steps is 0. Using fallback.");
									} else if (touch_off_move_length == 0) {
									touch_off_move_length = 10;
									sendToPC("Homing Wrn: Calculated touch_off_move_length is 0. Using minimal.");
								}
								long final_touch_off_move_steps = direction * touch_off_move_length;
								char log_msg[100];
								snprintf(log_msg, sizeof(log_msg), "Homing: Touch-off approach (steps): %ld", final_touch_off_move_steps);
								sendToPC(log_msg);
								moveMotors(final_touch_off_move_steps, final_touch_off_move_steps,
								(int)states.homing_torque_percent_param,
								states.homing_actual_touch_sps, states.homing_actual_accel_sps2);
							} break;
							case HOMING_PHASE_TOUCH_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing Err: No torque in TOUCH_OFF."
								: "Cartridge Homing Err: No torque in TOUCH_OFF.");
								states.errorState = ERROR_HOMING_NO_TORQUE_TOUCH;
								states.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;
							case HOMING_PHASE_RETRACT: {
								sendToPC(is_machine_homing ? "Machine Homing: RETRACT complete. Success."
								: "Cartridge Homing: RETRACT complete. Success.");
								if (is_machine_homing) {
									states.onHomingMachineDone();
									} else {
									states.onHomingCartridgeDone();
								}
								states.errorState = ERROR_NONE;
								states.currentHomingPhase = HOMING_PHASE_COMPLETE;
							} break;
							default:
							sendToPC("Homing Wrn: Unexpected phase with stopped motors.");
							states.errorState = ERROR_INVALID_OPERATION; // Changed from MANUAL_ABORT
							states.currentHomingPhase = HOMING_PHASE_ERROR;
							break;
						}
					}
				}
			}
			break;
			
			case FEED_MODE: {
				// Torque check during active feed operations
				if (states.feedState == FEED_INJECT_ACTIVE || states.feedState == FEED_PURGE_ACTIVE ||
				states.feedState == FEED_MOVING_TO_HOME || states.feedState == FEED_MOVING_TO_RETRACT ||
				states.feedState == FEED_INJECT_RESUMING || states.feedState == FEED_PURGE_RESUMING ) {
					if (checkTorqueLimit()) {
						// abortMove() is called by checkTorqueLimit
						states.errorState = ERROR_TORQUE_ABORT;
						sendToPC("FEED_MODE: Torque limit! Operation stopped.");
						finalizeAndResetActiveDispenseOperation(&states, false); // false = not successful completion
						states.feedState = FEED_STANDBY;
						states.feedingDone = true;
					}
				}

				// Handling completion of non-blocking feed operations or state transitions
				if (!checkMoving()) { // If motors are confirmed stopped
					if (!states.feedingDone) { // And the current operation segment isn't marked as done yet
						
						// Finalize dispense calculation for INJECT/PURGE if they just stopped
						if (states.feedState == FEED_INJECT_ACTIVE || states.feedState == FEED_PURGE_ACTIVE) {
							if (states.active_dispense_operation_ongoing) {
								if (states.active_op_steps_per_ml > 0.0001f) {
									long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - states.active_op_segment_initial_axis_steps;
									float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / states.active_op_steps_per_ml;
									states.active_op_total_dispensed_ml += segment_dispensed_ml;
								}
								// Check if total target steps/volume met (active_op_remaining_steps should be 0 or close)
								states.last_completed_dispense_ml = states.active_op_total_dispensed_ml;
								// fullyResetActiveDispenseOperation will be called below by FEED_OPERATION_COMPLETED
							}
							sendToPC("Feed Op: Inject/Purge segment/operation completed.");
							states.feedState = FEED_OPERATION_COMPLETED; // Transition to completed
							// feedingDone will be set below
						}
						else if (states.feedState == FEED_MOVING_TO_HOME || states.feedState == FEED_MOVING_TO_RETRACT) {
							sendToPC("Feed Op: Positioning move completed.");
							states.feedState = FEED_OPERATION_COMPLETED; // Transition to completed
							// feedingDone will be set below
						}
						// For PAUSED states, they only transition out via RESUME or CANCEL commands, not !checkMoving() alone
						
						// If any operation that sets feedingDone=false has completed:
						if (states.feedState == FEED_OPERATION_COMPLETED) {
							states.feedingDone = true; // Mark the overarching operation as done
							states.onFeedingDone();    // Call the handler if it does anything
							finalizeAndResetActiveDispenseOperation(&states, true); // true = successful completion
							states.feedState = FEED_STANDBY; // Finally, return to standby
							sendToPC("Feed Op: Finalized. Returning to Feed Standby.");
						}
					}
					} else { // Motors are moving
					// Transition from STARTING/RESUMING to ACTIVE once motion has begun
					if (states.feedState == FEED_INJECT_STARTING) {
						states.feedState = FEED_INJECT_ACTIVE;
						states.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Inject Active.");
						} else if (states.feedState == FEED_PURGE_STARTING) {
						states.feedState = FEED_PURGE_ACTIVE;
						states.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Purge Active.");
						} else if (states.feedState == FEED_INJECT_RESUMING) {
						states.feedState = FEED_INJECT_ACTIVE;
						states.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded(); // Re-baseline for resumed segment
						sendToPC("Feed Op: Inject Resumed, Active.");
						} else if (states.feedState == FEED_PURGE_RESUMING) {
						states.feedState = FEED_PURGE_ACTIVE;
						states.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Purge Resumed, Active.");
					}
				}

				// Handle transitions from states set by command handlers (like PAUSED or CANCELLED)
				if (states.feedState == FEED_INJECT_PAUSED || states.feedState == FEED_PURGE_PAUSED) {
					// System is waiting for RESUME or CANCEL. Motors should be stopped.
					// The dispense calculation up to the point of pause is done in handlePauseOperation or here when !checkMoving after pause cmd.
					if (!checkMoving() && states.active_dispense_operation_ongoing) { // Ensure motors are actually stopped to finalize pause calcs
						if (states.active_op_steps_per_ml > 0.0001f) {
							long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - states.active_op_segment_initial_axis_steps;
							float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / states.active_op_steps_per_ml;
							states.active_op_total_dispensed_ml += segment_dispensed_ml; // Accumulate before pause
							// Calculate remaining steps based on total dispensed
							states.active_op_remaining_steps = states.active_op_total_target_steps - (long)(states.active_op_total_dispensed_ml * states.active_op_steps_per_ml);
							if(states.active_op_remaining_steps < 0) states.active_op_remaining_steps = 0;
						}
						// active_dispense_operation_ongoing remains true
						sendToPC("Feed Op: Operation Paused. Waiting for Resume/Cancel.");
					}
					} else if (states.feedState == FEED_OPERATION_CANCELLED) {
					sendToPC("Feed Op: Cancelled. Returning to Feed Standby.");
					// fullyResetActiveDispenseOperation was called by handleCancelOperation
					states.feedState = FEED_STANDBY;
				}
			} break;

			case JOG_MODE:
			if (checkTorqueLimit()) {
				states.mainState = STANDBY_MODE;
				states.errorState = ERROR_TORQUE_ABORT;
				sendToPC("JOG_MODE: Torque limit, returning to STANDBY.");
				} else if (!checkMoving() && !states.jogDone) { // Assuming jogDone is set to false on jog start
				// states.jogDone = true; // Optional: mark jog as complete if needed
				// states.onJogDone();
			}
			break;

			case DISABLED_MODE:
			// Only ENABLE command should change state from here
			break;
		} // end switch
	} // end while
} // end main