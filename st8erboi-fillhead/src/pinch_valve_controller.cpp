#include "pinch_valve_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/**
 * @brief Constructs the PinchValve controller.
 */
PinchValve::PinchValve(const char* name, MotorDriver* motor, CommsController* comms) {
	m_name = name;
	m_motor = motor;
	m_comms = comms;
	m_state = VALVE_IDLE;
	m_isHomed = false;
	m_homingStartTime = 0;
	m_torqueLimit = 20.0f;
	m_smoothedTorque = 0.0f;
	m_firstTorqueReading = true;
	m_homingPhase = HOMING_PHASE_IDLE;
	m_opPhase = PHASE_IDLE;
	m_moveStartTime = 0;
}

/**
 * @brief Initializes the motor hardware for the pinch valve.
 */
void PinchValve::setup() {
	m_motor->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	m_motor->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	m_motor->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
	m_motor->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);
	m_motor->EnableRequest(true);
}

/**
 * @brief The main update loop for the valve's state machine.
 */
void PinchValve::updateState() {
	switch (m_state) {
		case VALVE_IDLE:
		case VALVE_OPEN:
		case VALVE_CLOSED:
		case VALVE_OPERATION_ERROR:
			// Do nothing in these terminal states
			m_opPhase = PHASE_IDLE;
			break;

		case VALVE_HOMING: {
			// Homing logic remains the same...
			if (Milliseconds() - m_homingStartTime > MAX_HOMING_DURATION_MS) {
				abort();
				m_state = VALVE_OPERATION_ERROR;
				m_homingPhase = HOMING_PHASE_IDLE;
				char errorMsg[128];
				snprintf(errorMsg, sizeof(errorMsg), "%s homing timeout.", m_name);
				m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
				return;
			}
			
			switch (m_homingPhase) {
				case INITIAL_BACKOFF_START:
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Performing initial backoff.");
					m_torqueLimit = PINCH_HOMING_BACKOFF_TORQUE_PERCENT;
					moveSteps(-m_homingBackoffSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = INITIAL_BACKOFF_WAIT_TO_START;
					break;
				case INITIAL_BACKOFF_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = INITIAL_BACKOFF_MOVING;
					} else if (Milliseconds() - m_homingStartTime > 500) {
						 m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Initial backoff did not start, proceeding.");
						 m_homingPhase = RAPID_SEARCH_START;
					}
					break;
				case INITIAL_BACKOFF_MOVING:
					if (checkTorqueLimit() || !m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = RAPID_SEARCH_START;
					}
					break;
				case RAPID_SEARCH_START:
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Starting rapid search.");
					m_torqueLimit = PINCH_HOMING_SEARCH_TORQUE_PERCENT;
					moveSteps(m_homingDistanceSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = RAPID_SEARCH_WAIT_TO_START;
					break;
				case RAPID_SEARCH_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) m_homingPhase = RAPID_SEARCH_MOVING;
					break;
				case RAPID_SEARCH_MOVING:
					if (checkTorqueLimit()) {
						m_homingPhase = BACKOFF_START;
					} else if (!m_motor->StatusReg().bit.StepsActive) {
						m_state = VALVE_OPERATION_ERROR;
						m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing failed: move finished before hard stop.");
					}
					break;
				case BACKOFF_START:
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Backing off hard stop.");
					m_torqueLimit = PINCH_HOMING_BACKOFF_TORQUE_PERCENT;
					moveSteps(-m_homingBackoffSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = BACKOFF_WAIT_TO_START;
					break;
				case BACKOFF_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) m_homingPhase = BACKOFF_MOVING;
					break;
				case BACKOFF_MOVING:
					if (!m_motor->StatusReg().bit.StepsActive) m_homingPhase = SLOW_SEARCH_START;
					break;
				case SLOW_SEARCH_START:
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Starting slow search.");
					m_torqueLimit = PINCH_HOMING_SEARCH_TORQUE_PERCENT;
					moveSteps(m_homingBackoffSteps * 2, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = SLOW_SEARCH_WAIT_TO_START;
					break;
				case SLOW_SEARCH_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) m_homingPhase = SLOW_SEARCH_MOVING;
					break;
				case SLOW_SEARCH_MOVING:
					if (checkTorqueLimit()) {
						m_homingPhase = SET_OFFSET_START;
					} else if (!m_motor->StatusReg().bit.StepsActive) {
						m_state = VALVE_OPERATION_ERROR;
						m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing failed during slow search.");
					}
					break;
				case SET_OFFSET_START:
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Moving to final offset.");
					m_torqueLimit = PINCH_HOMING_BACKOFF_TORQUE_PERCENT;
					moveSteps(-m_homingFinalOffsetSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = SET_OFFSET_WAIT_TO_START;
					break;
				case SET_OFFSET_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) m_homingPhase = SET_OFFSET_MOVING;
					break;
				case SET_OFFSET_MOVING:
					if (checkTorqueLimit() || !m_motor->StatusReg().bit.StepsActive) m_homingPhase = SET_ZERO;
					break;
				case SET_ZERO:
					m_motor->PositionRefSet(0); 
					m_isHomed = true;
					m_state = VALVE_OPEN;
					m_homingPhase = HOMING_PHASE_IDLE;
					char msg[64];
					snprintf(msg, sizeof(msg), "%s homing complete. Valve is OPEN.", m_name);
					m_comms->sendStatus(STATUS_PREFIX_DONE, msg);
					break;
				default:
					abort();
					m_state = VALVE_OPERATION_ERROR;
					m_homingPhase = HOMING_PHASE_IDLE;
					break;
			}
			break;
		}
		
		case VALVE_CLOSING:
			switch(m_opPhase) {
				case PHASE_START:
					
					break;
				case PHASE_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_opPhase = PHASE_MOVING;
					} else if (Milliseconds() - m_moveStartTime > 500) {
						m_state = VALVE_OPERATION_ERROR;
						m_opPhase = PHASE_IDLE;
						m_comms->sendStatus(STATUS_PREFIX_ERROR, "Close failed: Motor did not start moving.");
					}
					break;
				case PHASE_MOVING:
					if (checkTorqueLimit()) {
						m_state = VALVE_CLOSED;
						m_opPhase = PHASE_IDLE;
						char msg[64];
						snprintf(msg, sizeof(msg), "%s closed on torque.", m_name);
						m_comms->sendStatus(STATUS_PREFIX_DONE, msg);
					} else if (!m_motor->StatusReg().bit.StepsActive) {
						m_state = VALVE_OPERATION_ERROR;
						m_opPhase = PHASE_IDLE;
						m_comms->sendStatus(STATUS_PREFIX_ERROR, "Close failed: move finished before torque limit.");
					}
					break;
				default:
					m_state = VALVE_IDLE;
					m_opPhase = PHASE_IDLE;
					break;
			}
			break;

		case VALVE_OPENING:
		case VALVE_JOGGING: // Opening and Jogging have the same logic
			switch(m_opPhase) {
				case PHASE_START:
					break;
				case PHASE_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_opPhase = PHASE_MOVING;
					} else if (Milliseconds() - m_moveStartTime > 500) {
						m_state = VALVE_OPERATION_ERROR;
						m_opPhase = PHASE_IDLE;
						m_comms->sendStatus(STATUS_PREFIX_ERROR, "Move failed: Motor did not start.");
					}
					break;
				case PHASE_MOVING:
					if (checkTorqueLimit()) { // Safety check
						m_state = VALVE_OPERATION_ERROR;
						m_opPhase = PHASE_IDLE;
					} else if (!m_motor->StatusReg().bit.StepsActive) {
						// Success
						if (m_state == VALVE_OPENING) {
							m_state = VALVE_OPEN;
						} else {
							m_state = VALVE_IDLE;
						}
						m_opPhase = PHASE_IDLE;
						m_comms->sendStatus(STATUS_PREFIX_DONE, "Move complete.");
					}
					break;
				default:
					m_state = VALVE_IDLE;
					m_opPhase = PHASE_IDLE;
					break;
			}
			break;
		
		default:
			m_state = VALVE_IDLE;
			break;
	}
}

