#include "ClearCore.h"
#include "machine_1.h"    // For Systemmachine_1 and enums
#include "motor.h"     // For motor control functions and home reference externs
#include "messages.h"  // For sendToPC, communication handling, and potentially finalizeAndReset...
#include <math.h>      // For fabs
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_HOMING_DURATION_MS 2000000 // 30 seconds, adjust as needed

// These are defined in motor.cpp and externed in motor.h
// extern int32_t machineHomeReferenceSteps; // Accessed in messages.cpp
// extern int32_t cartridgeHomeReferenceSteps; // Accessed in messages.cpp


int main(void)
{
	Spinny_Spin_Machine machine_1;

	//SetupUsbSerial();
	//Delay_ms(5000);
	//ConnectorUsb.SendLine("main: Delay finished. Proceeding with setup...");

	SetupEthernet();
	SetupMotors();

	uint32_t now;
	uint32_t last_message_time = Milliseconds();
	int david_message_interval = 1000;
	

	while (true)
	{
		checkUdpBuffer(&machine_1);
		now = Milliseconds();
		sendTelem(&machine_1);
		
		switch (machine_1.mainState)
		{
			case DAVID_STATE:
			switch (machine_1.davidState)
				case HUNGRY_FOR_TACOS
					if (now - last_message_time > david_message_interval){
						sendToPC("David's here!  And he's hungry!");
						last_message_time = Milliseconds();
					} //if
				break;
				case SAD
					if (now - last_message_time > david_message_interval){
						sendToPC("David is sad :(");
						last_message_time = Milliseconds();
					} //if
				break;
				case SLEEPY
					if (now - last_message_time > david_message_interval){
						sendToPC("zzzzzzzzzz");
						last_message_time = Milliseconds();
					} //if		
				break;
				case HAPPY
					if (now - last_message_time > david_message_interval){
						sendToPC("David is happy!");
						last_message_time = Milliseconds();
					} //if
				break;
			
			break;
			
			case STANDBY_MODE:
			// If transitioning to STANDBY and an operation was active, it should have been finalized by its handler.
			// No active operations expected here.
			break;
			
			case TEST_MODE:
			break;

			case HOMING_MODE: {
				if (machine_1.currentHomingPhase == HOMING_PHASE_COMPLETE || machine_1.currentHomingPhase == HOMING_PHASE_ERROR) {
					if (machine_1.currentHomingPhase == HOMING_PHASE_COMPLETE) {
						sendToPC("Homing sequence complete. Returning to STANDBY_MODE.");
						} else {
						sendToPC("Homing sequence ended with error. Returning to STANDBY_MODE.");
					}
					machine_1.mainState = STANDBY_MODE;
					machine_1.homingState = HOMING_NONE;
					machine_1.currentHomingPhase = HOMING_PHASE_IDLE;
					// homingMachineDone/homingCartridgeDone set by onHomingMachineDone/onHomingCartridgeDone
					break;
				}

				if (machine_1.homingState == HOMING_NONE && machine_1.currentHomingPhase == HOMING_PHASE_IDLE) {
					break;
				}

				if ((machine_1.homingState == HOMING_MACHINE || machine_1.homingState == HOMING_CARTRIDGE) &&
				(machine_1.currentHomingPhase >= HOMING_PHASE_RAPID_MOVE && machine_1.currentHomingPhase < HOMING_PHASE_COMPLETE)) {

					if (now - machine_1.homingStartTime > MAX_HOMING_DURATION_MS) {
						sendToPC("Homing Error: Timeout. Aborting.");
						abortMove();
						machine_1.errorState = ERROR_HOMING_TIMEOUT;
						machine_1.currentHomingPhase = HOMING_PHASE_ERROR;
						break;
					}

					bool is_machine_homing = (machine_1.homingState == HOMING_MACHINE);
					int direction = is_machine_homing ? -1 : 1;

					if (machine_1.currentHomingPhase == HOMING_PHASE_RAPID_MOVE || machine_1.currentHomingPhase == HOMING_PHASE_TOUCH_OFF) {
						if (checkTorqueLimit()) {
							if (machine_1.currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
								{
									sendToPC(is_machine_homing ? "Machine Homing: Torque (RAPID). Backing off."
									: "Cartridge Homing: Torque (RAPID). Backing off.");
									machine_1.currentHomingPhase = HOMING_PHASE_BACK_OFF;
									long back_off_s = -direction * Systemmachine_1::HOMING_DEFAULT_BACK_OFF_STEPS;
									moveMotors(back_off_s, back_off_s, (int)machine_1.homing_torque_percent_param,
									machine_1.homing_actual_touch_sps, machine_1.homing_actual_accel_sps2);
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
									machine_1.currentHomingPhase = HOMING_PHASE_RETRACT;
									long retract_s = -direction * machine_1.homing_actual_retract_steps;
									moveMotors(retract_s, retract_s, (int)machine_1.homing_torque_percent_param,
									machine_1.homing_actual_rapid_sps, machine_1.homing_actual_accel_sps2);
								}
							}
							break;
						}
					}

					if (!checkMoving()) {
						switch (machine_1.currentHomingPhase) {
							case HOMING_PHASE_RAPID_MOVE: {
								sendToPC(is_machine_homing ? "Machine Homing Err: No torque in RAPID."
								: "Cartridge Homing Err: No torque in RAPID.");
								machine_1.errorState = ERROR_HOMING_NO_TORQUE_RAPID;
								machine_1.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;
							case HOMING_PHASE_BACK_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing: Back-off done. Touching off."
								: "Cartridge Homing: Back-off done. Touching off.");
								machine_1.currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
								long touch_off_move_length = Systemmachine_1::HOMING_DEFAULT_BACK_OFF_STEPS * 2;
								if (Systemmachine_1::HOMING_DEFAULT_BACK_OFF_STEPS == 0) {
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
								(int)machine_1.homing_torque_percent_param,
								machine_1.homing_actual_touch_sps, machine_1.homing_actual_accel_sps2);
							} break;
							case HOMING_PHASE_TOUCH_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing Err: No torque in TOUCH_OFF."
								: "Cartridge Homing Err: No torque in TOUCH_OFF.");
								machine_1.errorState = ERROR_HOMING_NO_TORQUE_TOUCH;
								machine_1.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;
							case HOMING_PHASE_RETRACT: {
								sendToPC(is_machine_homing ? "Machine Homing: RETRACT complete. Success."
								: "Cartridge Homing: RETRACT complete. Success.");
								if (is_machine_homing) {
									machine_1.onHomingMachineDone();
									} else {
									machine_1.onHomingCartridgeDone();
								}
								machine_1.errorState = ERROR_NONE;
								machine_1.currentHomingPhase = HOMING_PHASE_COMPLETE;
							} break;
							default:
							sendToPC("Homing Wrn: Unexpected phase with stopped motors.");
							machine_1.errorState = ERROR_INVALID_OPERATION; // Changed from MANUAL_ABORT
							machine_1.currentHomingPhase = HOMING_PHASE_ERROR;
							break;
						}
					}
				}
			}
			break;
			
			case FEED_MODE: {
				// Torque check during active feed operations
				if (machine_1.feedState == FEED_INJECT_ACTIVE || machine_1.feedState == FEED_PURGE_ACTIVE ||
				machine_1.feedState == FEED_MOVING_TO_HOME || machine_1.feedState == FEED_MOVING_TO_RETRACT ||
				machine_1.feedState == FEED_INJECT_RESUMING || machine_1.feedState == FEED_PURGE_RESUMING ) {
					if (checkTorqueLimit()) {
						// abortMove() is called by checkTorqueLimit
						machine_1.errorState = ERROR_TORQUE_ABORT;
						sendToPC("FEED_MODE: Torque limit! Operation stopped.");
						finalizeAndResetActiveDispenseOperation(&machine_1, false); // false = not successful completion
						machine_1.feedState = FEED_STANDBY;
						machine_1.feedingDone = true;
					}
				}

				// Handling completion of non-blocking feed operations or state transitions
				if (!checkMoving()) { // If motors are confirmed stopped
					if (!machine_1.feedingDone) { // And the current operation segment isn't marked as done yet
						
						// Finalize dispense calculation for INJECT/PURGE if they just stopped
						if (machine_1.feedState == FEED_INJECT_ACTIVE || machine_1.feedState == FEED_PURGE_ACTIVE) {
							if (machine_1.active_dispense_operation_ongoing) {
								if (machine_1.active_op_steps_per_ml > 0.0001f) {
									long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - machine_1.active_op_segment_initial_axis_steps;
									float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / machine_1.active_op_steps_per_ml;
									machine_1.active_op_total_dispensed_ml += segment_dispensed_ml;
								}
								// Check if total target steps/volume met (active_op_remaining_steps should be 0 or close)
								machine_1.last_completed_dispense_ml = machine_1.active_op_total_dispensed_ml;
								// fullyResetActiveDispenseOperation will be called below by FEED_OPERATION_COMPLETED
							}
							sendToPC("Feed Op: Inject/Purge segment/operation completed.");
							machine_1.feedState = FEED_OPERATION_COMPLETED; // Transition to completed
							// feedingDone will be set below
						}
						else if (machine_1.feedState == FEED_MOVING_TO_HOME || machine_1.feedState == FEED_MOVING_TO_RETRACT) {
							sendToPC("Feed Op: Positioning move completed.");
							machine_1.feedState = FEED_OPERATION_COMPLETED; // Transition to completed
							// feedingDone will be set below
						}
						// For PAUSED machine_1, they only transition out via RESUME or CANCEL commands, not !checkMoving() alone
						
						// If any operation that sets feedingDone=false has completed:
						if (machine_1.feedState == FEED_OPERATION_COMPLETED) {
							machine_1.feedingDone = true; // Mark the overarching operation as done
							machine_1.onFeedingDone();    // Call the handler if it does anything
							finalizeAndResetActiveDispenseOperation(&machine_1, true); // true = successful completion
							machine_1.feedState = FEED_STANDBY; // Finally, return to standby
							sendToPC("Feed Op: Finalized. Returning to Feed Standby.");
						}
					}
					} else { // Motors are moving
					// Transition from STARTING/RESUMING to ACTIVE once motion has begun
					if (machine_1.feedState == FEED_INJECT_STARTING) {
						machine_1.feedState = FEED_INJECT_ACTIVE;
						machine_1.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Inject Active.");
						} else if (machine_1.feedState == FEED_PURGE_STARTING) {
						machine_1.feedState = FEED_PURGE_ACTIVE;
						machine_1.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Purge Active.");
						} else if (machine_1.feedState == FEED_INJECT_RESUMING) {
						machine_1.feedState = FEED_INJECT_ACTIVE;
						machine_1.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded(); // Re-baseline for resumed segment
						sendToPC("Feed Op: Inject Resumed, Active.");
						} else if (machine_1.feedState == FEED_PURGE_RESUMING) {
						machine_1.feedState = FEED_PURGE_ACTIVE;
						machine_1.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Purge Resumed, Active.");
					}
				}

				// Handle transitions from machine_1 set by command handlers (like PAUSED or CANCELLED)
				if ((machine_1.feedState == FEED_INJECT_PAUSED || machine_1.feedState == FEED_PURGE_PAUSED) &&
				!machine_1.feedingDone && !checkMoving()) {

					long current_pos = ConnectorM0.PositionRefCommanded();
					long steps_moved_this_segment = current_pos - machine_1.active_op_segment_initial_axis_steps;

					// Ensure steps_per_ml is valid
					if (machine_1.active_op_steps_per_ml > 0.0001f) {
						float segment_dispensed_ml = fabs(steps_moved_this_segment) / machine_1.active_op_steps_per_ml;
						machine_1.active_op_total_dispensed_ml += segment_dispensed_ml;

						long total_steps_dispensed = (long)(machine_1.active_op_total_dispensed_ml * machine_1.active_op_steps_per_ml);
						machine_1.active_op_remaining_steps = machine_1.active_op_total_target_steps - total_steps_dispensed;

						if (machine_1.active_op_remaining_steps < 0) machine_1.active_op_remaining_steps = 0;

						char debugMsg[200];
						snprintf(debugMsg, sizeof(debugMsg),
						"Paused Calculation: stepsMoved=%ld, totalDispensed=%.3fml, remainingSteps=%ld",
						steps_moved_this_segment, machine_1.active_op_total_dispensed_ml,
						machine_1.active_op_remaining_steps);
						sendToPC(debugMsg);
					}

					machine_1.feedingDone = true;  // Mark segment calculation complete
					sendToPC("Feed Op: Operation Paused. Waiting for Resume/Cancel.");
				}
			} break;

			case JOG_MODE:
			if (checkTorqueLimit()) {
				machine_1.mainState = STANDBY_MODE;
				machine_1.errorState = ERROR_TORQUE_ABORT;
				sendToPC("JOG_MODE: Torque limit, returning to STANDBY.");
				} else if (!checkMoving() && !machine_1.jogDone) { // Assuming jogDone is set to false on jog start
				// machine_1.jogDone = true; // Optional: mark jog as complete if needed
				// machine_1.onJogDone();
			}
			break;

			case DISABLED_MODE:
			// Only ENABLE command should change state from here
			break;
		} // end switch
	} // end while
} // end main







