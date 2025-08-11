#include "injector_controller.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

// --- Constructor ---
Injector::Injector(MotorDriver* motorA, MotorDriver* motorB, CommsController* comms) {
	m_motorA = motorA;
	m_motorB = motorB;
	m_comms = comms;
	m_homingState = HOMING_NONE;
	m_homingPhase = HOMING_PHASE_IDLE;
	m_feedState = FEED_STANDBY;
	m_homingMachineDone = false;
	m_homingCartridgeDone = false;
	m_feedingDone = true;
	m_jogDone = true;
	m_homingStartTime = 0;
	m_isEnabled = false;

	// Initialize from config file constants
	m_torqueLimit = DEFAULT_INJECTOR_TORQUE_LIMIT;
	m_torqueOffset = DEFAULT_INJECTOR_TORQUE_OFFSET;
	m_homingDefaultBackoffSteps = HOMING_DEFAULT_BACKOFF_STEPS;
	m_feedDefaultTorquePercent = FEED_DEFAULT_TORQUE_PERCENT;
	m_feedDefaultVelocitySPS = FEED_DEFAULT_VELOCITY_SPS;
	m_feedDefaultAccelSPS2 = FEED_DEFAULT_ACCEL_SPS2;

	m_smoothedTorqueValue0 = 0.0f;
	m_smoothedTorqueValue1 = 0.0f;
	m_firstTorqueReading0 = true;
	m_firstTorqueReading1 = true;
	m_machineHomeReferenceSteps = 0;
	m_cartridgeHomeReferenceSteps = 0;

	fullyResetActiveDispenseOperation();
	m_activeFeedCommand = nullptr;
	m_activeJogCommand = nullptr;
}

// --- Setup ---
void Injector::setup() {
	m_motorA->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	m_motorA->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	m_motorA->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
	m_motorA->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);

	m_motorB->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	m_motorB->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	m_motorB->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
	m_motorB->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);
}