/**
 * @brief Routes a command to the appropriate handler function.
 */
void PinchValve::handleCommand(Command cmd, const char* args) {
	if (m_state != VALVE_IDLE && m_state != VALVE_OPEN && m_state != VALVE_CLOSED) {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "%s is busy. Command ignored.", m_name);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
		return;
	}

	switch(cmd) {
		case CMD_INJECTION_VALVE_HOME:
		case CMD_VACUUM_VALVE_HOME:
			home();
			break;
		case CMD_INJECTION_VALVE_OPEN:
		case CMD_VACUUM_VALVE_OPEN:
			open();
			break;
		case CMD_INJECTION_VALVE_CLOSE:
		case CMD_VACUUM_VALVE_CLOSE:
			close();
			break;
		case CMD_INJECTION_VALVE_JOG:
		case CMD_VACUUM_VALVE_JOG:
			jog(args);
			break;
		default:
			break;
	}
}

/**
 * @brief Initiates the homing sequence for the pinch valve.
 */
void PinchValve::home() {
	m_isHomed = false;
	m_state = VALVE_HOMING;
	m_homingPhase = INITIAL_BACKOFF_START;
	m_homingStartTime = Milliseconds();

	m_homingDistanceSteps = (long)(fabs(PINCH_HOMING_STROKE_MM) * STEPS_PER_MM_PINCH);
	m_homingBackoffSteps = (long)(PINCH_VALVE_HOMING_BACKOFF_MM * STEPS_PER_MM_PINCH);
	m_homingFinalOffsetSteps = (long)(PINCH_VALVE_FINAL_OFFSET_MM * STEPS_PER_MM_PINCH);
	m_homingUnifiedSps = (int)fabs(PINCH_HOMING_UNIFIED_VEL_MMS * STEPS_PER_MM_PINCH);
	m_homingAccelSps2 = (int)fabs(PINCH_HOMING_ACCEL_MMSS * STEPS_PER_MM_PINCH);

	char msg[64];
	snprintf(msg, sizeof(msg), "%s homing started.", m_name);
	m_comms->sendStatus(STATUS_PREFIX_INFO, msg);
}

