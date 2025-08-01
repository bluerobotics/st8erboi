#include "axis.h"
#include "fillhead.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Axis::Axis(Fillhead* controller, const char* name, MotorDriver* motor1, MotorDriver* motor2, float stepsPerMm, float minPosMm, float maxPosMm, Connector* homingSensor1, Connector* homingSensor2, Connector* limitSensor) {
	m_controller = controller;
	m_name = name;
	m_motor1 = motor1;
	m_motor2 = motor2;
	m_stepsPerMm = stepsPerMm;
	m_minPosMm = minPosMm;
	m_maxPosMm = maxPosMm;
	m_homingSensor1 = homingSensor1;
	m_homingSensor2 = homingSensor2;
	m_limitSensor = limitSensor;
	
	m_state = STATE_STANDBY;
	homingPhase = HOMING_NONE;
	m_homed = false;
	
	m_smoothedTorqueM1 = 0.0f;
	m_smoothedTorqueM2 = 0.0f;
	m_firstTorqueReadM1 = true;
	m_firstTorqueReadM2 = true;
	m_torqueLimit = 0.0f;
	m_activeCommand = nullptr;
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
    if (m_homingSensor1) m_homingSensor1->Mode(Connector::INPUT_DIGITAL);
    if (m_homingSensor2) m_homingSensor2->Mode(Connector::INPUT_DIGITAL);
    if (m_limitSensor) m_limitSensor->Mode(Connector::INPUT_DIGITAL);
    
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
	
    long final_steps = steps;
    // CORRECTED: Only the X-axis motor direction needs to be inverted.
    // Z-axis positive steps should move the motor in its positive direction (up).
    if (strcmp(m_name, "X") == 0) {
        final_steps = -steps;
    }

	m_motor1->VelMax(velSps);
	m_motor1->AccelMax(accelSps2);
	m_motor1->Move(final_steps);

	if (m_motor2) { // This will only be the Y axis
		m_motor2->VelMax(velSps);
		m_motor2->AccelMax(accelSps2);
		m_motor2->Move(-steps);
	}
}

float Axis::getPositionMm() const {
    long current_steps = m_motor1->PositionRefCommanded();
    // CORRECTED: Only the X-axis position reading needs to be inverted.
    // For Z, a positive motor step count should correspond to a positive coordinate.
    if (strcmp(m_name, "X") == 0) {
        current_steps = -current_steps;
    }
	return (float)current_steps / m_stepsPerMm;
}

void Axis::startMove(float target_mm, float vel_mms, float accel_mms2, int torque, MoveType moveType) {
	if ((m_motor1 && m_motor1->StatusReg().bit.MotorInFault) || (m_motor2 && m_motor2->StatusReg().bit.MotorInFault)){
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Motor in fault");
		return;
	}
	if (isMoving()) {
		return;
	}

	if (!m_homed) {
		sendStatus(STATUS_PREFIX_ERROR, "Cannot MOVE: Axis must be homed first.");
		return;
	}

	float currentPosMm = getPositionMm();
	float finalTargetPosMm;
	float distanceToMoveMm;

	if (moveType == ABSOLUTE) {
		finalTargetPosMm = target_mm;
		distanceToMoveMm = target_mm - currentPosMm;
	} else { // INCREMENTAL
		finalTargetPosMm = currentPosMm + target_mm;
		distanceToMoveMm = target_mm;
	}


	if (finalTargetPosMm < m_minPosMm || finalTargetPosMm > m_maxPosMm) {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "Move command to %.2fmm exceeds limits [%.2f, %.2f].",
		finalTargetPosMm, m_minPosMm, m_maxPosMm);
		sendStatus(STATUS_PREFIX_ERROR, errorMsg);
		return; 
	} 

	long steps = (long)(distanceToMoveMm * m_stepsPerMm);
	int velocity_sps = (int)fabs(vel_mms * m_stepsPerMm);
	int accel_sps2 = (int)fabs(accel_mms2 * m_stepsPerMm);
	
	moveSteps(steps, velocity_sps, accel_sps2, torque);
	m_state = STATE_STARTING_MOVE;
}