// --- Main State Update ---
void Injector::updateState() {
	// --- Homing State Machine ---
	if (m_homingState != HOMING_NONE) {
		if (Milliseconds() - m_homingStartTime > MAX_HOMING_DURATION_MS) {
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "Injector Homing Error: Timeout. Aborting.");
			abortMove();
			m_homingPhase = HOMING_PHASE_ERROR;
		}

		if (m_homingPhase == HOMING_PHASE_COMPLETE || m_homingPhase == HOMING_PHASE_ERROR) {
			if (m_homingPhase == HOMING_PHASE_COMPLETE) {
				char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
				const char* commandStr = (m_homingState == HOMING_MACHINE) ? CMD_STR_MACHINE_HOME_MOVE : CMD_STR_CARTRIDGE_HOME_MOVE;
				snprintf(doneMsg, sizeof(doneMsg), "%s complete.", commandStr);
				m_comms->sendStatus(STATUS_PREFIX_DONE, doneMsg);
				if (m_homingState == HOMING_MACHINE) m_homingMachineDone = true;
				if (m_homingState == HOMING_CARTRIDGE) m_homingCartridgeDone = true;
				} else {
				m_comms->sendStatus(STATUS_PREFIX_ERROR, "Injector homing sequence ended with error.");
			}
			m_homingState = HOMING_NONE;
			m_homingPhase = HOMING_PHASE_IDLE;
			return;
		}

		int direction = (m_homingState == HOMING_MACHINE) ? -1 : 1;
		switch (m_homingPhase) {
			case HOMING_PHASE_STARTING_MOVE:
			if (Milliseconds() - m_homingStartTime > 50) {
				if (isMoving()) {
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Move started. Entering RAPID_MOVE phase.");
					m_homingPhase = HOMING_PHASE_RAPID_MOVE;
					} else {
					m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Injector motors failed to start moving.");
					m_homingPhase = HOMING_PHASE_ERROR;
				}
			}
			break;
			case HOMING_PHASE_RAPID_MOVE:
			case HOMING_PHASE_TOUCH_OFF:
			if (checkTorqueLimit()) {
				if (m_homingPhase == HOMING_PHASE_RAPID_MOVE) {
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Torque (RAPID). Backing off.");
					m_homingPhase = HOMING_PHASE_BACK_OFF;
					long back_off_s = -direction * m_homingDefaultBackoffSteps;
					move(back_off_s, back_off_s, (int)HOMING_TORQUE_PERCENT, (int)(HOMING_BACK_OFF_VEL_MMS * STEPS_PER_MM_INJECTOR), (int)(HOMING_BACK_OFF_ACCEL_MMSS * STEPS_PER_MM_INJECTOR));
					} else { // TOUCH_OFF
					m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Torque (TOUCH_OFF). Zeroing & Retracting.");
					if (m_homingState == HOMING_MACHINE) {
						m_machineHomeReferenceSteps = m_motorA->PositionRefCommanded();
						} else {
						m_cartridgeHomeReferenceSteps = m_motorA->PositionRefCommanded();
					}
					m_homingPhase = HOMING_PHASE_RETRACT;
					long retract_s = -direction * (long)(HOMING_POST_TOUCH_RETRACT_MM * STEPS_PER_MM_INJECTOR);
					move(retract_s, retract_s, (int)HOMING_TORQUE_PERCENT, (int)(HOMING_RETRACT_VEL_MMS * STEPS_PER_MM_INJECTOR), (int)(HOMING_RETRACT_ACCEL_MMSS * STEPS_PER_MM_INJECTOR));
				}
				} else if (!isMoving()) {
				m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Move completed without hitting torque limit.");
				m_homingPhase = HOMING_PHASE_ERROR;
			}
			break;
			case HOMING_PHASE_BACK_OFF:
			if (!isMoving()) {
				m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Back-off done. Touching off.");
				m_homingPhase = HOMING_PHASE_TOUCH_OFF;
				long touch_off_s = direction * (m_homingDefaultBackoffSteps * 2);
				move(touch_off_s, touch_off_s, (int)HOMING_TORQUE_PERCENT, (int)(HOMING_TOUCH_VEL_MMS * STEPS_PER_MM_INJECTOR), (int)(HOMING_TOUCH_OFF_ACCEL_MMSS * STEPS_PER_MM_INJECTOR));
			}
			break;
			case HOMING_PHASE_RETRACT:
			if (!isMoving()) {
				m_homingPhase = HOMING_PHASE_COMPLETE;
			}
			break;
			default:
			break;
		}
	}

	// --- Feed State Machine ---
	if (m_active_dispense_INJECTION_ongoing) {
		if (checkTorqueLimit()) {
			m_comms->sendStatus(STATUS_PREFIX_ERROR,"FEED_MODE: Torque limit! Operation stopped.");
			finalizeAndResetActiveDispenseOperation(false);
			m_feedState = FEED_STANDBY;
			m_feedingDone = true;
			return;
		}

		if (!isMoving() && !m_feedingDone) {
			finalizeAndResetActiveDispenseOperation(true);
			m_feedState = FEED_INJECTION_COMPLETED;
			if (m_activeFeedCommand) {
				char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
				std::snprintf(doneMsg, sizeof(doneMsg), "%s complete.", m_activeFeedCommand);
				m_comms->sendStatus(STATUS_PREFIX_DONE, doneMsg);
				m_activeFeedCommand = nullptr;
			}
			m_feedingDone = true;
			m_feedState = FEED_STANDBY;
		}

		if (m_feedState == FEED_INJECT_STARTING && isMoving()) {
			m_feedState = FEED_INJECT_ACTIVE;
			m_active_op_segment_initial_axis_steps = m_motorA->PositionRefCommanded();
			} else if (m_feedState == FEED_INJECT_RESUMING && isMoving()) {
			m_feedState = FEED_INJECT_ACTIVE;
			m_active_op_segment_initial_axis_steps = m_motorA->PositionRefCommanded();
		}

		if (m_feedState == FEED_INJECT_PAUSED && !m_feedingDone && !isMoving()) {
			long current_pos = m_motorA->PositionRefCommanded();
			long steps_moved_this_segment = current_pos - m_active_op_segment_initial_axis_steps;
			if (m_active_op_steps_per_ml > 0.0001f) {
				float segment_dispensed_ml = std::abs(steps_moved_this_segment) / m_active_op_steps_per_ml;
				m_active_op_total_dispensed_ml += segment_dispensed_ml;
				long total_steps_dispensed = (long)(m_active_op_total_dispensed_ml * m_active_op_steps_per_ml);
				m_active_op_remaining_steps = m_active_op_total_target_steps - total_steps_dispensed;
				if (m_active_op_remaining_steps < 0) m_active_op_remaining_steps = 0;
			}
			m_feedingDone = true;
			m_comms->sendStatus(STATUS_PREFIX_INFO, "Feed Op: Operation Paused. Waiting for Resume/Cancel.");
		}
	}

	// --- Jog State Logic ---
	if (!m_jogDone) {
		if (checkTorqueLimit()) {
			abortMove();
			m_comms->sendStatus(STATUS_PREFIX_INFO, "JOG: Torque limit. Move stopped.");
			m_jogDone = true;
			if (m_activeJogCommand) m_activeJogCommand = nullptr;
			} else if (!isMoving()) {
			m_jogDone = true;
			if (m_activeJogCommand) {
				char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
				std::snprintf(doneMsg, sizeof(doneMsg), "%s complete.", m_activeJogCommand);
				m_comms->sendStatus(STATUS_PREFIX_DONE, doneMsg);
				m_activeJogCommand = nullptr;
			}
		}
	}
}


