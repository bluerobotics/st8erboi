#include "ClearCore.h"
#include "states.h"
#include "motor.h"
#include "messages.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Define a timeout for the entire homing operation
#define MAX_HOMING_DURATION_MS 30000 // 30 seconds, adjust as needed

// These should be defined in motor.cpp and externed in motor.h or states.h
// int32_t machineHomeReferenceSteps = 0; // Example definition, actual should be in motor.cpp
// int32_t cartridgeHomeReferenceSteps = 0; // Example definition, actual should be in motor.cpp

int main(void)
{
	SystemStates states;

	// --- Hardware and protocol initialization ---
	SetupEthernet();
	SetupMotors(); // This should initialize motors and motor-related globals
	
	uint32_t now = Milliseconds();
	uint32_t lastMotorTime = now; // For TEST_MODE
	uint32_t motorInterval = 2000; // For TEST_MODE
	int motorFlip = 1; // For TEST_MODE

	// --- Main application loop ---
	while (true)
	{
		checkUdpBuffer(&states);
		sendTelem(&states); // Sends current states including calculated steps from home
		now = Milliseconds();

		switch (states.mainState)
		{
			case STANDBY_MODE:
			// Idle, do nothing
			break;
			
			case TEST_MODE:
			if (checkTorqueLimit()) { // checkTorqueLimit calls abortMove()
				states.mainState = STANDBY_MODE;
				states.errorState = ERROR_TORQUE_ABORT;
				sendToPC("TEST_MODE: Torque limit reached, stopping test and returning to STANDBY.");
			}
			if (now - lastMotorTime > motorInterval) {
				// Ensure moveMotors updates the actual motor positions that get read for telemetry
				moveMotors(motorFlip * 800, motorFlip * 800, 20, 800, 5000);
				motorFlip *= -1;
				lastMotorTime = now;
			}
			break;

			case HOMING_MODE: { // Scope for local variables

				if (states.currentHomingPhase == HOMING_PHASE_COMPLETE || states.currentHomingPhase == HOMING_PHASE_ERROR) {
					if (states.currentHomingPhase == HOMING_PHASE_COMPLETE) {
						sendToPC("Homing sequence complete. Returning to STANDBY_MODE.");
						} else {
						sendToPC("Homing sequence ended with error. Returning to STANDBY_MODE.");
					}
					states.mainState = STANDBY_MODE;
					states.homingState = HOMING_NONE;
					states.currentHomingPhase = HOMING_PHASE_IDLE;
					break;
				}

				if (states.homingState == HOMING_NONE && states.currentHomingPhase == HOMING_PHASE_IDLE) {
					break;
				}

				if ((states.homingState == HOMING_MACHINE || states.homingState == HOMING_CARTRIDGE) &&
				(states.currentHomingPhase == HOMING_PHASE_RAPID_MOVE ||
				states.currentHomingPhase == HOMING_PHASE_BACK_OFF ||
				states.currentHomingPhase == HOMING_PHASE_TOUCH_OFF ||
				states.currentHomingPhase == HOMING_PHASE_RETRACT)) {

					if (now - states.homingStartTime > MAX_HOMING_DURATION_MS) {
						sendToPC("Homing Error: Timeout during active sequence. Aborting.");
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
									sendToPC(is_machine_homing ? "Machine Homing: Torque detected (RAPID_MOVE). Transitioning to BACK_OFF."
									: "Cartridge Homing: Torque detected (RAPID_MOVE). Transitioning to BACK_OFF.");
									states.currentHomingPhase = HOMING_PHASE_BACK_OFF;
									long back_off_s = -direction * SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS;
									moveMotors(back_off_s, back_off_s,
									(int)states.homing_torque_percent_param,
									states.homing_actual_touch_sps,
									states.homing_actual_accel_sps2);
								}
								} else { // Torque hit during HOMING_PHASE_TOUCH_OFF
								{
									sendToPC(is_machine_homing ? "Machine Homing: Torque detected (TOUCH_OFF). Zeroing reference and transitioning to RETRACT."
									: "Cartridge Homing: Torque detected (TOUCH_OFF). Zeroing reference and transitioning to RETRACT.");
									if (is_machine_homing) {
										// SET THE HOME REFERENCE POINT
										machineHomeReferenceSteps = ConnectorM0.PositionRefCommanded(); // Assuming M0 for machine
										sendToPC("Machine home reference point set.");
										} else { // Cartridge Homing
										// SET THE HOME REFERENCE POINT
										cartridgeHomeReferenceSteps = ConnectorM1.PositionRefCommanded(); // Assuming M1 for cartridge
										sendToPC("Cartridge home reference point set.");
									}
									states.currentHomingPhase = HOMING_PHASE_RETRACT;
									long retract_s = -direction * states.homing_actual_retract_steps;
									moveMotors(retract_s, retract_s,
									(int)states.homing_torque_percent_param,
									states.homing_actual_rapid_sps,
									states.homing_actual_accel_sps2);
								}
							}
							break;
						}
					}

					if (!checkMoving()) {
						switch (states.currentHomingPhase) {
							case HOMING_PHASE_RAPID_MOVE: {
								sendToPC(is_machine_homing ? "Machine Homing Error: RAPID_MOVE completed full stroke without torque."
								: "Cartridge Homing Error: RAPID_MOVE completed full stroke without torque.");
								states.errorState = ERROR_HOMING_NO_TORQUE_RAPID;
								states.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;

							case HOMING_PHASE_BACK_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing: BACK_OFF complete. Starting TOUCH_OFF."
								: "Cartridge Homing: BACK_OFF complete. Starting TOUCH_OFF.");
								states.currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
								long touch_off_move_length = SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS * 2;
								if (SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS == 0) {
									touch_off_move_length = 200;
									sendToPC("Homing Warning: HOMING_DEFAULT_BACK_OFF_STEPS is 0! Using fallback touch-off travel.");
									} else if (touch_off_move_length == 0) {
									touch_off_move_length = 10;
									sendToPC("Homing Warning: Calculated touch_off_move_length is 0, using minimal travel.");
								}
								long final_touch_off_move_steps = direction * touch_off_move_length;
								char log_msg[100];
								snprintf(log_msg, sizeof(log_msg), "Homing: Touch-off approach (steps): %ld", final_touch_off_move_steps);
								sendToPC(log_msg);
								moveMotors(final_touch_off_move_steps, final_touch_off_move_steps,
								(int)states.homing_torque_percent_param,
								states.homing_actual_touch_sps,
								states.homing_actual_accel_sps2);
							} break;

							case HOMING_PHASE_TOUCH_OFF: {
								sendToPC(is_machine_homing ? "Machine Homing Error: TOUCH_OFF completed without torque."
								: "Cartridge Homing Error: TOUCH_OFF completed without torque.");
								states.errorState = ERROR_HOMING_NO_TORQUE_TOUCH;
								states.currentHomingPhase = HOMING_PHASE_ERROR;
							} break;

							case HOMING_PHASE_RETRACT: {
								sendToPC(is_machine_homing ? "Machine Homing: RETRACT complete. Homing successful."
								: "Cartridge Homing: RETRACT complete. Homing successful.");
								if (is_machine_homing) {
									states.onHomingMachineDone();
									} else {
									states.onHomingCartridgeDone();
								}
								states.errorState = ERROR_NONE;
								states.currentHomingPhase = HOMING_PHASE_COMPLETE;
							} break;

							default:
							sendToPC("Homing Warning: Unexpected phase with stopped motors during active sequence. Erroring out.");
							states.errorState = ERROR_MANUAL_ABORT;
							states.currentHomingPhase = HOMING_PHASE_ERROR;
							break;
						}
					}
				}
			}
			break;
			
			case FEED_MODE:
			if (states.feedState == FEED_INJECT || states.feedState == FEED_PURGE || states.feedState == FEED_RETRACT) {
				if (checkTorqueLimit()) {
					states.errorState = ERROR_TORQUE_ABORT;
					states.feedState = FEED_STANDBY;
					sendToPC("FEED_MODE: Torque limit reached during feed operation. Feed stopped.");
					} else if (!checkMoving() && !states.feedingDone) {
					sendToPC("FEED_MODE: Feed operation move completed.");
					states.feedingDone = true;
					states.feedState = FEED_STANDBY;
				}
			}
			break;

			case JOG_MODE:
			if (checkTorqueLimit()) {
				states.mainState = STANDBY_MODE;
				states.errorState = ERROR_TORQUE_ABORT;
				sendToPC("JOG_MODE: Torque limit reached, jog aborted. Returning to STANDBY.");
				} else if (!checkMoving() && !states.jogDone) {
				// jogDone state management could be added if needed
			}
			break;

			case DISABLED_MODE:
			// Only ENABLE command should change state from here
			break;
		} // end switch (states.mainState)
	} // end while(true)
} // end main