void Axis::handleMove(const char* args) {
	MoveType moveType = ABSOLUTE;
	float target_mm, vel_mms, accel_mms2;
	int torque;
	char modeStr[5] = {0};

	int parsed_count = sscanf(args, "%4s %f %f %f %d", modeStr, &target_mm, &vel_mms, &accel_mms2, &torque);

	if (parsed_count == 5 && (strcmp(modeStr, "ABS") == 0 || strcmp(modeStr, "INC") == 0)) {
		if (strcmp(modeStr, "INC") == 0) {
			moveType = INCREMENTAL;
		}
	}
	else if (sscanf(args, "%f %f %f %d", &target_mm, &vel_mms, &accel_mms2, &torque) == 4) {
		moveType = ABSOLUTE;
	}
	else {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid MOVE format. Use [ABS|INC] <pos> <vel> <accel> <torque>");
		return;
	}

	if (strcmp(m_name, "X") == 0) m_activeCommand = "MOVE_X";
	else if (strcmp(m_name, "Y") == 0) m_activeCommand = "MOVE_Y";
	else if (strcmp(m_name, "Z") == 0) m_activeCommand = "MOVE_Z";
	
	char infoMsg[128];
	snprintf(infoMsg, sizeof(infoMsg), "%s initiated", m_activeCommand);
	sendStatus(STATUS_PREFIX_INFO, infoMsg);
	startMove(target_mm, vel_mms, accel_mms2, torque, moveType);
}

void Axis::handleHome(const char* args) {
	if (!m_homingSensor1) {
		sendStatus(STATUS_PREFIX_ERROR, "Homing not configured for this axis.");
		return;
	}
	if ((m_motor1 && m_motor1->StatusReg().bit.MotorInFault) || (m_motor2 && m_motor2->StatusReg().bit.MotorInFault)){
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start homing: Motor in fault");
		return;
	}
	if (isMoving()) {
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start homing: Axis already moving");
		return;
	}
	
	float max_dist_mm;
	if (args && sscanf(args, "%f", &max_dist_mm) == 1) {
	} else {
		max_dist_mm = m_maxPosMm - m_minPosMm;
	}
	
	if (strcmp(m_name, "X") == 0) m_activeCommand = "HOME_X";
	else if (strcmp(m_name, "Y") == 0) m_activeCommand = "HOME_Y";
	else if (strcmp(m_name, "Z") == 0) m_activeCommand = "HOME_Z";

	char infoMsg[128];
	snprintf(infoMsg, sizeof(infoMsg), "%s initiated with max travel of %.2f mm", m_activeCommand, max_dist_mm);
	sendStatus(STATUS_PREFIX_INFO, infoMsg);

	m_homed = false;
	m_state = STATE_HOMING;
	homingPhase = RAPID_SEARCH_START;
	
	float backoff_mm = 5.0;
	
	m_homingDistanceSteps = (long)(fabs(max_dist_mm) * m_stepsPerMm);
	m_homingBackoffSteps = (long)(backoff_mm * m_stepsPerMm);
	m_homingRapidSps = (int)fabs(HOMING_RAPID_VEL_MMS * m_stepsPerMm);
	m_homingBackoffSps = (int)fabs(HOMING_BACKOFF_VEL_MMS * m_stepsPerMm);
	m_homingTouchSps = (int)fabs(HOMING_TOUCH_VEL_MMS * m_stepsPerMm);
    m_homingAccelSps2 = (int)fabs(HOMING_ACCEL_MMSS * m_stepsPerMm * m_stepsPerMm);
    
    m_motor1_homed = false;
    m_motor2_homed = false;
}

