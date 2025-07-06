#include "axis.h"
#include "fillhead.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

Axis::Axis(Fillhead* controller, const char* name, MotorDriver* motor1, MotorDriver* motor2, float stepsPerMm) {
    m_controller = controller;
    m_name = name;
    m_motor1 = motor1;
    m_motor2 = motor2;
    m_stepsPerMm = stepsPerMm;
    resetHomingState();
}

void Axis::enable() {
    m_motor1->EnableRequest(true);
    if (m_motor2) m_motor2->EnableRequest(true);
}

void Axis::disable() {
    m_motor1->EnableRequest(false);
    if (m_motor2) m_motor2->EnableRequest(false);
}

void Axis::setupMotors() {
    m_motor1->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
    m_motor1->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
    m_motor1->VelMax(MAX_VEL);
    m_motor1->AccelMax(MAX_ACC);
    m_motor1->EnableRequest(true);
    if (m_motor2) {
        m_motor2->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
        m_motor2->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
        m_motor2->VelMax(MAX_VEL);
        m_motor2->AccelMax(MAX_ACC);
        m_motor2->EnableRequest(true);
    }
}

void Axis::moveSteps(long steps, int velSps, int accelSps2, int torque) {
    m_firstTorqueReadM1 = true;
    m_firstTorqueReadM2 = true;
    m_torqueLimit = (float)torque;
    if (velSps > MAX_VEL) velSps = MAX_VEL;
    if (accelSps2 > MAX_ACC) accelSps2 = MAX_ACC;
    m_motor1->VelMax(velSps);
    m_motor1->AccelMax(accelSps2);
    m_motor1->Move(steps);
    if (m_motor2) {
        m_motor2->VelMax(velSps);
        m_motor2->AccelMax(accelSps2);
        m_motor2->Move(steps);
    }
}

void Axis::handleMove(const char* args) {
    if (isMoving()) {
        sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Axis is already moving.");
        return;
    }
    
    float distance_mm, vel_mms, accel_mms2;
    int torque;
    if (!args || sscanf(args, "%f %f %f %d", &distance_mm, &vel_mms, &accel_mms2, &torque) != 4) {
        sendStatus(STATUS_PREFIX_ERROR, "Invalid MOVE format.");
        return;
    }
    long steps = (long)(distance_mm * m_stepsPerMm);
    int velocity_sps = (int)fabs(vel_mms * m_stepsPerMm);
    int accel_sps2 = (int)fabs(accel_mms2 * m_stepsPerMm);
    
    sendStatus(STATUS_PREFIX_INFO, "MOVE command received");
    moveSteps(steps, velocity_sps, accel_sps2, torque);
    m_state = STATE_STARTING_MOVE;
    m_delay_target_ms = Milliseconds() + 5; // Set 5ms timer to prevent race condition
}

void Axis::handleHome(const char* args) {
    if (isMoving()) {
        sendStatus(STATUS_PREFIX_ERROR, "Cannot start home: Axis is already moving.");
        return;
    }
    float max_dist_mm;
    if (!args || sscanf(args, "%d %f", &m_homingTorque, &max_dist_mm) != 2) {
        sendStatus(STATUS_PREFIX_ERROR, "Invalid HOME format.");
        return;
    }
    sendStatus(STATUS_PREFIX_INFO, "HOME command received");

    m_homingType = m_motor2 ? HOMING_TYPE_DUAL : HOMING_TYPE_SINGLE;
    m_homed = false;
    m_state = STATE_HOMING;
    m_singleHomingPhase = S_START;
    m_dualHomingPhase = D_START;
    
    float rapid_vel_mms = 15.0; 
    float touch_vel_mms = 5.0;
    float backoff_mm = 5.0;
    float final_retract_mm = 5.0;
    
    m_homingDistanceSteps = (long)(max_dist_mm * m_stepsPerMm);
    m_homingBackoffSteps = (long)(backoff_mm * m_stepsPerMm);
    m_finalRetractSteps = (long)(final_retract_mm * m_stepsPerMm);
    m_homingRapidSps = (int)fabs(rapid_vel_mms * m_stepsPerMm);
    m_homingTouchSps = (int)fabs(touch_vel_mms * m_stepsPerMm);
}

