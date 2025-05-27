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

int main(void)
{
	SystemStates states;

	// --- Hardware and protocol initialization ---
	//SetupUsbSerial();
	SetupEthernet();
	SetupMotors();
	
	uint32_t now = Milliseconds();
	uint32_t lastMotorTime = now; // For TEST_MODE
	uint32_t motorInterval = 2000; // For TEST_MODE
	int motorFlip = 1; // For TEST_MODE

	// --- Main application loop ---
	while (true)
	{
		checkUdpBuffer(&states);
		sendTelem(&states);
		now = Milliseconds();

		switch (states.mainState)
		{
			// ... (other cases like STANDBY_MODE, TEST_MODE) ...
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
				moveMotors(motorFlip * 800, motorFlip * 800, 20, 800, 5000); // Example: 20% torque
				motorFlip *= -1;
				lastMotorTime = now;
			}
			break;

			case HOMING_MODE: { // Scope for local variables in HOMING_MODE case
				if (states.homingState == HOMING_NONE || states.currentHomingPhase == HOMING_PHASE_IDLE || states.currentHomingPhase == HOMING_PHASE_COMPLETE || states.currentHomingPhase == HOMING_PHASE_ERROR) {
					if (states.currentHomingPhase != HOMING_PHASE_COMPLETE && states.currentHomingPhase != HOMING_PHASE_ERROR) {
						states.mainState = STANDBY_MODE;
						states.homingState = HOMING_NONE;
						states.currentHomingPhase = HOMING_PHASE_IDLE;
					}
					break;
				}

				if (now - states.homingStartTime > MAX_HOMING_DURATION_MS) {
					sendToPC("Homing Error: Timeout. Aborting homing.");
					abortMove();
					states.errorState = ERROR_HOMING_TIMEOUT;
					states.mainState = STANDBY_MODE;
					states.homingState = HOMING_NONE;
					states.currentHomingPhase = HOMING_PHASE_ERROR;
					break;
				}

				bool is_machine_homing = (states.homingState == HOMING_MACHINE);
				int direction = is_machine_homing ? -1 : 1;

				if (states.currentHomingPhase == HOMING_PHASE_RAPID_MOVE || states.currentHomingPhase == HOMING_PHASE_TOUCH_OFF) {
					if (checkTorqueLimit()) {
						if (states.currentHomingPhase == HOMING_PHASE_RAPID_MOVE) {
							// Create a new scope for variable declaration
							{ // <<< FIX: Added opening brace
								sendToPC(is_machine_homing ? "Machine Homing: Torque detected (RAPID_MOVE). Transitioning to BACK_OFF."
								: "Cartridge Homing: Torque detected (RAPID_MOVE). Transitioning to BACK_OFF.");
								states.currentHomingPhase = HOMING_PHASE_BACK_OFF;
								
								long back_off_s = -direction * SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS; // Line ~151 was here
								moveMotors(back_off_s, back_off_s,
								(int)states.homing_torque_percent_param,
								states.homing_actual_touch_sps,
								states.homing_actual_accel_sps2);
							} // <<< FIX: Added closing brace
							} else { // Torque hit during HOMING_PHASE_TOUCH_OFF
							// Create a new scope for variable declaration
							{ // <<< FIX: Added opening brace
								sendToPC(is_machine_homing ? "Machine Homing: Torque detected (TOUCH_OFF). Zeroing and transitioning to RETRACT."
								: "Cartridge Homing: Torque detected (TOUCH_OFF). Zeroing and transitioning to RETRACT.");
								if (is_machine_homing) {
									machineStepCounter = 0;
									sendToPC("Machine step counter zeroed.");
									} else {
									cartridgeStepCounter = 0;
									sendToPC("Cartridge step counter zeroed.");
								}
								
								states.currentHomingPhase = HOMING_PHASE_RETRACT;
								long retract_s = -direction * states.homing_actual_retract_steps; // Line ~160 was here
								moveMotors(retract_s, retract_s,
								(int)states.homing_torque_percent_param,
								states.homing_actual_rapid_sps,
								states.homing_actual_accel_sps2);
							} // <<< FIX: Added closing brace
						}
						break;
					}
				}

				if (!checkMoving()) {
					switch (states.currentHomingPhase) {
						case HOMING_PHASE_RAPID_MOVE:
						sendToPC(is_machine_homing ? "Machine Homing Error: RAPID_MOVE completed full stroke without torque."
						: "Cartridge Homing Error: RAPID_MOVE completed full stroke without torque.");
						states.errorState = ERROR_HOMING_NO_TORQUE_RAPID;
						states.mainState = STANDBY_MODE;
						states.currentHomingPhase = HOMING_PHASE_ERROR;
						break;

						case HOMING_PHASE_BACK_OFF:
						// Create a new scope for variable declaration
						{ // <<< FIX: Added opening brace
							sendToPC(is_machine_homing ? "Machine Homing: BACK_OFF complete. Starting TOUCH_OFF."
							: "Cartridge Homing: BACK_OFF complete. Starting TOUCH_OFF.");
							states.currentHomingPhase = HOMING_PHASE_TOUCH_OFF;
							
							long touch_off_move_dist = direction * (SystemStates::HOMING_DEFAULT_BACK_OFF_STEPS + SystemStates::HOMING_TOUCH_OFF_EXTRA_STEPS); // Line ~180 was here
							moveMotors(touch_off_move_dist, touch_off_move_dist,
							(int)states.homing_torque_percent_param,
							states.homing_actual_touch_sps,
							states.homing_actual_accel_sps2);
						} // <<< FIX: Added closing brace
						break;

						case HOMING_PHASE_TOUCH_OFF:
						sendToPC(is_machine_homing ? "Machine Homing Error: TOUCH_OFF completed without torque."
						: "Cartridge Homing Error: TOUCH_OFF completed without torque.");
						states.errorState = ERROR_HOMING_NO_TORQUE_TOUCH;
						states.mainState = STANDBY_MODE;
						states.currentHomingPhase = HOMING_PHASE_ERROR;
						break;

						case HOMING_PHASE_RETRACT:
						sendToPC(is_machine_homing ? "Machine Homing: RETRACT complete. Homing successful."
						: "Cartridge Homing: RETRACT complete. Homing successful.");
						if (is_machine_homing) {
							states.onHomingMachineDone();
							} else {
							states.onHomingCartridgeDone();
						}
						states.mainState = STANDBY_MODE;
						states.currentHomingPhase = HOMING_PHASE_COMPLETE;
						states.errorState = ERROR_NONE;
						states.homingState = HOMING_NONE;
						break;
						
						default:
						if (states.currentHomingPhase != HOMING_PHASE_COMPLETE && states.currentHomingPhase != HOMING_PHASE_ERROR) {
							sendToPC("Homing Warning: Unexpected phase with stopped motors. Resetting homing.");
							states.mainState = STANDBY_MODE;
							states.homingState = HOMING_NONE;
							states.currentHomingPhase = HOMING_PHASE_IDLE;
						}
						break;
					}
				}
			}
			break;

			// ... (other cases like FEED_MODE, JOG_MODE, DISABLED_MODE) ...
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
				// states.jogDone = true; // This flag is typically managed by the command handler or if a jog completion event is needed
			}
			break;

			case DISABLED_MODE:
			break;
		}
	}
}