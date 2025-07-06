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
    m_state = STATE_STANDBY;
    m_homingPhase = HOMING_IDLE;
    m_homed = false;
    m_torqueLimit = 20.0f;
    m_firstTorqueReadM1 = true;
    m_firstTorqueReadM2 = true;
    m_smoothedTorqueM1 = 0.0f;
    m_smoothedTorqueM2 = 0.0f;
    m_motor1_hit_rapid = false;
    m_motor2_hit_rapid = false;
}

void Axis::enable() {
    m_motor1->EnableRequest(true);
    if (m_motor2) m_motor2->EnableRequest(true);
    char msg[50];
    snprintf(msg, sizeof(msg), "Axis %s Enabled.", m_name);
    m_controller->sendStatus(STATUS_PREFIX_INFO, msg);
}

void Axis::disable() {
    m_motor1->EnableRequest(false);
    if (m_motor2) m_motor2->EnableRequest(false);
    char msg[50];
    snprintf(msg, sizeof(msg), "Axis %s Disabled.", m_name);
    m_controller->sendStatus(STATUS_PREFIX_INFO, msg);
}

bool Axis::isEnabled() {
    if (m_motor2) {
        return m_motor1->StatusReg().bit.Enabled && m_motor2->StatusReg().bit.Enabled;
    }
    return m_motor1->StatusReg().bit.Enabled;
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
    if (m_state != STATE_STANDBY) {
        sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Not in STANDBY");
        return;
    }
    float distance_mm, vel_mms, accel_mms2;
    int torque;
    if (!args || sscanf(args, "%f %f %f %d", &distance_mm, &vel_mms, &accel_mms2, &torque) != 4) {
        sendStatus(STATUS_PREFIX_ERROR, "Invalid MOVE format. Expected: <dist_mm> <vel_mm_s> <accel_mm_s2> <torque>");
        return;
    }
    long steps = (long)(distance_mm * m_stepsPerMm);
    int velocity_sps = (int)fabs(vel_mms * m_stepsPerMm);
    int accel_sps2 = (int)fabs(accel_mms2 * m_stepsPerMm);
    char info[100];
    snprintf(info, sizeof(info), "MOVE command received (steps: %ld)", steps);
    sendStatus(STATUS_PREFIX_INFO, info);
    moveSteps(steps, velocity_sps, accel_sps2, torque);
    m_state = STATE_STARTING_MOVE;
}

void Axis::handleHome(const char* args) {
    if (m_state != STATE_STANDBY) {
        sendStatus(STATUS_PREFIX_ERROR, "Cannot start home: Not in STANDBY");
        return;
    }
    float max_dist_mm;
    if (!args || sscanf(args, "%d %f", &m_homingTorque, &max_dist_mm) != 2) {
        sendStatus(STATUS_PREFIX_ERROR, "Invalid HOME format. Expected: <torque> <max_dist_mm>");
        return;
    }
    sendStatus(STATUS_PREFIX_INFO, "HOME command received");

    float rapid_vel_mms = 2.5;
    float touch_vel_mms = 0.25;
    float backoff_mm = 5.0;
    float final_retract_mm = 5.0;
    float gantry_align_mm = 5.0;

    m_homingDistanceSteps = (long)(max_dist_mm * m_stepsPerMm);
    m_homingBackoffSteps = (long)(backoff_mm * m_stepsPerMm);
    m_finalRetractSteps = (long)(final_retract_mm * m_stepsPerMm);
    m_gantry_align_max_steps = (long)fabs(gantry_align_mm * m_stepsPerMm);
    m_homingRapidSps = (int)fabs(rapid_vel_mms * m_stepsPerMm);
    m_homingTouchSps = (int)fabs(touch_vel_mms * m_stepsPerMm);

    m_firstTorqueReadM1 = true;
    m_firstTorqueReadM2 = true;
    m_state = STATE_HOMING;
    m_homingPhase = HOMING_START;
    m_homed = false;
}