/**
 * @brief Kicks off the state machine to open the valve.
 */
void PinchValve::open() {
	if (!m_isHomed) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Cannot open: not homed.");
		return;
	}
	m_state = VALVE_OPENING;
	m_opPhase = PHASE_WAIT_TO_START;
	m_moveStartTime = Milliseconds();
	m_torqueLimit = JOG_DEFAULT_TORQUE_PERCENT;
	
	long target_steps = 0;
	long current_steps = m_motor->PositionRefCommanded();
	int vel_sps = (int)(PINCH_VALVE_OPEN_VEL_MMS * STEPS_PER_MM_PINCH);
	int accel_sps2 = (int)(PINCH_VALVE_OPEN_ACCEL_MMSS * STEPS_PER_MM_PINCH);

	moveSteps(target_steps - current_steps, vel_sps, accel_sps2);
}

/**
 * @brief Kicks off the state machine to close the valve.
 */
void PinchValve::close() {
	if (!m_isHomed) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Cannot close: not homed.");
		return;
	}
	m_state = VALVE_CLOSING;
	m_opPhase = PHASE_WAIT_TO_START;
	m_moveStartTime = Milliseconds();
	m_torqueLimit = PINCH_VALVE_PINCH_TORQUE_PERCENT;
	
	long long_move_steps = (long)(PINCH_HOMING_STROKE_MM * STEPS_PER_MM_PINCH);
	int vel_sps = (int)(PINCH_VALVE_PINCH_VEL_MMS * STEPS_PER_MM_PINCH);
	int accel_sps2 = (int)(PINCH_JOG_DEFAULT_ACCEL_MMSS * STEPS_PER_MM_PINCH);

	moveSteps(long_move_steps, vel_sps, accel_sps2);
}

