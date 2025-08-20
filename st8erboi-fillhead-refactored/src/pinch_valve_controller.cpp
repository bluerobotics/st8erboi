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
void PinchValve::update() {
	switch (m_state) {
		case VALVE_IDLE:
		case VALVE_OPEN:
		case VALVE_CLOSED:
		case VALVE_ERROR:
			// Do nothing in these terminal states
			break;

		case VALVE_HOMING: {
			// Check for a global timeout on the whole homing sequence
			if (Milliseconds() - m_homingStartTime > MAX_HOMING_DURATION_MS) {
				abort();
				m_state = VALVE_ERROR;
				m_homingPhase = HOMING_PHASE_IDLE;
				char errorMsg[128];
				snprintf(errorMsg, sizeof(errorMsg), "%s homing timeout.", m_name);
				m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
				return;
			}
			
			// Homing Phase State Machine
			switch (m_homingPhase) {
				case INITIAL_BACKOFF_START: {
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Performing initial backoff.");
					m_torqueLimit = PINCH_HOMING_BACKOFF_TORQUE_PERCENT;
					moveSteps(-m_homingBackoffSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = INITIAL_BACKOFF_WAIT_TO_START;
					break;
				}
				case INITIAL_BACKOFF_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = INITIAL_BACKOFF_MOVING;
					} else if (Milliseconds() - m_homingStartTime > 500) {
						 m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Initial backoff did not start, proceeding to rapid search.");
						 m_homingPhase = RAPID_SEARCH_START;
					}
					break;
				case INITIAL_BACKOFF_MOVING:
					if (getSmoothedTorque() > m_torqueLimit) {
						m_motor->MoveStopAbrupt();
						m_homingPhase = RAPID_SEARCH_START;
						char infoMsg[128];
						snprintf(infoMsg, sizeof(infoMsg), "%s torque limit hit during initial backoff, starting rapid search.", m_name);
						m_comms->sendStatus(STATUS_PREFIX_INFO, infoMsg);
					}
					else if (!m_motor->StatusReg().bit.StepsActive) {
						m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Initial backoff complete.");
						m_homingPhase = RAPID_SEARCH_START;
					}
					break;

				case RAPID_SEARCH_START: {
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: rapid search movesteps called.");
					m_torqueLimit = PINCH_HOMING_SEARCH_TORQUE_PERCENT;
					moveSteps(m_homingDistanceSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = RAPID_SEARCH_WAIT_TO_START;
					break;
				}
				case RAPID_SEARCH_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = RAPID_SEARCH_MOVING;
					}
					break;

				case RAPID_SEARCH_MOVING:
					if (getSmoothedTorque() > m_torqueLimit) {
						m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Rapid search torque limit hit.");
						m_motor->MoveStopAbrupt();
						m_homingPhase = BACKOFF_START;
					}
					break;

				case BACKOFF_START: {
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: backoff moveSteps called.");
					m_torqueLimit = PINCH_HOMING_BACKOFF_TORQUE_PERCENT;
					moveSteps(-m_homingBackoffSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = BACKOFF_WAIT_TO_START;
					break;
				}
				case BACKOFF_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = BACKOFF_MOVING;
					}
					break;
				case BACKOFF_MOVING:
					if (!m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = SLOW_SEARCH_START;
					}
					break;

				case SLOW_SEARCH_START: {
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: slow search moveSteps called.");
					m_torqueLimit = PINCH_HOMING_SEARCH_TORQUE_PERCENT;
					moveSteps(m_homingBackoffSteps * 2, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = SLOW_SEARCH_WAIT_TO_START;
					break;
				}
				case SLOW_SEARCH_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = SLOW_SEARCH_MOVING;
					}
					break;
				case SLOW_SEARCH_MOVING:
					if (getSmoothedTorque() > m_torqueLimit) {
						m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Precise position found.");
						m_motor->MoveStopAbrupt();
						m_homingPhase = SET_OFFSET_START;
					}
					break;

				case SET_OFFSET_START: {
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: offset moveSteps called.");
					m_torqueLimit = PINCH_HOMING_BACKOFF_TORQUE_PERCENT;
					moveSteps(-m_homingFinalOffsetSteps, m_homingUnifiedSps, m_homingAccelSps2);
					m_homingPhase = SET_OFFSET_WAIT_TO_START;
					break;
				}
				case SET_OFFSET_WAIT_TO_START:
					if (m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = SET_OFFSET_MOVING;
					}
					break;
				case SET_OFFSET_MOVING:
					if (!m_motor->StatusReg().bit.StepsActive) {
						m_homingPhase = SET_ZERO;
					}
					break;

				case SET_ZERO: {
					// MODIFIED: The '0' position is now the fully OPEN/retracted position.
					m_motor->PositionRefSet(0); 
					m_isHomed = true;
					m_state = VALVE_OPEN; // The final state is homed and OPEN
					m_homingPhase = HOMING_PHASE_IDLE;
					char msg[64];
					snprintf(msg, sizeof(msg), "%s homing complete. Valve is OPEN.", m_name);
					m_comms->sendStatus(STATUS_PREFIX_DONE, msg);
					break;
				}
				default:
					abort();
					m_state = VALVE_ERROR;
					m_homingPhase = HOMING_PHASE_IDLE;
					char errorMsg[128];
					snprintf(errorMsg, sizeof(errorMsg), "%s unknown homing phase, aborting.", m_name);
					m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
					break;
			}
			break; // End case VALVE_HOMING
		}
		
		// NEW: State for handling the torque-based closing operation
		case VALVE_CLOSING_ON_TORQUE:
			if (getSmoothedTorque() > m_torqueLimit) {
				// SUCCESS: We've hit the target torque
				m_motor->MoveStopAbrupt(); // Stop immediately
				m_state = VALVE_CLOSED;
				char msg[64];
				snprintf(msg, sizeof(msg), "%s closed on torque.", m_name);
				m_comms->sendStatus(STATUS_PREFIX_DONE, msg);
			} else if (!m_motor->StatusReg().bit.StepsActive) {
				// ERROR: The move completed before reaching the target torque
				m_state = VALVE_ERROR;
				char errorMsg[128];
				snprintf(errorMsg, sizeof(errorMsg), "%s move finished before pinch torque was reached.", m_name);
				m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
			}
			break;

		case VALVE_MOVING:
			// This state now handles JOG and OPEN moves.
			if (getSmoothedTorque() > m_torqueLimit) {
				abort();
				m_state = VALVE_ERROR;
				char errorMsg[128];
				snprintf(errorMsg, sizeof(errorMsg), "%s move stopped on safety torque limit.", m_name);
				m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
			} else if (!m_motor->StatusReg().bit.StepsActive) {
				// Move is complete. Determine if we are now open or just idle from a jog.
				if (abs(m_motor->PositionRefCommanded()) < 10) { // If we are very close to 0
					m_state = VALVE_OPEN;
				} else {
					m_state = VALVE_IDLE;
				}
				char msg[64];
				snprintf(msg, sizeof(msg), "%s move complete.", m_name);
				m_comms->sendStatus(STATUS_PREFIX_DONE, msg);
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
void PinchValve::handleCommand(UserCommand cmd, const char* args) {
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
 * @brief Initiates the homing sequence for the pinch valve. The 'home' position is fully OPEN.
 */
void PinchValve::home() {
	if (m_state != VALVE_IDLE && m_state != VALVE_OPEN && m_state != VALVE_CLOSED) {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "%s is busy and cannot home.", m_name);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
		return;
	}
	m_isHomed = false;
	m_state = VALVE_HOMING;
	m_homingPhase = INITIAL_BACKOFF_START;
	m_homingStartTime = Milliseconds();

	// Load parameters from config.h
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
 * @brief MODIFIED: Commands the valve to move to the fully open (home, 0mm) position.
 */
void PinchValve::open() {
	if (!m_isHomed) {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "Cannot open %s, must be homed first.", m_name);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
		return;
	}
	m_state = VALVE_MOVING;
	m_torqueLimit = JOG_DEFAULT_TORQUE_PERCENT; // Use a safe torque for this positional move
	
	long target_steps = 0; // The 'open' position is the homed '0' position
	long current_steps = m_motor->PositionRefCommanded();
	
	int vel_sps = (int)(PINCH_VALVE_OPEN_VEL_MMS * STEPS_PER_MM_PINCH);
	int accel_sps2 = (int)(PINCH_VALVE_OPEN_ACCEL_MMSS * STEPS_PER_MM_PINCH);

	moveSteps(target_steps - current_steps, vel_sps, accel_sps2);
}

/**
 * @brief MODIFIED: Commands the valve to close by moving until a target torque is reached.
 */
void PinchValve::close() {
	if (!m_isHomed) {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "Cannot close %s, must be homed first.", m_name);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
		return;
	}
	m_state = VALVE_CLOSING_ON_TORQUE;
	m_torqueLimit = PINCH_VALVE_PINCH_TORQUE_PERCENT; // Set the target pinch torque
	
	// Command a long move to ensure we hit the tube
	long long_move_steps = (long)(PINCH_HOMING_STROKE_MM * STEPS_PER_MM_PINCH);
	
	int vel_sps = (int)(PINCH_VALVE_PINCH_VEL_MMS * STEPS_PER_MM_PINCH);
	int accel_sps2 = (int)(PINCH_JOG_DEFAULT_ACCEL_MMSS * STEPS_PER_MM_PINCH); // Use jog accel

	moveSteps(long_move_steps, vel_sps, accel_sps2);
}

/**
 * @brief Jogs the valve by a specified distance.
 */
void PinchValve::jog(const char* args) {
	if (m_state != VALVE_IDLE && m_state != VALVE_OPEN && m_state != VALVE_CLOSED) {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "%s is busy and cannot jog.", m_name);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
		return;
	}
	if (!args) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid jog command arguments.");
		return;
	}
	m_state = VALVE_MOVING;
	float dist_mm = atof(args);
	long steps = (long)(dist_mm * STEPS_PER_MM_PINCH);
	int vel_sps = (int)(PINCH_JOG_DEFAULT_VEL_MMS * STEPS_PER_MM_PINCH);
	int accel_sps2_val = (int)(PINCH_JOG_DEFAULT_ACCEL_MMSS * STEPS_PER_MM_PINCH);
	m_torqueLimit = JOG_DEFAULT_TORQUE_PERCENT;
	moveSteps(steps, vel_sps, accel_sps2_val);
}