// --- Command Handling ---
void Injector::handleCommand(UserCommand cmd, const char* args) {
	if (!m_isEnabled && cmd != CMD_SET_INJECTOR_TORQUE_OFFSET) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Injector command ignored: Motors are disabled.");
		return;
	}
	if ((m_homingState != HOMING_NONE || !m_feedingDone || !m_jogDone) &&
	(cmd == CMD_JOG_MOVE || cmd == CMD_MACHINE_HOME_MOVE || cmd == CMD_CARTRIDGE_HOME_MOVE || cmd == CMD_INJECT_MOVE)) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Injector command ignored: Another operation is in progress.");
		return;
	}

	switch(cmd) {
		case CMD_JOG_MOVE:                  handleJogMove(args); break;
		case CMD_MACHINE_HOME_MOVE:         handleMachineHome(); break;
		case CMD_CARTRIDGE_HOME_MOVE:       handleCartridgeHome(); break;
		case CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(); break;
		case CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(args); break;
		case CMD_INJECT_MOVE:               handleInjectMove(args); break;
		case CMD_PAUSE_INJECTION:           handlePauseOperation(); break;
		case CMD_RESUME_INJECTION:          handleResumeOperation(); break;
		case CMD_CANCEL_INJECTION:          handleCancelOperation(); break;
		case CMD_SET_INJECTOR_TORQUE_OFFSET: handleSetTorqueOffset(args); break;
		default:
		break;
	}
}


// --- Public Methods ---
void Injector::enable() {
	m_motorA->EnableRequest(true);
	m_motorB->EnableRequest(true);
	m_isEnabled = true;
	m_comms->sendStatus(STATUS_PREFIX_INFO, "Injector motors enabled.");
}

void Injector::disable() {
	m_motorA->EnableRequest(false);
	m_motorB->EnableRequest(false);
	m_isEnabled = false;
	m_comms->sendStatus(STATUS_PREFIX_INFO, "Injector motors disabled.");
}

void Injector::abortMove() {
	m_motorA->MoveStopDecel();
	m_motorB->MoveStopDecel();
	Delay_ms(POST_ABORT_DELAY_MS);
}

void Injector::resetState() {
	m_homingState = HOMING_NONE;
	m_homingPhase = HOMING_PHASE_IDLE;
	m_feedState = FEED_STANDBY;
	m_jogDone = true;
	fullyResetActiveDispenseOperation();
}