void Axis::updateState() {
	switch (m_state) {
		case STATE_STANDBY:
		break;
		
		case STATE_STARTING_MOVE:
		if ((m_motor1 && m_motor1->StatusReg().bit.MotorInFault) || (m_motor2 && m_motor2->StatusReg().bit.MotorInFault)){
			sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Motor in fault");
			m_state = STATE_STANDBY;
			return;
		}
		if (isMoving()){
			m_state = STATE_MOVING;
		}
		break;
		
		case STATE_MOVING:
		if (m_limitSensor && m_limitSensor->State()) {
			abort();
			sendStatus(STATUS_PREFIX_ERROR, "MOVE aborted due to limit switch trigger.");
			m_state = STATE_STANDBY;
			m_activeCommand = nullptr;
			break;
		}
		if ((m_motor1 && checkTorqueLimit(m_motor1)) || (m_motor2 && checkTorqueLimit(m_motor2))) {
			abort();
			sendStatus(STATUS_PREFIX_ERROR, "MOVE aborted due to torque limit.");
			m_state = STATE_STANDBY;
			m_activeCommand = nullptr;
		}
		else if (!isMoving()) {
			if (m_activeCommand) {
				char doneMsg[64];
				snprintf(doneMsg, sizeof(doneMsg), "%s complete.", m_activeCommand);
				sendStatus(STATUS_PREFIX_DONE, doneMsg);
				m_activeCommand = nullptr;
			}
			m_state = STATE_STANDBY;
		}
		break;
		
		case STATE_HOMING: {
			if ((m_motor1 && m_motor1->StatusReg().bit.MotorInFault) || (m_motor2 && m_motor2->StatusReg().bit.MotorInFault)){
				abort();
				sendStatus(STATUS_PREFIX_ERROR, "Homing failed: Motor in fault.");
				m_state = STATE_STANDBY;
				homingPhase = HOMING_NONE;
				m_activeCommand = nullptr;
				break;
			}

			switch (homingPhase) {
				case RAPID_SEARCH_START: {
					sendStatus(STATUS_PREFIX_INFO, "Homing: Starting rapid search.");
					long rapid_search_steps = -m_homingDistanceSteps;
					if (strcmp(m_name, "Z") == 0) {
						rapid_search_steps = m_homingDistanceSteps;
					}
					moveSteps(rapid_search_steps, m_homingRapidSps, m_homingAccelSps2, HOMING_TORQUE);
					homingPhase = RAPID_SEARCH_WAIT_TO_START;
					break;
				}
				case RAPID_SEARCH_WAIT_TO_START:
					if(isMoving()) {
						homingPhase = RAPID_SEARCH_MOVING;
					}
					break;
				case RAPID_SEARCH_MOVING: {
					bool sensor1_triggered = m_homingSensor1 && m_homingSensor1->State();
					bool sensor2_triggered = m_homingSensor2 && m_homingSensor2->State();

					if (sensor1_triggered && !m_motor1_homed) {
						m_motor1->MoveStopAbrupt();
						m_motor1_homed = true;
						sendStatus(STATUS_PREFIX_INFO, "Homing: Motor 1 sensor hit.");
					}
					if (m_motor2 && sensor2_triggered && !m_motor2_homed) {
						m_motor2->MoveStopAbrupt();
						m_motor2_homed = true;
						sendStatus(STATUS_PREFIX_INFO, "Homing: Motor 2 sensor hit.");
					}

					if (m_motor1_homed && (m_motor2 == nullptr || m_motor2_homed)) {
						sendStatus(STATUS_PREFIX_INFO, "Homing: Rapid search complete.");
						homingPhase = BACKOFF_START;
					} else if (!isMoving()) {
						abort();
						sendStatus(STATUS_PREFIX_ERROR, "Homing failed: Axis stopped before sensor was triggered.");
						m_state = STATE_STANDBY;
						homingPhase = HOMING_NONE;
					}
					break;
				}
				
				case BACKOFF_START: {
					sendStatus(STATUS_PREFIX_INFO, "Homing: Starting backoff.");
					long backoff_steps = m_homingBackoffSteps;
					if (strcmp(m_name, "Z") == 0) {
						backoff_steps = -m_homingBackoffSteps;
					}
					moveSteps(backoff_steps, m_homingBackoffSps, m_homingAccelSps2, HOMING_TORQUE);
					homingPhase = BACKOFF_WAIT_TO_START;
					break;
				}
				case BACKOFF_WAIT_TO_START:
					if(isMoving()) {
						homingPhase = BACKOFF_MOVING;
					}
					break;
				case BACKOFF_MOVING:
					if (!isMoving()) {
						sendStatus(STATUS_PREFIX_INFO, "Homing: Backoff complete.");
						homingPhase = SLOW_SEARCH_START;
					}
					break;
					
				case SLOW_SEARCH_START: {
					sendStatus(STATUS_PREFIX_INFO, "Homing: Starting slow search.");
					m_motor1_homed = false;
					m_motor2_homed = false;
					long slow_search_steps = -m_homingBackoffSteps * 2;
					if (strcmp(m_name, "Z") == 0) {
						slow_search_steps = m_homingBackoffSteps * 2;
					}
					moveSteps(slow_search_steps, m_homingTouchSps, m_homingAccelSps2, HOMING_TORQUE);
					homingPhase = SLOW_SEARCH_WAIT_TO_START;
					break;
				}
				case SLOW_SEARCH_WAIT_TO_START:
					if(isMoving()) {
						homingPhase = SLOW_SEARCH_MOVING;
					}
					break;
				case SLOW_SEARCH_MOVING: {
					bool sensor1_triggered = m_homingSensor1 && m_homingSensor1->State();
					bool sensor2_triggered = m_homingSensor2 && m_homingSensor2->State();

					if (sensor1_triggered && !m_motor1_homed) {
						m_motor1->MoveStopAbrupt();
						m_motor1_homed = true;
					}
					if (m_motor2 && sensor2_triggered && !m_motor2_homed) {
						m_motor2->MoveStopAbrupt();
						m_motor2_homed = true;
					}

					if (m_motor1_homed && (m_motor2 == nullptr || m_motor2_homed)) {
						sendStatus(STATUS_PREFIX_INFO, "Homing: Precise position found. Moving to offset.");
						homingPhase = SET_OFFSET_START;
					} else if (!isMoving()) {
						abort();
						sendStatus(STATUS_PREFIX_ERROR, "Homing failed during slow search.");
						m_state = STATE_STANDBY;
						homingPhase = HOMING_NONE;
					}
					break;
				}
				
				case SET_OFFSET_START: {
					long offset_steps = m_homingBackoffSteps;
					if (strcmp(m_name, "Z") == 0) {
						offset_steps = -m_homingBackoffSteps;
					}
					moveSteps(offset_steps, m_homingBackoffSps, m_homingAccelSps2, HOMING_TORQUE);
					homingPhase = SET_OFFSET_WAIT_TO_START;
					break;
				}
				case SET_OFFSET_WAIT_TO_START:
					if(isMoving()) {
						homingPhase = SET_OFFSET_MOVING;
					}
					break;
				case SET_OFFSET_MOVING:
					if (!isMoving()) {
						sendStatus(STATUS_PREFIX_INFO, "Homing: Offset position reached.");
						homingPhase = SET_ZERO;
					}
					break;

				case SET_ZERO:
					m_motor1->PositionRefSet(0);
					if (m_motor2) m_motor2->PositionRefSet(0);
					m_homed = true;

					if (m_activeCommand) {
						char doneMsg[64];
						snprintf(doneMsg, sizeof(doneMsg), "%s complete. Current position: %.2f", m_activeCommand, getPositionMm());
						sendStatus(STATUS_PREFIX_DONE, doneMsg);
						m_activeCommand = nullptr;
					}
					m_state = STATE_STANDBY;
					homingPhase = HOMING_NONE;
					break;

				default:
					abort();
					sendStatus(STATUS_PREFIX_ERROR, "Unknown homing phase, aborting.");
					m_state = STATE_STANDBY;
					homingPhase = HOMING_NONE;
					break;
			}
			break;
		}
	}
}


void Axis::abort() {
	m_motor1->MoveStopAbrupt();
	if (m_motor2) m_motor2->MoveStopAbrupt();
	m_activeCommand = nullptr;
}

bool Axis::isMoving() {
	bool motor1Moving = m_motor1->StatusReg().bit.StepsActive;
	bool motor2Moving = m_motor2 ? m_motor2->StatusReg().bit.StepsActive : false;
	return motor1Moving || motor2Moving;
}

const char* Axis::getStateString() {
	if (m_state == STATE_HOMING) return "Homing";
	if (m_state == STATE_MOVING) return "Moving";
	if (m_state == STATE_STARTING_MOVE) return "Starting";
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