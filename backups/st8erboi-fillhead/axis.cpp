#include "axis.h"
#include "fillhead.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Axis::Axis(Fillhead* controller, const char* name, MotorDriver* motor1, MotorDriver* motor2, float stepsPerMm, float minPosMm, float maxPosMm) {
	m_controller = controller;
	m_name = name;
	m_motor1 = motor1;
	m_motor2 = motor2;
	m_stepsPerMm = stepsPerMm;
	m_minPosMm = minPosMm;
	m_maxPosMm = maxPosMm;
	
	m_state = STATE_STANDBY;
	homingPhase = HOMING_NONE;
	m_homed = false;
	
	m_smoothedTorqueM1 = 0.0f;
	m_smoothedTorqueM2 = 0.0f;
	m_firstTorqueReadM1 = true;
	m_firstTorqueReadM2 = true;
	m_torqueLimit = 0.0f;
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

void Axis::startMove(float target_mm, float vel_mms, float accel_mms2, int torque, MoveType moveType) {
	if ((m_motor1 && m_motor1->StatusReg().bit.MotorInFault) || (m_motor2 && m_motor2->StatusReg().bit.MotorInFault)){
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Motor in fault");
		return;
	}
	if (isMoving()) {
		// This can be called frequently by routines, so don't flood with errors.
		// The calling function is responsible for checking this if it's an issue.
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

	// Check if the final position is within the machine's travel limits
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
	MoveType moveType = ABSOLUTE; // Default to absolute positioning
	float target_mm, vel_mms, accel_mms2;
	int torque;
	char modeStr[5] = {0}; // Increased size for safety

	// Try parsing with a mode string (ABS or INC) first
	int parsed_count = sscanf(args, "%4s %f %f %f %d", modeStr, &target_mm, &vel_mms, &accel_mms2, &torque);

	if (parsed_count == 5 && (strcmp(modeStr, "ABS") == 0 || strcmp(modeStr, "INC") == 0)) {
		if (strcmp(modeStr, "INC") == 0) {
			moveType = INCREMENTAL;
		}
		// moveType is already ABSOLUTE by default
	}
	// If that fails, try parsing without the mode string, which implies an absolute move
	else if (sscanf(args, "%f %f %f %d", &target_mm, &vel_mms, &accel_mms2, &torque) == 4) {
		moveType = ABSOLUTE;
	}
	// If both parsing attempts fail
	else {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid MOVE format. Use [ABS|INC] <pos> <vel> <accel> <torque>");
		return;
	}

	char infoMsg[128];
	snprintf(infoMsg, sizeof(infoMsg), "MOVE %s command received", moveType == ABSOLUTE ? "ABSOLUTE" : "INCREMENTAL");
	sendStatus(STATUS_PREFIX_INFO, infoMsg);
	startMove(target_mm, vel_mms, accel_mms2, torque, moveType);
}

void Axis::handleHome(const char* args) {
	if ((m_motor1 && m_motor1->StatusReg().bit.MotorInFault) || (m_motor2 && m_motor2->StatusReg().bit.MotorInFault)){
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Motor in fault");
		return;
	}
	if (isMoving()) {
		sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Axis already moving");
		return;
	}
	
	float max_dist_mm;
	if (!args || sscanf(args, "%d %f", &m_homingTorque, &max_dist_mm) != 2) {
		sendStatus(STATUS_PREFIX_ERROR, "Invalid HOME format.");
		return;
	}
	sendStatus(STATUS_PREFIX_INFO, "HOME command received");

	m_homed = false;
	m_state = STATE_HOMING;
	homingPhase = DEBIND_START;
	
	float rapid_vel_mms = 15;
	float touch_vel_mms = 5.0;
	float backoff_mm = 5.0;
	
	m_homingDistanceSteps = (long)(max_dist_mm * m_stepsPerMm);
	m_homingBackoffSteps = (long)(backoff_mm * m_stepsPerMm);
	m_homingRapidSps = (int)fabs(rapid_vel_mms * m_stepsPerMm);
	m_homingTouchSps = (int)fabs(touch_vel_mms * m_stepsPerMm);
}

void Axis::updateState() {
	switch (m_state) {
		case STATE_STANDBY:
		break;
		
		case STATE_STARTING_MOVE:
		if ((m_motor1 && m_motor1->StatusReg().bit.MotorInFault) || (m_motor2 && m_motor2->StatusReg().bit.MotorInFault)){
			sendStatus(STATUS_PREFIX_ERROR, "Cannot start move: Motor in fault");
			return;
		}
		if (isMoving()){
			// Don't send status here, it's too noisy for the demo
			m_state = STATE_MOVING;
			break;
		}
		break;
		
		case STATE_MOVING:
		if ((m_motor1 && checkTorqueLimit(m_motor1)) || (m_motor2 && checkTorqueLimit(m_motor2))) {
			abort();
			sendStatus(STATUS_PREFIX_ERROR, "MOVE aborted due to torque limit, entering STANDBY.");
			m_state = STATE_STANDBY;
		}
		else if (!isMoving()) {
			// Don't send status here, it's too noisy for the demo
			m_state = STATE_STANDBY;
		}
		break;
		
		case STATE_HOMING:
		// Homing logic remains unchanged
		switch (homingPhase) {
			case DEBIND_START:
			moveSteps(400, 200, MAX_ACC, MAX_TRQ);
			homingPhase = DEBIND_WAIT_TO_START;
			break;
			
			case DEBIND_WAIT_TO_START:
			if (isMoving()) {
				sendStatus(STATUS_PREFIX_INFO, "DEBIND STARTED, entering DEBIND_MOVING");
				homingPhase = DEBIND_MOVING;
				break;
			}
			break;
			
			case DEBIND_MOVING:
			if ((m_motor1 && checkTorqueLimit(m_motor1)) || (m_motor2 && checkTorqueLimit(m_motor2))) {
				abort();
				sendStatus(STATUS_PREFIX_ERROR, "Debind aborted due to torque limit, entering STANDBY.");
				m_state = STATE_STANDBY;
				homingPhase = HOMING_NONE;
			}
			else if (!isMoving()) {
				sendStatus(STATUS_PREFIX_DONE, "Backoff done, entering TOUCH_START");
				homingPhase = RAPID_START;
			}
			break;
			
			case RAPID_START:
			moveSteps(-m_homingDistanceSteps, m_homingRapidSps, MAX_ACC, m_homingTorque);
			homingPhase = RAPID_WAIT_TO_START;
			break;
			
			case RAPID_WAIT_TO_START:
			if (isMoving()) {
				sendStatus(STATUS_PREFIX_INFO, "HOMING STARTED, entering RAPID_MOVING");
				homingPhase = RAPID_MOVING;
				break;
			}
			break;

			case RAPID_MOVING: {
				bool motor1_at_limit = (m_motor1 && checkTorqueLimit(m_motor1));
				bool motor2_at_limit = (m_motor2 && checkTorqueLimit(m_motor2));

				if (motor1_at_limit) m_motor1->MoveStopAbrupt();
				if (motor2_at_limit) m_motor2->MoveStopAbrupt();

				if ((motor1_at_limit || !m_motor1) && (motor2_at_limit || !m_motor2)) {
					sendStatus(STATUS_PREFIX_INFO, "Endstops hit on rapid, entering BACKOFF_START");
					homingPhase = BACKOFF_START;
				}
				break;
			}
			
			case BACKOFF_START:
			moveSteps(m_homingBackoffSteps, m_homingRapidSps, MAX_ACC, MAX_TRQ);
			homingPhase = BACKOFF_WAIT_TO_START;
			break;
			
			case BACKOFF_WAIT_TO_START:
			if (isMoving()) {
				sendStatus(STATUS_PREFIX_INFO, "BACKOFF STARTED, entering BACKOFF_MOVING");
				homingPhase = BACKOFF_MOVING;
				break;
			}
			break;
			
			case BACKOFF_MOVING:
			if ((m_motor1 && checkTorqueLimit(m_motor1)) || (m_motor2 && checkTorqueLimit(m_motor2))) {
				abort();
				sendStatus(STATUS_PREFIX_ERROR, "Backoff aborted due to torque limit, entering STANDBY.");
				m_state = STATE_STANDBY;
				homingPhase = HOMING_NONE;
			}
			else if (!isMoving()) {
				sendStatus(STATUS_PREFIX_DONE, "Backoff done, entering TOUCH_START");
				homingPhase = TOUCH_START;
			}
			break;

			case TOUCH_START:
			moveSteps(-m_homingBackoffSteps*2, m_homingTouchSps, MAX_ACC, m_homingTorque);
			homingPhase = TOUCH_WAIT_TO_START;
			break;
			
			case TOUCH_WAIT_TO_START:
			if (isMoving()) {
				sendStatus(STATUS_PREFIX_INFO, "TOUCH STARTED, entering TOUCH_MOVING");
				homingPhase = TOUCH_MOVING;
				break;
			}
			break;
			
			case TOUCH_MOVING: {
				bool motor1_at_limit = (m_motor1 && checkTorqueLimit(m_motor1));
				bool motor2_at_limit = (m_motor2 && checkTorqueLimit(m_motor2));

				if (motor1_at_limit) m_motor1->MoveStopAbrupt();
				if (motor2_at_limit) m_motor2->MoveStopAbrupt();

				if ((motor1_at_limit || !m_motor1) && (motor2_at_limit || !m_motor2)) {
					sendStatus(STATUS_PREFIX_INFO, "Endstops hit on touch, entering DESTRESS_DISABLE");
					homingPhase = DESTRESS_DISABLE;
				}
				break;
			}
			
			case DESTRESS_DISABLE:
			disable();
			m_delay_target_ms = Milliseconds() + 250;
			homingPhase = AWAIT_DESTRESS_TIMER;
			break;
			
			case AWAIT_DESTRESS_TIMER:
			if(Milliseconds() >= m_delay_target_ms) {
				homingPhase = DESTRESS_ENABLE;
			}
			break;
			
			case DESTRESS_ENABLE:
			enable();
			m_delay_target_ms = Milliseconds() + 250;
			homingPhase = AWAIT_ENABLE_TIMER;
			break;
			
			case AWAIT_ENABLE_TIMER:
			if(Milliseconds() >= m_delay_target_ms) {
				homingPhase = RETRACT_START;
			}
			break;
			
			case RETRACT_START: {
				long retractSteps;
				if (strcmp(m_name, "Z") == 0) {
					// Special case for Z axis: retract a fixed 35mm
					retractSteps = (long)(35.0f * m_stepsPerMm);
					} else {
					// Standard retraction for X and Y axes
					retractSteps = m_homingBackoffSteps;
				}
				moveSteps(retractSteps, m_homingRapidSps, MAX_ACC, MAX_TRQ);
				homingPhase = RETRACT_WAIT_TO_START;
				break;
			}
			
			case RETRACT_WAIT_TO_START:
			if (isMoving()) {
				sendStatus(STATUS_PREFIX_INFO, "RETRACT STARTED, entering RETRACT_MOVING");
				homingPhase = RETRACT_MOVING;
				break;
			}
			break;
			
			case RETRACT_MOVING:
			if ((m_motor1 && checkTorqueLimit(m_motor1)) || (m_motor2 && checkTorqueLimit(m_motor2))) {
				abort();
				sendStatus(STATUS_PREFIX_ERROR, "RETRACT aborted due to torque limit, entering STANDBY.");
				m_state = STATE_STANDBY;
				homingPhase = HOMING_NONE;
			}
			else if (!isMoving()) {
				sendStatus(STATUS_PREFIX_DONE, "Retract done, entering SET_ZERO");
				homingPhase = SET_ZERO;
			}
			break;
			
			case SET_ZERO:
			m_motor1->PositionRefSet(0);
			if(m_motor2) m_motor2->PositionRefSet(0);
			sendStatus(STATUS_PREFIX_DONE, "Zero set, homing complete, entering STANDBY");
			m_homed = true;
			homingPhase = HOMING_NONE;
			m_state = STATE_STANDBY;
			
			break;
			
			default:
			abort();
			sendStatus(STATUS_PREFIX_ERROR, "Unknown homing phase, entering STANDBY");
			m_state = STATE_STANDBY;
			homingPhase = HOMING_NONE;
			break;
		}
		break;
	}
}

void Axis::abort() {
	m_motor1->MoveStopAbrupt();
	if (m_motor2) m_motor2->MoveStopAbrupt();
}

bool Axis::isMoving() {
	bool motor1Moving = m_motor1->StatusReg().bit.StepsActive;
	bool motor2Moving = m_motor2 ? m_motor2->StatusReg().bit.StepsActive : false;
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