// --- Private Command Handler Implementations ---
void Injector::handleJogMove(const char* args) {
	float dist_mm1 = 0, dist_mm2 = 0, vel_mms = 0, accel_mms2 = 0;
	int torque_percent = 0;

	int parsed_count = std::sscanf(args, "%f %f %f %f %d", &dist_mm1, &dist_mm2, &vel_mms, &accel_mms2, &torque_percent);

	if (parsed_count == 5) {
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = JOG_DEFAULT_TORQUE_PERCENT;
		if (vel_mms <= 0) vel_mms = JOG_DEFAULT_VEL_MMS;
		if (accel_mms2 <= 0) accel_mms2 = JOG_DEFAULT_ACCEL_MMSS;

		long steps1 = (long)(dist_mm1 * STEPS_PER_MM_INJECTOR);
		long steps2 = (long)(dist_mm2 * STEPS_PER_MM_INJECTOR);
		int velocity_sps = (int)(vel_mms * STEPS_PER_MM_INJECTOR);
		int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_INJECTOR);
		
		m_activeJogCommand = CMD_STR_JOG_MOVE;
		m_jogDone = false;
		move(steps1, steps2, torque_percent, velocity_sps, accel_sps2_val);
		} else {
		char errorMsg[STATUS_MESSAGE_BUFFER_SIZE];
		std::snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
	}
}

void Injector::handleMachineHome() {
	long homing_stroke_steps = (long)(HOMING_STROKE_MM * STEPS_PER_MM_INJECTOR);
	int homing_rapid_sps = (int)(HOMING_RAPID_VEL_MMS * STEPS_PER_MM_INJECTOR);
	int homing_accel_sps2 = (int)(HOMING_ACCEL_MMSS * STEPS_PER_MM_INJECTOR);
	
	m_homingState = HOMING_MACHINE;
	m_homingPhase = HOMING_PHASE_STARTING_MOVE;
	m_homingStartTime = Milliseconds();

	m_comms->sendStatus(STATUS_PREFIX_START, "MACHINE_HOME_MOVE initiated.");
	move(-homing_stroke_steps, -homing_stroke_steps, (int)HOMING_TORQUE_PERCENT, homing_rapid_sps, homing_accel_sps2);
}

void Injector::handleCartridgeHome() {
	long homing_stroke_steps = (long)(HOMING_STROKE_MM * STEPS_PER_MM_INJECTOR);
	int homing_rapid_sps = (int)(HOMING_RAPID_VEL_MMS * STEPS_PER_MM_INJECTOR);
	int homing_accel_sps2 = (int)(HOMING_ACCEL_MMSS * STEPS_PER_MM_INJECTOR);
	
	m_homingState = HOMING_CARTRIDGE;
	m_homingPhase = HOMING_PHASE_STARTING_MOVE;
	m_homingStartTime = Milliseconds();

	m_comms->sendStatus(STATUS_PREFIX_START, "CARTRIDGE_HOME_MOVE initiated.");
	move(homing_stroke_steps, homing_stroke_steps, (int)HOMING_TORQUE_PERCENT, homing_rapid_sps, homing_accel_sps2);
}

void Injector::handleMoveToCartridgeHome() {
	if (!m_homingCartridgeDone) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Cartridge not homed.");
		return;
	}
	
	fullyResetActiveDispenseOperation();
	m_feedState = FEED_MOVING_TO_HOME;
	m_feedingDone = false;
	m_activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_HOME;
	
	long current_pos = m_motorA->PositionRefCommanded();
	long steps_to_move = m_cartridgeHomeReferenceSteps - current_pos;

	move(steps_to_move, steps_to_move, m_feedDefaultTorquePercent, m_feedDefaultVelocitySPS, m_feedDefaultAccelSPS2);
}

void Injector::handleMoveToCartridgeRetract(const char* args) {
	if (!m_homingCartridgeDone) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Cartridge not homed.");
		return;
	}
	
	float offset_mm = 0.0f;
	if (std::sscanf(args, "%f", &offset_mm) != 1 || offset_mm < 0) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT.");
		return;
	}
	
	fullyResetActiveDispenseOperation();
	m_feedState = FEED_MOVING_TO_RETRACT;
	m_feedingDone = false;
	m_activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_RETRACT;

	long offset_steps = (long)(offset_mm * STEPS_PER_MM_INJECTOR);
	long target_pos = m_cartridgeHomeReferenceSteps - offset_steps; // Assuming retract is moving away from cartridge
	long current_pos = m_motorA->PositionRefCommanded();
	long steps_to_move = target_pos - current_pos;

	move(steps_to_move, steps_to_move, m_feedDefaultTorquePercent, m_feedDefaultVelocitySPS, m_feedDefaultAccelSPS2);
}