void Axis::updateState() {
    switch (m_state) {
        case STATE_STANDBY:
            break;
        case STATE_STARTING_MOVE:
            if (isMoving()) {
                m_state = STATE_MOVING;
            }
            break;
        case STATE_MOVING:
            if (checkTorqueLimit(m_motor1) || (m_motor2 && checkTorqueLimit(m_motor2))) {
                abort();
                sendStatus(STATUS_PREFIX_ERROR, "MOVE aborted due to torque limit");
            } else if (!isMoving()) {
                sendStatus(STATUS_PREFIX_DONE, "MOVE");
                m_state = STATE_STANDBY;
            }
            break;
        case STATE_HOMING: {
            switch (m_homingPhase) {
                case HOMING_START:
                    m_torqueLimit = (float)m_homingTorque;
                    m_motor1_hit_rapid = false;
                    m_motor2_hit_rapid = false;
                    moveSteps(-m_homingDistanceSteps, m_homingRapidSps, 200000, m_homingTorque);
                    m_homingPhase = HOMING_AWAIT_RAPID_START;
                    break;
                
                case HOMING_AWAIT_RAPID_START:
                    if (isMoving()) {
                        m_homingPhase = m_motor2 ? HOMING_Y_GANTRY_SQUARING : HOMING_RAPID;
                    }
                    break;

                case HOMING_RAPID: // For X and Z axes
                    if (checkTorqueLimit(m_motor1)) {
                        m_motor1->MoveStopAbrupt();
                        m_homingPhase = HOMING_BACK_OFF;
                    } else if (!isMoving()) {
                        sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Rapid move completed without torque");
                        m_state = STATE_STANDBY;
                        m_homingPhase = HOMING_IDLE;
                    }
                    break;
                case HOMING_Y_GANTRY_SQUARING:
                    if (!m_motor1_hit_rapid && checkTorqueLimit(m_motor1)) {
                        m_motor1->MoveStopAbrupt();
                        m_motor1_hit_rapid = true;
                        sendStatus(STATUS_PREFIX_INFO, "Y-Homing: Motor 1 hit endstop.");
                    }
                    if (!m_motor2_hit_rapid && checkTorqueLimit(m_motor2)) {
                        m_motor2->MoveStopAbrupt();
                        m_motor2_hit_rapid = true;
                        sendStatus(STATUS_PREFIX_INFO, "Y-Homing: Motor 2 hit endstop.");
                    }
                    if (m_motor1_hit_rapid && m_motor2_hit_rapid) {
                        m_homingPhase = HOMING_BACK_OFF;
                    } else if (m_motor1_hit_rapid || m_motor2_hit_rapid) {
                        m_homingPhase = HOMING_Y_ALIGNING;
                    } else if (!isMoving()) {
                        sendStatus(STATUS_PREFIX_ERROR, "Y-Homing Error: Rapid move completed without torque");
                        m_state = STATE_STANDBY;
                        m_homingPhase = HOMING_IDLE;
                    }
                    break;
                case HOMING_Y_ALIGNING:
                    if (!m_motor1_hit_rapid) {
                        if (checkTorqueLimit(m_motor1) || (m_motor1->PositionRefCommanded() < -m_gantry_align_max_steps)) {
                            m_motor1->MoveStopAbrupt();
                            m_motor1_hit_rapid = true;
                        }
                    }
                    if (!m_motor2_hit_rapid) {
                        if (checkTorqueLimit(m_motor2) || (m_motor2->PositionRefCommanded() < -m_gantry_align_max_steps)) {
                            m_motor2->MoveStopAbrupt();
                            m_motor2_hit_rapid = true;
                        }
                    }
                    if (m_motor1_hit_rapid && m_motor2_hit_rapid) {
                        sendStatus(STATUS_PREFIX_INFO, "Y-Homing: Gantry aligned.");
                        m_homingPhase = HOMING_BACK_OFF;
                    }
                    break;
                case HOMING_BACK_OFF:
                    if (!isMoving()) {
                        moveSteps(m_homingBackoffSteps, m_homingRapidSps, 200000, 90);
                        m_homingPhase = HOMING_TOUCH;
                    }
                    break;
                case HOMING_TOUCH:
                    if (isMoving()) {
                        if (checkTorqueLimit(m_motor1) || (m_motor2 && checkTorqueLimit(m_motor2))) {
                            m_motor1->MoveStopAbrupt();
                            if (m_motor2) m_motor2->MoveStopAbrupt();
                            m_homingPhase = HOMING_DE_STRESS;
                        }
                    }
                    break;
                case HOMING_DE_STRESS:
                    if (!isMoving()) {
                        disable();
                        Delay_ms(200);
                        m_homingPhase = HOMING_SET_ZERO;
                    }
                    break;
                case HOMING_SET_ZERO:
                    m_motor1->PositionRefSet(0);
                    if(m_motor2) m_motor2->PositionRefSet(0);
                    enable();
                    Delay_ms(100);
                    m_homingPhase = HOMING_FINAL_RETRACT;
                    break;
                case HOMING_FINAL_RETRACT:
                    moveSteps(m_finalRetractSteps, m_homingTouchSps, 200000, 90);
                    m_homingPhase = HOMING_AWAIT_RETRACT_FINISH;
                    break;
                case HOMING_AWAIT_RETRACT_FINISH:
                    if (!isMoving()) {
                        m_homed = true;
                        sendStatus(STATUS_PREFIX_DONE, "HOME");
                        m_state = STATE_STANDBY;
                        m_homingPhase = HOMING_IDLE;
                    }
                    break;
                default:
                    m_state = STATE_STANDBY;
                    m_homingPhase = HOMING_IDLE;
                    break;
            }
        }
        break;
    }
}

