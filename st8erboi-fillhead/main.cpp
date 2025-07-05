#include "fillhead.h"

// Global instance of our Fillhead controller class
Fillhead fillhead;

/*
    The main state machine. This is called continuously from the main loop
    to handle ongoing operations like monitoring moves, checking torque,
    and stepping through the homing sequence.
*/
void Fillhead::updateState() {
    switch (state) {
        case STATE_STANDBY:
            // Do nothing while in standby
            break;

        case STATE_STARTING_MOVE: {
            if (isMoving()) {
                state = STATE_MOVING;
            }
            break;
        }

        case STATE_MOVING: {
            // This is the known-good logic that works for jogging.
            bool torque_exceeded = false;
            if (activeMotor1 && checkTorqueLimit(activeMotor1)) {
                torque_exceeded = true;
            }
            // Ensure both motors are checked every time, no short-circuiting.
            if (activeMotor2 && checkTorqueLimit(activeMotor2)) {
                torque_exceeded = true;
            }

            if (torque_exceeded) {
                abortAllMoves();
                sendStatus(STATUS_PREFIX_ERROR, "MOVE aborted due to torque limit");
                state = STATE_STANDBY;
                activeMotor1 = nullptr;
                activeMotor2 = nullptr;
            } else if (!isMoving()) {
                sendStatus(STATUS_PREFIX_DONE, "MOVE");
                state = STATE_STANDBY;
                activeMotor1 = nullptr;
                activeMotor2 = nullptr;
            }
            break;
        }

        case STATE_HOMING: {
            switch (homingPhase) {
                case HOMING_START:
                    torqueLimit = (float)homingTorque;
                    homingBackoffSteps = 400;
                    if (activeMotor1) moveAxis(activeMotor1, -homingDistanceSteps, homingRapidSps, 10000, homingTorque);
                    if (activeMotor2) moveAxis(activeMotor2, -homingDistanceSteps, homingRapidSps, 10000, homingTorque);

                    // Transition to the specific wait state for the rapid move
                    homingPhase = HOMING_WAIT_FOR_RAPID_START;
                    break;

                case HOMING_WAIT_FOR_RAPID_START:
                    if (isMoving()) {
                        // The rapid move is confirmed to be active. Proceed.
                        homingPhase = HOMING_RAPID;
                    }
                    break;

                case HOMING_RAPID: { // Scoped for clarity
                    bool torque_exceeded = false;
                    if (activeMotor1 && checkTorqueLimit(activeMotor1)) torque_exceeded = true;
                    if (activeMotor2 && checkTorqueLimit(activeMotor2)) torque_exceeded = true;

                    if (torque_exceeded) {
                        abortAllMoves();
                        Delay_ms(50);
                        // --- MODIFIED ---
                        // Start the back-off move at rapid speed
                        moveAxis(activeMotor1, homingBackoffSteps, homingRapidSps, 200000, 100);
                        if (activeMotor2) moveAxis(activeMotor2, homingBackoffSteps, homingRapidSps, 200000, 100);
                        homingPhase = HOMING_BACK_OFF;
                    } else if (!isMoving()) {
                        sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Rapid move completed without torque");
                        state = STATE_STANDBY;
                        homingPhase = HOMING_IDLE;
                    }
                    break;
                }

                case HOMING_BACK_OFF:
                    // This state now ONLY waits for the back-off move to complete.
                    if (!isMoving()) {
                        // The back-off is done. Now, start the final touch-off move.
                        moveAxis(activeMotor1, -homingBackoffSteps * 2, homingTouchSps, 200000, homingTorque);
                        if (activeMotor2) moveAxis(activeMotor2, -homingBackoffSteps * 2, homingTouchSps, 200000, homingTorque);

                        // Transition to the new wait state for the touch-off move.
                        homingPhase = HOMING_WAIT_FOR_TOUCH_START;
                    }
                    break;

                case HOMING_WAIT_FOR_TOUCH_START:
                    if (isMoving()) {
                        // The touch-off move is confirmed to be active. Proceed to monitor it.
                        homingPhase = HOMING_TOUCH;
                    }
                    break;

                case HOMING_TOUCH: { // Scoped for clarity
                    bool torque_exceeded = false;
                    if (activeMotor1 && checkTorqueLimit(activeMotor1)) torque_exceeded = true;
                    if (activeMotor2 && checkTorqueLimit(activeMotor2)) torque_exceeded = true;

                    if (torque_exceeded) {
                        // This is the successful homing condition
                        abortAllMoves();
                        homingPhase = HOMING_COMPLETE;
                    } else if (!isMoving()) {
                        // This check is now reliable for the touch-off move.
                        sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Touch-off move completed without torque");
                        state = STATE_STANDBY;
                        homingPhase = HOMING_IDLE;
                    }
                    break;
                }

                case HOMING_COMPLETE:
                    if (!isMoving()) {
                        if (activeMotor1 == &MotorX) homedX = true;
                        if (activeMotor1 == &MotorY1) homedY1 = true;
                        if (activeMotor2 == &MotorY2) homedY2 = true;
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

                default:
                    state = STATE_STANDBY;
                    homingPhase = HOMING_IDLE;
                    break;
            }
            break;
        }
    }
}

void Fillhead::loop() {
    processUdp();
    updateState();
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