/**
 * @brief Low-level move command.
 */
void PinchValve::moveSteps(long steps, int velocity_sps, int accel_sps2) {
	if (m_motor->StatusReg().bit.Enabled) {
		m_firstTorqueReading = true; // Reset torque filter for new move
		m_motor->VelMax(velocity_sps);
		m_motor->AccelMax(accel_sps2);
		m_motor->Move(steps);
	} else {
		char errorMsg[128];
		snprintf(errorMsg, sizeof(errorMsg), "%s motor is not enabled.", m_name);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
	}
}

void PinchValve::enable() {
	m_motor->EnableRequest(true);
}

void PinchValve::disable() {
	m_motor->EnableRequest(false);
}

void PinchValve::abort() {
	m_motor->MoveStopDecel();
	m_state = VALVE_IDLE;
}

void PinchValve::reset() {
	if (m_motor->StatusReg().bit.StepsActive) {
		m_motor->MoveStopDecel();
	}
	m_state = VALVE_IDLE;
}

/**
 * @brief Gets the instantaneous (raw) torque value from the motor.
 */
float PinchValve::getSmoothedTorque() {
	float currentRawTorque = m_motor->HlfbPercent();
	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return 0.0f; // Return a safe value if torque is invalid
	}
	// Bypassing the smoothing filter to return the raw value directly.
	return currentRawTorque;
}

/**
 * @brief Assembles the telemetry string for this valve.
 */
const char* PinchValve::getTelemetryString() {
	// The position is now relative to the OPEN state (0mm).
	// A positive value indicates how far the valve has moved towards closing.
	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"%s_pos:%.2f,%s_homed:%d,%s_state:%d",
	m_name, (float)m_motor->PositionRefCommanded() / STEPS_PER_MM_PINCH,
	m_name, (int)m_isHomed,
	m_name, m_state
	);
	return m_telemetryBuffer;
}