bool Axis::isMoving() {
    bool motor1Moving = m_motor1->StatusReg().bit.StepsActive;
    bool motor2Moving = m_motor2 ? m_motor2->StatusReg().bit.StepsActive : false;
    return motor1Moving || motor2Moving;
}

void Axis::abort() {
    m_motor1->MoveStopAbrupt();
    if (m_motor2) m_motor2->MoveStopAbrupt();
    m_state = STATE_STANDBY;
    m_homingPhase = HOMING_IDLE;
}

const char* Axis::getStateString() {
    if (m_state == STATE_HOMING) return "HOMING";
    if (m_state == STATE_MOVING) return "MOVING";
    if (m_state == STATE_STARTING_MOVE) return "STARTING_MOVE";
    return "STANDBY";
}

void Axis::sendStatus(const char* statusType, const char* message) {
    char fullMsg[128];
    snprintf(fullMsg, sizeof(fullMsg), "Axis %s: %s", m_name, message);
    m_controller->sendStatus(statusType, fullMsg);
}

float Axis::getRawTorque(MotorDriver* motor, float* smoothedValue, bool* firstRead) {
    float currentRawTorque = motor->HlfbPercent();
    if (currentRawTorque < -100) {
        return 0;
    }
    if (*firstRead) {
        *smoothedValue = currentRawTorque;
        *firstRead = false;
    } else {
        *smoothedValue = (EWMA_ALPHA * currentRawTorque) + (1.0f - EWMA_ALPHA) * (*smoothedValue);
    }
    return *smoothedValue;
}

float Axis::getSmoothedTorque() {
    if (!m_motor1->StatusReg().bit.StepsActive) return 0.0f;
    return getRawTorque(m_motor1, &m_smoothedTorqueM1, &m_firstTorqueReadM1);
}

bool Axis::checkTorqueLimit(MotorDriver* motor) {
    if (!motor || !motor->StatusReg().bit.Enabled || !motor->StatusReg().bit.StepsActive) {
        return false;
    }
    float* smoothedValue = nullptr;
    bool* firstRead = nullptr;

    if (motor == m_motor1) {
        smoothedValue = &m_smoothedTorqueM1;
        firstRead = &m_firstTorqueReadM1;
    } else if (motor == m_motor2) {
        smoothedValue = &m_smoothedTorqueM2;
        firstRead = &m_firstTorqueReadM2;
    } else {
        return false;
    }

    float torque = getRawTorque(motor, smoothedValue, firstRead);
    if (fabsf(torque) > m_torqueLimit) {
        return true;
    }
    return false;
}