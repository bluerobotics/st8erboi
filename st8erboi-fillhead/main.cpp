#include "fillhead.h"

// Global instance of our Fillhead controller class
Fillhead fillhead;

/*
    The main state machine. This is called continuously from the main loop
    to handle ongoing operations like monitoring moves, checking torque,
    and stepping through the homing sequence.
*/
void Fillhead::updateState() {
    switch(state) {
        case STATE_STANDBY:
            // Do nothing while in standby
            break;

        // NEW: This case was added to solve the race condition.
        // It waits for the motor to physically start moving before transitioning
        // to the main STATE_MOVING where torque is monitored.
        case STATE_STARTING_MOVE: {
            if (isMoving()) {
                // The motor's "StepsActive" bit is now high, confirming the move has started.
                // We can now safely transition to the monitoring state.
                state = STATE_MOVING;
            }
            // Note: A timeout could be added here to handle cases where a
            // move is commanded but fails to start for some reason.
            break;
        }

        case STATE_MOVING: {
            bool torque_exceeded = false;
            // Check motor 1 for torque limit
            if (activeMotor1) {
                if (checkTorqueLimit(activeMotor1)) {
                    torque_exceeded = true;
                }
            }
            // Check motor 2 for torque limit if motor 1 was okay
            if (!torque_exceeded && activeMotor2) {
                if (checkTorqueLimit(activeMotor2)) {
                    torque_exceeded = true;
                }
            }

            // If torque was exceeded on either motor, abort the move
            if (torque_exceeded) {
                abortAllMoves();
                sendStatus(STATUS_PREFIX_ERROR, "MOVE aborted due to torque limit");
                state = STATE_STANDBY;
                activeMotor1 = nullptr;
                activeMotor2 = nullptr;
            }
            // Otherwise, if the move has completed normally (no longer moving)
            else if (!isMoving()) {
                sendStatus(STATUS_PREFIX_DONE, "MOVE");
                state = STATE_STANDBY;
                activeMotor1 = nullptr;
                activeMotor2 = nullptr;
            }
            break;
        }

        case STATE_HOMING: {
            // This logic remains the same.
            bool torque1 = checkTorqueLimit(activeMotor1);
            bool torque2 = checkTorqueLimit(activeMotor2);

            switch(homingPhase) {
                case HOMING_START:
                    torqueLimit = (float)homingTorque;
                    homingBackoffSteps = 400;
                    if (activeMotor1) moveAxis(activeMotor1, -200000, homingRapidSps, 200000, homingTorque);
                    if (activeMotor2) moveAxis(activeMotor2, -200000, homingRapidSps, 200000, homingTorque);
                    homingPhase = HOMING_RAPID;
                    break;

                case HOMING_RAPID:
                    if (torque1 || torque2) {
                        abortAllMoves();
                        Delay_ms(50);
                        if (activeMotor1) moveAxis(activeMotor1, homingBackoffSteps, homingTouchSps, 200000, 100);
                        if (activeMotor2) moveAxis(activeMotor2, homingBackoffSteps, homingTouchSps, 200000, 100);
                        homingPhase = HOMING_BACK_OFF;
                    } else if (!isMoving()) {
                        sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Move completed without torque");
                        state = STATE_STANDBY;
                        homingPhase = HOMING_IDLE;
                    }
                    break;

                case HOMING_BACK_OFF:
                    if (!isMoving()) {
                        if (activeMotor1) moveAxis(activeMotor1, -homingBackoffSteps * 2, homingTouchSps, 200000, homingTorque);
                        if (activeMotor2) moveAxis(activeMotor2, -homingBackoffSteps * 2, homingTouchSps, 200000, homingTorque);
                        homingPhase = HOMING_TOUCH;
                    }
                    break;

                case HOMING_TOUCH:
                     if (torque1 || torque2) {
                        abortAllMoves();
                        homingPhase = HOMING_COMPLETE;
                    } else if (!isMoving()) {
                        sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Touch-off move completed without torque");
                        state = STATE_STANDBY;
                        homingPhase = HOMING_IDLE;
                    }
                    break;

                case HOMING_COMPLETE:
                    if (!isMoving()) {
                        if (activeMotor1 == &MotorX) homedX = true;
                        if (activeMotor1 == &MotorY1) homedY1 = true;
                        if (activeMotor2 == &MotorY2) homedY2 = true;
                        // MODIFIED: Corrected a typo here ("2homedZ" to "homedZ")
                        if (activeMotor1 == &MotorZ) homedZ = true;

                        if (activeMotor1) activeMotor1->PositionRefSet(0);
                        if (activeMotor2) activeMotor2->PositionRefSet(0);

                        sendStatus(STATUS_PREFIX_DONE, "HOME");
                        state = STATE_STANDBY;
                        homingPhase = HOMING_IDLE;
                        activeMotor1 = nullptr;
                        activeMotor2 = nullptr;
                    }
                    break;

                case HOMING_IDLE:
                case HOMING_ERROR:
                default:
                    state = STATE_STANDBY;
                    break;
            }
            break;
        }
    }
}

void Fillhead::loop() {
	// --- Fast Code ---
	processUdp();
	updateState();

	// --- Slow Code ---
	if (checkSlowCodeInterval()) {
		sendGuiTelemetry();
	}
}

/*
    Main application entry point.
*/

int main(void) {
	// Perform one-time setup
	fillhead.setup();

	// Main non-blocking application loop
	while (true) {
		// Just call the main loop handler for our fillhead object.
		fillhead.loop();
	}
}