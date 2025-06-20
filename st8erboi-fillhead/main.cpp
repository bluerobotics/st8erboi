#include "ClearCore.h"
#include "Ethernet.h"  // For sendToPC, communication handling
#include "InjectorStates.h"     // For Injector and enums
#include "InjectorMotors.h"     // For motor control functions and home reference externs
#include "InjectorMessages.h"   // Communication handling
#include "InjectorHandlers.h"   // Communication handling
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_HOMING_DURATION_MS 2000000 // 30 seconds, adjust as needed

// These are defined in motor.cpp and externed in motor.h
// extern int32_t machineHomeReferenceSteps; // Accessed in messages.cpp
// extern int32_t cartridgeHomeReferenceSteps; // Accessed in messages.cpp


int main(void)
{
	Injector injector;
	
	//SetupUsbSerial();
	//Delay_ms(5000);
	//ConnectorUsb.SendLine("main: Delay finished. Proceeding with setup...");

	SetupEthernet();
	setupInjectorMotors();
	
	uint32_t now = Milliseconds();

	while (true)
	{
		checkUdpBuffer(&injector);
		now = Milliseconds();
		sendTelem(&injector);
		
		switch (injector.mainState)
		{
			case STANDBY_MODE:
			// If transitioning to STANDBY and an operation was active, it should have been finalized by its handler.
			// No active operations expected here.
			break;

			case HOMING_MODE: {
				if (injector.currentHomingPhase == HOMING_PHASE_COMPLETE || injector.currentHomingPhase == HOMING_PHASE_ERROR) {
					if (injector.currentHomingPhase == HOMING_PHASE_COMPLETE) {
						sendToPC("Homing sequence complete. Returning to STANDBY_MODE.");
						} else {
						sendToPC("Homing sequence ended with error. Returning to STANDBY_MODE.");
					}
					injector.mainState = STANDBY_MODE;
					injector.homingState = HOMING_NONE;
					injector.currentHomingPhase = HOMING_PHASE_IDLE;
					// homingMachineDone/homingCartridgeDone set by onHomingMachineDone/onHomingCartridgeDone
					break;
				}

				if (injector.homingState == HOMING_NONE && injector.currentHomingPhase == HOMING_PHASE_IDLE) {
					break;
				}

				if ((injector.homingState == HOMING_MACHINE || injector.homingState == HOMING_CARTRIDGE) &&
				(injector.currentHomingPhase >= HOMING_PHASE_RAPID_MOVE && injector.currentHomingPhase < HOMING_PHASE_COMPLETE)) {

					if (now - injector.homingStartTime > MAX_HOMING_DURATION_MS) {
						sendToPC("Homing Error: Timeout. Aborting.");
						abortInjectorMove();
						injector.errorState = ERROR_HOMING_TIMEOUT;
						injector.currentHomingPhase = HOMING_PHASE_ERROR;
						break;
					}

					bool is_machine_homing = (injector.homingState == HOMING_MACHINE);
					int direction = is_machine_homing ? -1 : 1;

					if (injector.currentHomingPhase == HOMING_PHASE_RAPID_MOVE || injector.currentHomingPhase == HOMING_PHASE_TOUCH_OFF) {
						if (checkInjectorTorqueLimit()) {
							if (injector.currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
								{
									sendToPC(is_machine_homing ? "Machine Homing: Torque (RAPID). Backing off."
									: "Cartridge Homing: Torque (RAPID). Backing off.");
									injector.currentHomingPhase = HOMING_PHASE_BACK_OFF;
									long back_off_s = -direction * Injector::HOMING_DEFAULT_BACK_OFF_STEPS;
									moveInjectorMotors(back_off_s, back_off_s, (int)injector.homing_torque_percent_param,
									injector.homing_actual_touch_sps, injector.homing_actual_accel_sps2);
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
									injector.currentHomingPhase = HOMING_PHASE_RETRACT;
									long retract_s = -direction * injector.homing_actual_retract_steps;
									moveInjectorMotors(retract_s, retract_s, (int)injector.homing_torque_percent_param,
									injector.homing_actual_rapid_sps, injector.homing_actual_accel_sps2);
								}
							}
							break;
						}
					}

					if (!checkInjectorMoving()) {
						switch (injector.currentHomingPhase) {
							case HOMING_PHASE_RAPID_MOVE: {
								sendToPC(is_machine_homing ? "Machine Homing Err: No torque in RAPID."
								: "Cartridge Homing Err: No torque in RAPID.");
								injector.errorState = ERROR_HOMING_NO_TORQUE_RAPID;
								injector.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;
							case HOMING_PHASE_BACK_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing: Back-off done. Touching off."
								: "Cartridge Homing: Back-off done. Touching off.");
								injector.currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
								long touch_off_move_length = Injector::HOMING_DEFAULT_BACK_OFF_STEPS * 2;
								if (Injector::HOMING_DEFAULT_BACK_OFF_STEPS == 0) {
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
								moveInjectorMotors(final_touch_off_move_steps, final_touch_off_move_steps,
								(int)injector.homing_torque_percent_param,
								injector.homing_actual_touch_sps, injector.homing_actual_accel_sps2);
							} break;
							case HOMING_PHASE_TOUCH_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing Err: No torque in TOUCH_OFF."
								: "Cartridge Homing Err: No torque in TOUCH_OFF.");
								injector.errorState = ERROR_HOMING_NO_TORQUE_TOUCH;
								injector.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;
							case HOMING_PHASE_RETRACT: {
								sendToPC(is_machine_homing ? "Machine Homing: RETRACT complete. Success."
								: "Cartridge Homing: RETRACT complete. Success.");
								if (is_machine_homing) {
									injector.onHomingMachineDone();
									} else {
									injector.onHomingCartridgeDone();
								}
								injector.errorState = ERROR_NONE;
								injector.currentHomingPhase = HOMING_PHASE_COMPLETE;
							} break;
							default:
							sendToPC("Homing Wrn: Unexpected phase with stopped motors.");
							injector.errorState = ERROR_INVALID_INJECTION; // Changed from MANUAL_ABORT
							injector.currentHomingPhase = HOMING_PHASE_ERROR;
							break;
						}
					}
				}
			}
			break;
			
			case FEED_MODE: {
				// Torque check during active feed operations
				if (injector.feedState == FEED_INJECT_ACTIVE || injector.feedState == FEED_PURGE_ACTIVE ||
				injector.feedState == FEED_MOVING_TO_HOME || injector.feedState == FEED_MOVING_TO_RETRACT ||
				injector.feedState == FEED_INJECT_RESUMING || injector.feedState == FEED_PURGE_RESUMING ) {
					if (checkInjectorTorqueLimit()) {
						// abortInjectorMove() is called by checkInjectorTorqueLimit
						injector.errorState = ERROR_TORQUE_ABORT;
						sendToPC("FEED_MODE: Torque limit! Operation stopped.");
						finalizeAndResetActiveDispenseOperation(&injector, false); // false = not successful completion
						injector.feedState = FEED_STANDBY;
						injector.feedingDone = true;
					}
				}

				// Handling completion of non-blocking feed operations or state transitions
				if (!checkInjectorMoving()) { // If motors are confirmed stopped
					if (!injector.feedingDone) { // And the current operation segment isn't marked as done yet
						
						// Finalize dispense calculation for INJECT/PURGE if they just stopped
						if (injector.feedState == FEED_INJECT_ACTIVE || injector.feedState == FEED_PURGE_ACTIVE) {
							if (injector.active_dispense_INJECTION_ongoing) {
								if (injector.active_op_steps_per_ml > 0.0001f) {
									long steps_moved_this_segment = ConnectorM0.PositionRefCommanded() - injector.active_op_segment_initial_axis_steps;
									float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / injector.active_op_steps_per_ml;
									injector.active_op_total_dispensed_ml += segment_dispensed_ml;
								}
								// Check if total target steps/volume met (active_op_remaining_steps should be 0 or close)
								injector.last_completed_dispense_ml = injector.active_op_total_dispensed_ml;
								// fullyResetActiveDispenseOperation will be called below by FEED_INJECTION_COMPLETED
							}
							sendToPC("Feed Op: Inject/Purge segment/operation completed.");
							injector.feedState = FEED_INJECTION_COMPLETED; // Transition to completed
							// feedingDone will be set below
						}
						else if (injector.feedState == FEED_MOVING_TO_HOME || injector.feedState == FEED_MOVING_TO_RETRACT) {
							sendToPC("Feed Op: Positioning move completed.");
							injector.feedState = FEED_INJECTION_COMPLETED; // Transition to completed
							// feedingDone will be set below
						}
						// For PAUSED injector, they only transition out via RESUME or CANCEL commands, not !checkInjectorMoving() alone
						
						// If any operation that sets feedingDone=false has completed:
						if (injector.feedState == FEED_INJECTION_COMPLETED) {
							injector.feedingDone = true; // Mark the overarching operation as done
							injector.onFeedingDone();    // Call the handler if it does anything
							finalizeAndResetActiveDispenseOperation(&injector, true); // true = successful completion
							injector.feedState = FEED_STANDBY; // Finally, return to standby
							sendToPC("Feed Op: Finalized. Returning to Feed Standby.");
						}
					}
					} else { // Motors are moving
					// Transition from STARTING/RESUMING to ACTIVE once motion has begun
					if (injector.feedState == FEED_INJECT_STARTING) {
						injector.feedState = FEED_INJECT_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Inject Active.");
						} else if (injector.feedState == FEED_PURGE_STARTING) {
						injector.feedState = FEED_PURGE_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Purge Active.");
						} else if (injector.feedState == FEED_INJECT_RESUMING) {
						injector.feedState = FEED_INJECT_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded(); // Re-baseline for resumed segment
						sendToPC("Feed Op: Inject Resumed, Active.");
						} else if (injector.feedState == FEED_PURGE_RESUMING) {
						injector.feedState = FEED_PURGE_ACTIVE;
						injector.active_op_segment_initial_axis_steps = ConnectorM0.PositionRefCommanded();
						sendToPC("Feed Op: Purge Resumed, Active.");
					}
				}

				// Handle transitions from injector set by command handlers (like PAUSED or CANCELLED)
				if ((injector.feedState == FEED_INJECT_PAUSED || injector.feedState == FEED_PURGE_PAUSED) &&
				!injector.feedingDone && !checkInjectorMoving()) {

					long current_pos = ConnectorM0.PositionRefCommanded();
					long steps_moved_this_segment = current_pos - injector.active_op_segment_initial_axis_steps;

					// Ensure steps_per_ml is valid
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
						sendToPC(debugMsg);
					}

					injector.feedingDone = true;  // Mark segment calculation complete
					sendToPC("Feed Op: Operation Paused. Waiting for Resume/Cancel.");
				}
			} break;

			case JOG_MODE:
			if (checkInjectorTorqueLimit()) {
				injector.mainState = STANDBY_MODE;
				injector.errorState = ERROR_TORQUE_ABORT;
				sendToPC("JOG_MODE: Torque limit, returning to STANDBY.");
				} else if (!checkInjectorMoving() && !injector.jogDone) { // Assuming jogDone is set to false on jog start
				// injector.jogDone = true; // Optional: mark jog as complete if needed
				// injector.onJogDone();
			}
			break;

			case DISABLED_MODE:
			// Only ENABLE command should change state from here
			break;
		} // end switch
	} // end while
} // end main