/**
 * @brief Kicks off the state machine to jog the valve.
 */
void PinchValve::jog(const char* args) {
	if (!args) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid jog command arguments.");
		return;
	}
	m_state = VALVE_JOGGING;
	m_opPhase = PHASE_WAIT_TO_START;
	m_moveStartTime = Milliseconds();
	m_torqueLimit = JOG_DEFAULT_TORQUE_PERCENT;

	float dist_mm = atof(args);
	long steps = (long)(dist_mm * STEPS_PER_MM_PINCH);
	int vel_sps = (int)(PINCH_JOG_DEFAULT_VEL_MMS * STEPS_PER_MM_PINCH);
	int accel_sps2_val = (int)(PINCH_JOG_DEFAULT_ACCEL_MMSS * STEPS_PER_MM_PINCH);
	
	moveSteps(steps, vel_sps, accel_sps2_val);
}

/**
 * @brief Low-level move command.
 */
void PinchValve::moveSteps(long steps, int velocity_sps, int accel_sps2) {
	if (m_motor->StatusReg().bit.Enabled) {
		m_firstTorqueReading = true;
		m_motor->VelMax(velocity_sps);
		m_motor->AccelMax(accel_sps2);
		m_motor->Move(steps);
	} else {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "%s motor is not enabled.", m_name);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
	}
}

void PinchValve::enable() { m_motor->EnableRequest(true); }
void PinchValve::disable() { m_motor->EnableRequest(false); }
void PinchValve::abort() { m_motor->MoveStopDecel(); m_state = VALVE_IDLE; }
void PinchValve::reset() { if (m_motor->StatusReg().bit.StepsActive) { m_motor->MoveStopDecel(); } m_state = VALVE_IDLE; }

/**
 * @brief Gets the instantaneous, raw torque value from the motor.
 */
float PinchValve::getInstantaneousTorque() {
	float currentRawTorque = m_motor->HlfbPercent();
	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return 0.0f;
	}
	return currentRawTorque;
}

/**
 * @brief Gets a smoothed torque value using an EWMA filter.
 */
float PinchValve::getSmoothedTorque() {
	float currentRawTorque = getInstantaneousTorque();
    if (m_firstTorqueReading) {
        m_smoothedTorque = currentRawTorque;
        m_firstTorqueReading = false;
    } else {
        m_smoothedTorque = EWMA_ALPHA_TORQUE * currentRawTorque + (1.0f - EWMA_ALPHA_TORQUE) * m_smoothedTorque;
    }
    return m_smoothedTorque;
}

/**
 * @brief Checks if the torque on the motor has exceeded the current limit.
 */
bool PinchValve::checkTorqueLimit() {
    if (m_motor->StatusReg().bit.StepsActive) {
        float torque = getInstantaneousTorque(); 
        if (torque > m_torqueLimit) {
            m_motor->MoveStopAbrupt();
            char torque_msg[STATUS_MESSAGE_BUFFER_SIZE];
            snprintf(torque_msg, sizeof(torque_msg), "%s TORQUE LIMIT REACHED (%.1f%%)", m_name, m_torqueLimit);
            m_comms->sendStatus(STATUS_PREFIX_INFO, torque_msg);
            return true;
        }
    }
    return false;
}

/**
 * @brief Assembles the telemetry string for this valve using smoothed torque.
 */
const char* PinchValve::getTelemetryString() {
	float displayTorque = getSmoothedTorque();
	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"%s_pos:%.2f,%s_torque:%.1f,%s_homed:%d,%s_state:%d",
	m_name, (float)m_motor->PositionRefCommanded() / STEPS_PER_MM_PINCH,
	m_name, displayTorque,
	m_name, (int)m_isHomed,
	m_name, m_state
	);
	return m_telemetryBuffer;
}

bool PinchValve::isBusy() const {
	return m_state == VALVE_HOMING || m_state == VALVE_OPENING || m_state == VALVE_CLOSING || m_state == VALVE_JOGGING;
}

bool PinchValve::isInFault() const {
	return m_motor->StatusReg().bit.MotorInFault;
}