void Axis::updateState() {
    switch (m_state) {
        case STATE_STANDBY:
            break;
			
		case STATE_STARTING_MOVE: {
            if (isMoving()){
				m_state = STATE_MOVING;
				sendStatus(STATUS_PREFIX_INFO, "MOVE STARTED");
			}
			if (m_motor1->StatusReg().bit.MotorInFault) {
                sendStatus(STATUS_PREFIX_ERROR, "Homing aborted due to motor fault.");
                abort();
                return;
            }
            break;
        }
		
        case STATE_MOVING: {
            // This is the known-good logic that works for jogging.
            bool torque_exceeded = false;
            if (m_motor1 && checkTorqueLimit(m_motor1)) {
                torque_exceeded = true;
            }
            // Ensure both motors are checked every time, no short-circuiting.
            if (m_motor2 && checkTorqueLimit(m_motor2)) {
                torque_exceeded = true;
            }

            if (torque_exceeded) {
                abort();
                sendStatus(STATUS_PREFIX_ERROR, "MOVE aborted due to torque limit");
                m_state = STATE_STANDBY;
                m_motor1 = nullptr;
				m_motor2 = nullptr;
            } else if (!isMoving()) {
                sendStatus(STATUS_PREFIX_DONE, "MOVE DONE");
                m_state = STATE_STANDBY;
                m_motor1 = nullptr;
                m_motor2 = nullptr;
            }
            break;
        }
		
        case STATE_HOMING: {
            if (m_motor1->StatusReg().bit.MotorInFault) {
                sendStatus(STATUS_PREFIX_ERROR, "Homing aborted due to motor fault.");
                abort();
                return;
            }
            // RESTORED: Full state machine for homing
            switch (m_homingType) {
                case HOMING_TYPE_SINGLE:
                    switch (m_singleHomingPhase) {
                        case S_START:
                            moveSteps(-m_homingDistanceSteps, m_homingRapidSps, 200000, m_homingTorque);
                            m_singleHomingPhase = S_RAPID_TO_ENDSTOP;
                            break;
                        case S_RAPID_TO_ENDSTOP:
                            if (fabsf(m_motor1->HlfbPercent()) > m_torqueLimit) {
                                m_motor1->MoveStopAbrupt();
                                m_singleHomingPhase = S_DESTRESS_DISABLE;
                            } else if (!isMoving()) {
                                sendStatus(STATUS_PREFIX_ERROR, "Homing(S): Did not hit endstop during rapid move.");
                                abort();
                            }
                            break;
                        case S_DESTRESS_DISABLE:
                            disable();
                            m_delay_target_ms = Milliseconds() + 250;
                            m_singleHomingPhase = S_AWAIT_DESTRESS_TIMER;
                            break;
                        case S_AWAIT_DESTRESS_TIMER:
                            if(Milliseconds() >= m_delay_target_ms) {
                                m_singleHomingPhase = S_DESTRESS_ENABLE;
                            }
                            break;
                        case S_DESTRESS_ENABLE:
                            enable();
                            m_delay_target_ms = Milliseconds() + 250;
                            m_singleHomingPhase = S_AWAIT_ENABLE_TIMER;
                            break;
                        case S_AWAIT_ENABLE_TIMER:
                            if(Milliseconds() >= m_delay_target_ms) {
                                m_singleHomingPhase = S_FIRST_BACKOFF;
                            }
                            break;
                        case S_FIRST_BACKOFF:
                             moveSteps(m_homingBackoffSteps, m_homingRapidSps, 200000, 100);
                             m_singleHomingPhase = S_AWAIT_BACKOFF_MOVE;
                            break;
                        case S_AWAIT_BACKOFF_MOVE:
                            if (!isMoving()) {
                                moveSteps(-m_homingBackoffSteps * 2, m_homingTouchSps, 200000, m_homingTorque);
                                m_singleHomingPhase = S_AWAIT_TOUCH_MOVE;
                            }
                            break;
                        case S_AWAIT_TOUCH_MOVE:
                            if (fabsf(m_motor1->HlfbPercent()) > m_torqueLimit) {
                                m_motor1->MoveStopAbrupt();
                                m_singleHomingPhase = S_FINAL_DESTRESS;
                            } else if (!isMoving()) {
                                sendStatus(STATUS_PREFIX_ERROR, "Homing(S): Did not hit endstop during touch move.");
                                abort();
                            }
                            break;
                        case S_FINAL_DESTRESS:
                            disable();
                            m_delay_target_ms = Milliseconds() + 250;
                            m_singleHomingPhase = S_AWAIT_FINAL_DESTRESS_TIMER;
                            break;
                        case S_AWAIT_FINAL_DESTRESS_TIMER:
                            if(Milliseconds() >= m_delay_target_ms) {
                                m_singleHomingPhase = S_SET_ZERO;
                            }
                            break;
                        case S_SET_ZERO:
                            m_motor1->PositionRefSet(0);
                            m_singleHomingPhase = S_FINAL_ENABLE;
                            break;
                        case S_FINAL_ENABLE:
                            enable();
                            m_delay_target_ms = Milliseconds() + 250;
                            m_singleHomingPhase = S_AWAIT_FINAL_ENABLE_TIMER;
                            break;
                        case S_AWAIT_FINAL_ENABLE_TIMER:
                            if(Milliseconds() >= m_delay_target_ms) {
                                m_singleHomingPhase = S_FINAL_RETRACT;
                            }
                            break;
                        case S_FINAL_RETRACT:
                            moveSteps(m_finalRetractSteps, m_homingRapidSps, 200000, 100);
                            m_singleHomingPhase = S_AWAIT_FINAL_RETRACT_MOVE;
                            break;
                        case S_AWAIT_FINAL_RETRACT_MOVE:
                            if (!isMoving()) { m_singleHomingPhase = S_COMPLETE; }
                            break;
                        case S_COMPLETE:
                            m_homed = true;
                            sendStatus(STATUS_PREFIX_DONE, "HOME");
                            resetHomingState();
                            break;
                        default:
                            abort();
                            break;
                    }
                    break;
                case HOMING_TYPE_DUAL:
                    break;
                case HOMING_TYPE_NONE:
                default:
                    break;
            }
            break; 
        }
    }
}