void Injector::handleInjectMove(const char* args) {
	float volume_ml, speed_ml_s, accel_sps2, steps_per_ml;
	int torque_percent;

	if (std::sscanf(args, "%f %f %f %f %d", &volume_ml, &speed_ml_s, &accel_sps2, &steps_per_ml, &torque_percent) == 5) {
		if (steps_per_ml <= 0.0001f) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Steps/ml must be positive."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = m_feedDefaultTorquePercent;
		if (volume_ml <= 0) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Inject speed must be positive."); return; }
		if (accel_sps2 <= 0) accel_sps2 = (float)m_feedDefaultAccelSPS2;

		fullyResetActiveDispenseOperation();
		m_feedState = FEED_INJECT_STARTING;
		m_feedingDone = false;
		m_active_dispense_INJECTION_ongoing = true;
		m_active_op_target_ml = volume_ml;
		m_active_op_steps_per_ml = steps_per_ml;
		m_active_op_total_target_steps = (long)(volume_ml * steps_per_ml);
		m_active_op_remaining_steps = m_active_op_total_target_steps;
		m_active_op_initial_axis_steps = m_motorA->PositionRefCommanded();
		m_active_op_velocity_sps = (int)(speed_ml_s * steps_per_ml);
		m_active_op_accel_sps2 = (int)accel_sps2;
		m_active_op_torque_percent = torque_percent;
		m_activeFeedCommand = CMD_STR_INJECT_MOVE;
		
		m_comms->sendStatus(STATUS_PREFIX_START, "INJECT_MOVE initiated.");
		move(m_active_op_remaining_steps, m_active_op_remaining_steps, m_active_op_torque_percent, m_active_op_velocity_sps, m_active_op_accel_sps2);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid INJECT_MOVE format. Expected 5 params.");
	}
}

void Injector::handlePauseOperation() {
	if (!m_active_dispense_INJECTION_ongoing || m_feedState != FEED_INJECT_ACTIVE) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "PAUSE ignored: No active injection to pause.");
		return;
	}
	abortMove();
	m_feedState = FEED_INJECT_PAUSED;
	m_comms->sendStatus(STATUS_PREFIX_DONE, "PAUSE_INJECTION complete.");
}

void Injector::handleResumeOperation() {
	if (!m_active_dispense_INJECTION_ongoing || m_feedState != FEED_INJECT_PAUSED) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: No operation was paused.");
		return;
	}
	if (m_active_op_remaining_steps <= 0) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: No remaining volume to dispense.");
		fullyResetActiveDispenseOperation();
		m_feedState = FEED_STANDBY;
		return;
	}
	m_active_op_segment_initial_axis_steps = m_motorA->PositionRefCommanded();
	m_feedingDone = false;
	m_feedState = FEED_INJECT_RESUMING;
	move(m_active_op_remaining_steps, m_active_op_remaining_steps, m_active_op_torque_percent, m_active_op_velocity_sps, m_active_op_accel_sps2);
	m_comms->sendStatus(STATUS_PREFIX_DONE, "RESUME_INJECTION complete.");
}

void Injector::handleCancelOperation() {
	if (!m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "CANCEL ignored: No active operation to cancel.");
		return;
	}
	abortMove();
	finalizeAndResetActiveDispenseOperation(false);
	m_feedState = FEED_INJECTION_CANCELLED;
	m_feedingDone = true;
	m_comms->sendStatus(STATUS_PREFIX_DONE, "CANCEL_INJECTION complete.");
}

void Injector::handleSetTorqueOffset(const char* args) {
	m_torqueOffset = std::atof(args);
	char response[STATUS_MESSAGE_BUFFER_SIZE];
	std::snprintf(response, sizeof(response), "Injector torque offset set to %.2f", m_torqueOffset);
	m_comms->sendStatus(STATUS_PREFIX_DONE, response);
}


// --- Private Helper Methods ---
void Injector::move(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel) {
	m_torqueLimit = (float)torque_limit;
	m_motorA->VelMax(velocity);
	m_motorB->VelMax(velocity);
	m_motorA->AccelMax(accel);
	m_motorB->AccelMax(accel);
	if (stepsM0 != 0) m_motorA->Move(stepsM0);
	if (stepsM1 != 0) m_motorB->Move(stepsM1);
}