void Axis::abort() {
    m_motor1->MoveStopAbrupt();
    if (m_motor2) m_motor2->MoveStopAbrupt();
}

void Axis::resetHomingState() {
    m_state = STATE_STANDBY;
    m_homingType = HOMING_TYPE_NONE;
    m_singleHomingPhase = S_IDLE;
    m_dualHomingPhase = D_IDLE;
}

bool Axis::isMoving() {
    bool motor1Moving = m_motor1->StatusReg().bit.StepsActive || !m_motor1->StatusReg().bit.AtTargetPosition;
    bool motor2Moving = m_motor2 ? (m_motor2->StatusReg().bit.StepsActive || !m_motor2->StatusReg().bit.AtTargetPosition) : false;
    return motor1Moving || motor2Moving;
}

const char* Axis::getStateString() {
    if (m_state == STATE_HOMING) return "Homing";
    if (m_state == STATE_MOVING) return "Moving";
    return "Standby";
}

void Axis::sendStatus(const char* statusType, const char* message) {
    char fullMsg[128];
    snprintf(fullMsg, sizeof(fullMsg), "Axis %s: %s", m_name, message);
    m_controller->sendStatus(statusType, fullMsg);
}

bool Axis::isEnabled() {
    if (m_motor2) {
        return m_motor1->StatusReg().bit.Enabled && m_motor2->StatusReg().bit.Enabled;
    }
    return m_motor1->StatusReg().bit.Enabled;
}

float Axis::getRawTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead) {
    float currentRawTorque = motor->HlfbPercent();
    if (currentRawTorque < -100) { return 0; }
    if (*firstRead) {
        *smoothedValue = currentRawTorque;
        *firstRead = false;
    } else {
        *smoothedValue = (EWMA_ALPHA * currentRawTorque) + (1.0f - EWMA_ALPHA) * (*smoothedValue);
    }
    return *smoothedValue;
}

float Axis::getSmoothedTorque() {
    return getRawTorque(m_motor1, &m_smoothedTorqueM1, &m_firstTorqueReadM1);
}

bool Axis::checkTorqueLimit(MotorDriver* motor) {
    if (!motor || !motor->StatusReg().bit.Enabled) {
        return false;
    }
    
    if (!isMoving()) {
        return false;
    }

    float torque;
    if (motor == m_motor1) {
        torque = getRawTorque(motor, &m_smoothedTorqueM1, &m_firstTorqueReadM1);
    } else if (motor == m_motor2) {
        torque = getRawTorque(motor, &m_smoothedTorqueM2, &m_firstTorqueReadM2);
    } else {
        return false;
    }

    if (fabsf(torque) > m_torqueLimit) {
        return true;
    }
    return false;
}