bool Injector::isMoving() {
	if (!m_isEnabled) return false;
	bool m0_moving = m_motorA->StatusReg().bit.StepsActive;
	bool m1_moving = m_motorB->StatusReg().bit.StepsActive;
	return (m0_moving || m1_moving);
}

float Injector::getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead) {
	float currentRawTorque = motor->HlfbPercent();
	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return TORQUE_SENTINEL_INVALID_VALUE;
	}
	if (*firstRead) {
		*smoothedValue = currentRawTorque;
		*firstRead = false;
		} else {
		*smoothedValue = EWMA_ALPHA_TORQUE * currentRawTorque + (1.0f - EWMA_ALPHA_TORQUE) * (*smoothedValue);
	}
	return *smoothedValue + m_torqueOffset;
}

bool Injector::checkTorqueLimit() {
	if (isMoving()) {
		float torque0 = getSmoothedTorqueEWMA(m_motorA, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
		float torque1 = getSmoothedTorqueEWMA(m_motorB, &m_smoothedTorqueValue1, &m_firstTorqueReading1);

		bool m0_over_limit = (torque0 != TORQUE_SENTINEL_INVALID_VALUE && std::abs(torque0) > m_torqueLimit);
		bool m1_over_limit = (torque1 != TORQUE_SENTINEL_INVALID_VALUE && std::abs(torque1) > m_torqueLimit);

		if (m0_over_limit || m1_over_limit) {
			abortMove();
			char torque_msg[STATUS_MESSAGE_BUFFER_SIZE];
			std::snprintf(torque_msg, sizeof(torque_msg), "TORQUE LIMIT REACHED (%.1f%%)", m_torqueLimit);
			m_comms->sendStatus(STATUS_PREFIX_INFO, torque_msg);
			return true;
		}
	}
	return false;
}

void Injector::finalizeAndResetActiveDispenseOperation(bool success) {
	if (m_active_dispense_INJECTION_ongoing) {
		if (m_active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = m_motorA->PositionRefCommanded() - m_active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)std::abs(steps_moved_this_segment) / m_active_op_steps_per_ml;
			m_active_op_total_dispensed_ml += segment_dispensed_ml;
		}
		if (success) {
			m_last_completed_dispense_ml = m_active_op_total_dispensed_ml;
		}
	}
	fullyResetActiveDispenseOperation();
}

void Injector::fullyResetActiveDispenseOperation() {
	m_active_dispense_INJECTION_ongoing = false;
	m_active_op_target_ml = 0.0f;
	m_active_op_total_dispensed_ml = 0.0f;
	m_last_completed_dispense_ml = 0.0f;
	m_active_op_total_target_steps = 0;
	m_active_op_remaining_steps = 0;
	m_active_op_segment_initial_axis_steps = 0;
	m_active_op_initial_axis_steps = 0;
	m_active_op_steps_per_ml = 0.0f;
	m_activeFeedCommand = nullptr;
}

const char* Injector::getTelemetryString() {
	float displayTorque0 = getSmoothedTorqueEWMA(m_motorA, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
	float displayTorque1 = getSmoothedTorqueEWMA(m_motorB, &m_smoothedTorqueValue1, &m_firstTorqueReading1);
	
	long current_pos_steps_m0 = m_motorA->PositionRefCommanded();
	float machine_pos_mm = (float)(current_pos_steps_m0 - m_machineHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;
	float cartridge_pos_mm = (float)(current_pos_steps_m0 - m_cartridgeHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;

	std::snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"inj_t0:%.1f,inj_t1:%.1f,"
	"inj_h_mach:%d,inj_h_cart:%d,"
	"inj_mach_mm:%.2f,inj_cart_mm:%.2f,"
	"inj_disp_ml:%.2f,inj_tgt_ml:%.2f",
	displayTorque0, displayTorque1,
	(int)m_homingMachineDone, (int)m_homingCartridgeDone,
	machine_pos_mm, cartridge_pos_mm,
	m_last_completed_dispense_ml, m_active_op_target_ml
	);
	return m_telemetryBuffer;
}
