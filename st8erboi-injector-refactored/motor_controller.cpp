#include "motor_controller.h"
#include <math.h>

MotorController::MotorController(InjectorComms* comms) :
m_injectionValve("inj_valve", &MOTOR_INJECTION_VALVE, comms),
m_vacuumValve("vac_valve", &MOTOR_VACUUM_VALVE, comms)
{
	m_comms = comms;
	m_homingState = HOMING_NONE;
	m_homingPhase = HOMING_PHASE_IDLE;
	m_feedState = FEED_STANDBY;
	m_homingMachineDone = false;
	m_homingCartridgeDone = false;
	m_feedingDone = true;
	m_jogDone = true;
	m_homingStartTime = 0;
	m_motorsAreEnabled = false;
	
	// Initialize from config file
	m_injectorMotorsTorqueLimit = DEFAULT_INJECTOR_TORQUE_LIMIT;
	m_injectorMotorsTorqueOffset = DEFAULT_INJECTOR_TORQUE_OFFSET;
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

void MotorController::setup() {
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);

	MOTOR_INJECTOR_A.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MOTOR_INJECTOR_A.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MOTOR_INJECTOR_A.VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
	MOTOR_INJECTOR_A.AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);

	MOTOR_INJECTOR_B.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	MOTOR_INJECTOR_B.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	MOTOR_INJECTOR_B.VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
	MOTOR_INJECTOR_B.AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);

	m_injectionValve.setup();
	m_vacuumValve.setup();

	enableMotors("Initial motor enable on setup");
}

void MotorController::updateState() {
	// Update pinch valves first
	m_injectionValve.update();
	m_vacuumValve.update();

	// --- Homing State Machine ---
	if (m_homingState != HOMING_NONE) {
		if (Milliseconds() - m_homingStartTime > MAX_HOMING_DURATION_MS) {
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Timeout. Aborting.");
			abortMove();
			m_homingPhase = HOMING_PHASE_ERROR;
		}

		if (m_homingPhase == HOMING_PHASE_COMPLETE || m_homingPhase == HOMING_PHASE_ERROR) {
			if (m_homingPhase == HOMING_PHASE_COMPLETE) {
				char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
				const char* commandStr = "UNKNOWN_HOME";
				if (m_homingState == HOMING_MACHINE) {
					commandStr = CMD_STR_MACHINE_HOME_MOVE;
					m_homingMachineDone = true;
					} else if (m_homingState == HOMING_CARTRIDGE) {
					commandStr = CMD_STR_CARTRIDGE_HOME_MOVE;
					m_homingCartridgeDone = true;
				}
				snprintf(doneMsg, sizeof(doneMsg), "%s complete.", commandStr);
				m_comms->sendStatus(STATUS_PREFIX_DONE, doneMsg);
				} else {
				m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing sequence ended with error.");
			}
			m_homingState = HOMING_NONE;
			m_homingPhase = HOMING_PHASE_IDLE;
			return;
		}

		switch (m_homingState) {
			case HOMING_MACHINE:
			case HOMING_CARTRIDGE: {
				int direction = (m_homingState == HOMING_MACHINE) ? -1 : 1;
				switch (m_homingPhase) {
					case HOMING_PHASE_STARTING_MOVE:
					if (Milliseconds() - m_homingStartTime > 50) {
						if (checkMoving()) {
							m_comms->sendStatus(STATUS_PREFIX_START, "Homing: Move started. Entering RAPID_MOVE phase.");
							m_homingPhase = HOMING_PHASE_RAPID_MOVE;
							} else {
							m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Motor failed to start moving.");
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
							moveInjectorMotors(back_off_s, back_off_s, (int)m_injectorMotorsTorqueLimit, (int)(HOMING_BACK_OFF_VEL_MMS * STEPS_PER_MM_INJECTOR), (int)(HOMING_BACK_OFF_ACCEL_MMSS * STEPS_PER_MM_INJECTOR));
							} else { // TOUCH_OFF
							m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Torque (TOUCH_OFF). Zeroing & Retracting.");
							if (m_homingState == HOMING_MACHINE) {
								m_machineHomeReferenceSteps = MOTOR_INJECTOR_A.PositionRefCommanded();
								} else {
								m_cartridgeHomeReferenceSteps = MOTOR_INJECTOR_A.PositionRefCommanded();
							}
							m_homingPhase = HOMING_PHASE_RETRACT;
							long retract_s = -direction * (long)(HOMING_POST_TOUCH_RETRACT_MM * STEPS_PER_MM_INJECTOR);
							moveInjectorMotors(retract_s, retract_s, (int)m_injectorMotorsTorqueLimit, (int)(HOMING_RETRACT_VEL_MMS * STEPS_PER_MM_INJECTOR), (int)(HOMING_RETRACT_ACCEL_MMSS * STEPS_PER_MM_INJECTOR));
						}
						} else if (!checkMoving()) {
						m_comms->sendStatus(STATUS_PREFIX_ERROR, "Homing Error: Move completed without hitting torque limit.");
						m_homingPhase = HOMING_PHASE_ERROR;
					}
					break;
					case HOMING_PHASE_BACK_OFF:
					if (!checkMoving()) {
						m_comms->sendStatus(STATUS_PREFIX_INFO, "Homing: Back-off done. Touching off.");
						m_homingPhase = HOMING_PHASE_TOUCH_OFF;
						long touch_off_s = direction * (m_homingDefaultBackoffSteps * 2);
						moveInjectorMotors(touch_off_s, touch_off_s, (int)m_injectorMotorsTorqueLimit, (int)(HOMING_TOUCH_VEL_MMS * STEPS_PER_MM_INJECTOR), (int)(HOMING_TOUCH_OFF_ACCEL_MMSS * STEPS_PER_MM_INJECTOR));
					}
					break;
					case HOMING_PHASE_RETRACT:
					if (!checkMoving()) {
						m_homingPhase = HOMING_PHASE_COMPLETE;
					}
					break;
					default:
					break;
				}
				break;
			}
			default:
			break;
		}
	}

	// --- Feed State Machine ---
	if (m_active_dispense_INJECTION_ongoing) {
		switch (m_feedState) {
			case FEED_INJECT_ACTIVE:
			case FEED_MOVING_TO_HOME:
			case FEED_MOVING_TO_RETRACT:
			case FEED_INJECT_RESUMING:
			if (checkTorqueLimit()) {
				m_comms->sendStatus(STATUS_PREFIX_ERROR,"FEED_MODE: Torque limit! Operation stopped.");
				finalizeAndResetActiveDispenseOperation(false);
				m_feedState = FEED_STANDBY;
				m_feedingDone = true;
			}
			if (!checkMoving()) {
				if (!m_feedingDone) {
					finalizeAndResetActiveDispenseOperation(true);
					m_feedState = FEED_INJECTION_COMPLETED;
					if (m_activeFeedCommand) {
						char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
						snprintf(doneMsg, sizeof(doneMsg), "%s complete.", m_activeFeedCommand);
						m_comms->sendStatus(STATUS_PREFIX_DONE, doneMsg);
					}
					m_feedingDone = true;
					m_feedState = FEED_STANDBY;
				}
			}
			break;

			case FEED_INJECT_STARTING:
			if (checkMoving()) {
				m_feedState = FEED_INJECT_ACTIVE;
				m_active_op_segment_initial_axis_steps = MOTOR_INJECTOR_A.PositionRefCommanded();
			}
			break;
			case FEED_INJECT_PAUSED:
			if (!m_feedingDone && !checkMoving()) {
				long current_pos = MOTOR_INJECTOR_A.PositionRefCommanded();
				long steps_moved_this_segment = current_pos - m_active_op_segment_initial_axis_steps;
				if (m_active_op_steps_per_ml > 0.0001f) {
					float segment_dispensed_ml = fabs(steps_moved_this_segment) / m_active_op_steps_per_ml;
					m_active_op_total_dispensed_ml += segment_dispensed_ml;
					long total_steps_dispensed = (long)(m_active_op_total_dispensed_ml * m_active_op_steps_per_ml);
					m_active_op_remaining_steps = m_active_op_total_target_steps - total_steps_dispensed;
					if (m_active_op_remaining_steps < 0) m_active_op_remaining_steps = 0;
				}
				m_feedingDone = true;
			}
			break;
			default:
			break;
		}
	}

	// --- Jog State Logic ---
	if (!m_jogDone) {
		if (checkTorqueLimit()) {
			abortMove();
			m_comms->sendStatus(STATUS_PREFIX_INFO, "JOG: Torque limit, returning to STANDBY.");
			m_jogDone = true;
			if (m_activeJogCommand != nullptr) {
				m_activeJogCommand = nullptr;
			}
			} else if (!checkMoving()) {
			m_jogDone = true;
			if (m_activeJogCommand != nullptr) {
				char doneMsg[STATUS_MESSAGE_BUFFER_SIZE];
				snprintf(doneMsg, sizeof(doneMsg), "%s complete.", m_activeJogCommand);
				m_comms->sendStatus(STATUS_PREFIX_DONE, doneMsg);
				m_activeJogCommand = nullptr;
			}
		}
	}
}

const char* MotorController::getTelemetryString() {
	float displayTorque0 = getSmoothedTorqueEWMA(&MOTOR_INJECTOR_A, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
	float displayTorque1 = getSmoothedTorqueEWMA(&MOTOR_INJECTOR_B, &m_smoothedTorqueValue1, &m_firstTorqueReading1);
	
	long current_pos_steps_m0 = MOTOR_INJECTOR_A.PositionRefCommanded();
	float machine_pos_mm = (float)(current_pos_steps_m0 - m_machineHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;
	float cartridge_pos_mm = (float)(current_pos_steps_m0 - m_cartridgeHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;

	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"t0:%.1f,t1:%.1f,"
	"p0:%.2f,p1:%.2f,"
	"h_mach:%d,h_cart:%d,"
	"mach_mm:%.2f,cart_mm:%.2f,"
	"disp_ml:%.2f,tgt_ml:%.2f,"
	"%s,%s", // Pinch valve telemetry
	displayTorque0, displayTorque1,
	(float)current_pos_steps_m0 / STEPS_PER_MM_INJECTOR,
	(float)MOTOR_INJECTOR_B.PositionRefCommanded() / STEPS_PER_MM_INJECTOR,
	(int)m_homingMachineDone, (int)m_homingCartridgeDone,
	machine_pos_mm, cartridge_pos_mm,
	m_last_completed_dispense_ml, m_active_op_target_ml,
	m_injectionValve.getTelemetryString(),
	m_vacuumValve.getTelemetryString()
	);
	return m_telemetryBuffer;
}

void MotorController::handleCommand(UserCommand cmd, const char* args) {
	switch(cmd) {
		case CMD_JOG_MOVE:                  handleJogMove(args); break;
		case CMD_MACHINE_HOME_MOVE:         handleMachineHomeMove(); break;
		case CMD_CARTRIDGE_HOME_MOVE:       handleCartridgeHomeMove(); break;
		case CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(); break;
		case CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(args); break;
		case CMD_INJECT_MOVE:               handleInjectMove(args); break;
		case CMD_PAUSE_INJECTION:           handlePauseOperation(); break;
		case CMD_RESUME_INJECTION:          handleResumeOperation(); break;
		case CMD_CANCEL_INJECTION:          handleCancelOperation(); break;
		case CMD_SET_INJECTOR_TORQUE_OFFSET: handleSetTorqueOffset(args); break;

		// Delegate to Injection Valve
		case CMD_INJECTION_VALVE_HOME:      m_injectionValve.home(); break;
		case CMD_INJECTION_VALVE_OPEN:      m_injectionValve.open(); break;
		case CMD_INJECTION_VALVE_CLOSE:     m_injectionValve.close(); break;
		case CMD_INJECTION_VALVE_JOG: {
			float dist, vel, acc; int torq;
			sscanf(args, "%f %f %f %d", &dist, &vel, &acc, &torq);
			m_injectionValve.jog(dist, vel, acc, torq);
			break;
		}

		// Delegate to Vacuum Valve
		case CMD_VACUUM_VALVE_HOME:         m_vacuumValve.home(); break;
		case CMD_VACUUM_VALVE_OPEN:         m_vacuumValve.open(); break;
		case CMD_VACUUM_VALVE_CLOSE:        m_vacuumValve.close(); break;
		case CMD_VACUUM_VALVE_JOG: {
			float dist, vel, acc; int torq;
			sscanf(args, "%f %f %f %d", &dist, &vel, &acc, &torq);
			m_vacuumValve.jog(dist, vel, acc, torq);
			break;
		}

		default: break;
	}
}

void MotorController::enableMotors(const char* reason) {
	MOTOR_INJECTOR_A.EnableRequest(true);
	MOTOR_INJECTOR_B.EnableRequest(true);
	m_injectionValve.enable();
	m_vacuumValve.enable();
	m_motorsAreEnabled = true;
	m_comms->sendStatus(STATUS_PREFIX_INFO, reason);
}

void MotorController::disableMotors(const char* reason) {
	MOTOR_INJECTOR_A.EnableRequest(false);
	MOTOR_INJECTOR_B.EnableRequest(false);
	m_injectionValve.disable();
	m_vacuumValve.disable();
	m_motorsAreEnabled = false;
	m_comms->sendStatus(STATUS_PREFIX_INFO, reason);
}

bool MotorController::areMotorsEnabled() const {
	return m_motorsAreEnabled;
}

void MotorController::abortMove() {
	MOTOR_INJECTOR_A.MoveStopAbrupt();
	MOTOR_INJECTOR_B.MoveStopAbrupt();
	Delay_ms(POST_ABORT_DELAY_MS);
}

void MotorController::resetState() {
	m_homingState = HOMING_NONE;
	m_homingPhase = HOMING_PHASE_IDLE;
	m_feedState = FEED_STANDBY;
	m_jogDone = true;
	fullyResetActiveDispenseOperation();
}

void MotorController::handleJogMove(const char* args) {
	if (m_homingState != HOMING_NONE || m_feedState != FEED_STANDBY || !m_jogDone || m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "JOG_MOVE ignored: Another operation is in progress.");
		return;
	}

	float dist_mm1 = 0, dist_mm2 = 0, vel_mms = 0, accel_mms2 = 0;
	int torque_percent = 0;

	int parsed_count = sscanf(args, "%f %f %f %f %d", &dist_mm1, &dist_mm2, &vel_mms, &accel_mms2, &torque_percent);

	if (parsed_count == 5) {
		if (!m_motorsAreEnabled) {
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "JOG_MOVE blocked: Motors are disabled.");
			return;
		}

		if (torque_percent <= 0 || torque_percent > 100) torque_percent = JOG_DEFAULT_TORQUE_PERCENT;
		if (vel_mms <= 0) vel_mms = JOG_DEFAULT_VEL_MMS;
		if (accel_mms2 <= 0) accel_mms2 = JOG_DEFAULT_ACCEL_MMSS;

		long steps1 = (long)(dist_mm1 * STEPS_PER_MM_INJECTOR);
		long steps2 = (long)(dist_mm2 * STEPS_PER_MM_INJECTOR);
		int velocity_sps = (int)(vel_mms * STEPS_PER_MM_INJECTOR);
		int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_INJECTOR);
		
		m_activeJogCommand = CMD_STR_JOG_MOVE;
		moveInjectorMotors(steps1, steps2, torque_percent, velocity_sps, accel_sps2_val);
		m_jogDone = false;
		} else {
		char errorMsg[STATUS_MESSAGE_BUFFER_SIZE];
		snprintf(errorMsg, sizeof(errorMsg), "Invalid JOG_MOVE format. Expected 5 params, got %d.", parsed_count);
		m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
	}
}

void MotorController::handleMachineHomeMove() {
	if (m_homingState != HOMING_NONE || m_feedState != FEED_STANDBY || !m_jogDone || m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "MACHINE_HOME_MOVE ignored: Another operation is in progress.");
		return;
	}
	if (!m_motorsAreEnabled) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "MACHINE_HOME_MOVE blocked: Motors disabled.");
		m_homingState = HOMING_MACHINE;
		m_homingPhase = HOMING_PHASE_ERROR;
		return;
	}

	long homing_actual_stroke_steps = (long)(HOMING_STROKE_MM * STEPS_PER_MM_INJECTOR);
	int homing_actual_rapid_sps = (int)(HOMING_RAPID_VEL_MMS * STEPS_PER_MM_INJECTOR);
	int homing_actual_accel_sps2 = (int)(HOMING_ACCEL_MMSS * STEPS_PER_MM_INJECTOR);
	
	m_homingState = HOMING_MACHINE;
	m_homingPhase = HOMING_PHASE_STARTING_MOVE;
	m_homingStartTime = Milliseconds();

	m_comms->sendStatus(STATUS_PREFIX_START, "MACHINE_HOME_MOVE initiated.");
	long initial_move_steps = -1 * homing_actual_stroke_steps;
	moveInjectorMotors(initial_move_steps, initial_move_steps, (int)HOMING_TORQUE_PERCENT, homing_actual_rapid_sps, homing_actual_accel_sps2);
}

void MotorController::handleCartridgeHomeMove() {
	if (m_homingState != HOMING_NONE || m_feedState != FEED_STANDBY || !m_jogDone || m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "CARTRIDGE_HOME_MOVE ignored: Another operation is in progress.");
		return;
	}
	if (!m_motorsAreEnabled) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "CARTRIDGE_HOME_MOVE blocked: Motors disabled.");
		m_homingState = HOMING_CARTRIDGE;
		m_homingPhase = HOMING_PHASE_ERROR;
		return;
	}

	long homing_actual_stroke_steps = (long)(HOMING_STROKE_MM * STEPS_PER_MM_INJECTOR);
	int homing_actual_rapid_sps = (int)(HOMING_RAPID_VEL_MMS * STEPS_PER_MM_INJECTOR);
	int homing_actual_accel_sps2 = (int)(HOMING_ACCEL_MMSS * STEPS_PER_MM_INJECTOR);
	
	m_homingState = HOMING_CARTRIDGE;
	m_homingPhase = HOMING_PHASE_STARTING_MOVE;
	m_homingStartTime = Milliseconds();

	m_comms->sendStatus(STATUS_PREFIX_START, "CARTRIDGE_HOME_MOVE initiated.");
	long initial_move_steps = 1 * homing_actual_stroke_steps;
	moveInjectorMotors(initial_move_steps, initial_move_steps, (int)HOMING_TORQUE_PERCENT, homing_actual_rapid_sps, homing_actual_accel_sps2);
}

void MotorController::handleMoveToCartridgeHome() {
	if (m_homingState != HOMING_NONE || m_feedState != FEED_STANDBY || !m_jogDone || m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Err: Busy. Cannot move to cart home now.");
		return;
	}
	if (!m_homingCartridgeDone) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Err: Cartridge not homed.");
		return;
	}
	if (!m_motorsAreEnabled) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Err: Motors disabled.");
		return;
	}
	
	fullyResetActiveDispenseOperation();
	m_feedState = FEED_MOVING_TO_HOME;
	m_feedingDone = false;
	m_activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_HOME;
	
	long current_axis_pos = MOTOR_INJECTOR_A.PositionRefCommanded();
	long steps_to_move_axis = m_cartridgeHomeReferenceSteps - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	m_feedDefaultTorquePercent, m_feedDefaultVelocitySPS, m_feedDefaultAccelSPS2);
}

void MotorController::handleMoveToCartridgeRetract(const char* args) {
	if (m_homingState != HOMING_NONE || m_feedState != FEED_STANDBY || !m_jogDone || m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Err: Busy. Cannot move to cart retract now.");
		return;
	}
	if (!m_homingCartridgeDone) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Err: Cartridge not homed."); return; }
	if (!m_motorsAreEnabled) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Err: Motors disabled."); return; }
	
	float offset_mm = 0.0f;
	if (sscanf(args, "%f", &offset_mm) != 1 || offset_mm < 0) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Err: Invalid offset for MOVE_TO_CARTRIDGE_RETRACT."); return;
	}
	
	fullyResetActiveDispenseOperation();
	m_feedState = FEED_MOVING_TO_RETRACT;
	m_feedingDone = false;
	m_activeFeedCommand = CMD_STR_MOVE_TO_CARTRIDGE_RETRACT;

	const float current_steps_per_mm = (float)PULSES_PER_REV / INJECTOR_PITCH_MM_PER_REV;
	long offset_steps = (long)(offset_mm * current_steps_per_mm);
	long target_absolute_axis_position = m_cartridgeHomeReferenceSteps + offset_steps;

	long current_axis_pos = MOTOR_INJECTOR_A.PositionRefCommanded();
	long steps_to_move_axis = target_absolute_axis_position - current_axis_pos;

	moveInjectorMotors((int)steps_to_move_axis, (int)steps_to_move_axis,
	m_feedDefaultTorquePercent, m_feedDefaultVelocitySPS, m_feedDefaultAccelSPS2);
}

void MotorController::handleInjectMove(const char* args) {
	if (m_homingState != HOMING_NONE || m_feedState != FEED_STANDBY || !m_jogDone || m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Operation already in progress or motors busy.");
		return;
	}

	float volume_ml, speed_ml_s, acceleration_sps2_param, steps_per_ml_val, torque_percent;
	if (sscanf(args, "%f %f %f %f %f", &volume_ml, &speed_ml_s, &acceleration_sps2_param, &steps_per_ml_val, &torque_percent) == 5) {
		
		if (!m_motorsAreEnabled) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "INJECT blocked: Motors disabled."); return; }
		if (steps_per_ml_val <= 0.0001f) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Steps/ml must be positive and non-zero."); return; }
		if (torque_percent <= 0 || torque_percent > 100) torque_percent = 50.0f;
		if (volume_ml <= 0) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Inject volume must be positive."); return; }
		if (speed_ml_s <= 0) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Inject speed must be positive."); return; }
		if (acceleration_sps2_param <= 0) { m_comms->sendStatus(STATUS_PREFIX_ERROR, "Error: Inject acceleration must be positive."); return; }

		fullyResetActiveDispenseOperation();
		m_last_completed_dispense_ml = 0.0f;

		m_feedState = FEED_INJECT_STARTING;
		m_feedingDone = false;
		m_active_dispense_INJECTION_ongoing = true;
		m_active_op_target_ml = volume_ml;
		m_active_op_steps_per_ml = steps_per_ml_val;
		m_active_op_total_target_steps = (long)(volume_ml * m_active_op_steps_per_ml);
		m_active_op_remaining_steps = m_active_op_total_target_steps;
		
		m_active_op_initial_axis_steps = MOTOR_INJECTOR_A.PositionRefCommanded();
		m_active_op_segment_initial_axis_steps = m_active_op_initial_axis_steps;
		
		m_active_op_total_dispensed_ml = 0.0f;

		m_active_op_velocity_sps = (int)(speed_ml_s * m_active_op_steps_per_ml);
		m_active_op_accel_sps2 = (int)acceleration_sps2_param;
		m_active_op_torque_percent = (int)torque_percent;
		if (m_active_op_velocity_sps <= 0) m_active_op_velocity_sps = INJECT_DEFAULT_VELOCITY_SPS;
		
		m_activeFeedCommand = CMD_STR_INJECT_MOVE;
		
		m_comms->sendStatus(STATUS_PREFIX_START, "INJECT_MOVE initiated.");
		moveInjectorMotors(m_active_op_remaining_steps, m_active_op_remaining_steps,
		m_active_op_torque_percent, m_active_op_velocity_sps, m_active_op_accel_sps2);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid INJECT_MOVE format. Expected 5 params.");
	}
}

void MotorController::handlePauseOperation() {
	if (!m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "PAUSE ignored: No active inject/purge operation.");
		return;
	}

	if (m_feedState == FEED_INJECT_ACTIVE) {
		MOTOR_INJECTOR_A.MoveStopDecel();
		MOTOR_INJECTOR_B.MoveStopDecel();
		m_feedState = FEED_INJECT_PAUSED;
		m_comms->sendStatus(STATUS_PREFIX_DONE, "PAUSE_INJECTION complete.");
		} else {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "PAUSE ignored: Not in an active inject/purge state.");
	}
}

void MotorController::handleResumeOperation() {
	if (!m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: No operation was ongoing or paused.");
		return;
	}
	if (m_feedState == FEED_INJECT_PAUSED) {
		if (m_active_op_remaining_steps <= 0) {
			m_comms->sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: No remaining volume/steps to dispense.");
			fullyResetActiveDispenseOperation();
			m_feedState = FEED_STANDBY;
			return;
		}
		m_active_op_segment_initial_axis_steps = MOTOR_INJECTOR_A.PositionRefCommanded();
		m_feedingDone = false;

		moveInjectorMotors(m_active_op_remaining_steps, m_active_op_remaining_steps,
		m_active_op_torque_percent, m_active_op_velocity_sps, m_active_op_accel_sps2);
		
		m_feedState = FEED_INJECT_RESUMING;
		
		m_comms->sendStatus(STATUS_PREFIX_DONE, "RESUME_INJECTION complete.");
		} else {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "RESUME ignored: Operation not paused.");
	}
}

void MotorController::handleCancelOperation() {
	if (!m_active_dispense_INJECTION_ongoing) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "CANCEL ignored: No active inject/purge operation to cancel.");
		return;
	}

	abortMove();

	if (m_active_op_steps_per_ml > 0.0001f && m_active_dispense_INJECTION_ongoing) {
		long steps_moved_on_axis = MOTOR_INJECTOR_A.PositionRefCommanded() - m_active_op_segment_initial_axis_steps;
		float segment_dispensed_ml = (float)fabs(steps_moved_on_axis) / m_active_op_steps_per_ml;
		m_active_op_total_dispensed_ml += segment_dispensed_ml;
	}
	
	m_last_completed_dispense_ml = 0.0f;
	fullyResetActiveDispenseOperation();
	
	m_feedState = FEED_INJECTION_CANCELLED;
	m_feedingDone = true;

	m_comms->sendStatus(STATUS_PREFIX_DONE, "CANCEL_INJECTION complete.");
}

void MotorController::handleSetTorqueOffset(const char* args) {
	float newOffset = atof(args);
	m_injectorMotorsTorqueOffset = newOffset;
	char response[STATUS_MESSAGE_BUFFER_SIZE];
	snprintf(response, sizeof(response), "Global torque offset set to %.2f", m_injectorMotorsTorqueOffset);
	m_comms->sendStatus(STATUS_PREFIX_DONE, response);
}

void MotorController::moveInjectorMotors(int stepsM0, int stepsM1, int torque_limit, int velocity, int accel) {
	if (!m_motorsAreEnabled) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "MOVE BLOCKED: Motors are disabled.");
		return;
	}
	m_injectorMotorsTorqueLimit = (float)torque_limit;
	MOTOR_INJECTOR_A.VelMax(velocity);
	MOTOR_INJECTOR_B.VelMax(velocity);
	MOTOR_INJECTOR_A.AccelMax(accel);
	MOTOR_INJECTOR_B.AccelMax(accel);
	if (stepsM0 != 0) MOTOR_INJECTOR_A.Move(stepsM0);
	if (stepsM1 != 0) MOTOR_INJECTOR_B.Move(stepsM1);
}

bool MotorController::checkMoving() {
	bool m0_moving = !MOTOR_INJECTOR_A.StepsComplete() && MOTOR_INJECTOR_A.StatusReg().bit.Enabled;
	bool m1_moving = !MOTOR_INJECTOR_B.StepsComplete() && MOTOR_INJECTOR_B.StatusReg().bit.Enabled;
	return m_motorsAreEnabled && (m0_moving || m1_moving);
}

float MotorController::getSmoothedTorqueEWMA(MotorDriver *motor, float *smoothedValue, bool *firstRead) {
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
	float adjusted = *smoothedValue + m_injectorMotorsTorqueOffset;
	return adjusted;
}

bool MotorController::checkTorqueLimit() {
	if (checkMoving()) {
		float torque0 = getSmoothedTorqueEWMA(&MOTOR_INJECTOR_A, &m_smoothedTorqueValue0, &m_firstTorqueReading0);
		float torque1 = getSmoothedTorqueEWMA(&MOTOR_INJECTOR_B, &m_smoothedTorqueValue1, &m_firstTorqueReading1);

		bool m0_over_limit = !MOTOR_INJECTOR_A.StepsComplete() && (torque0 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(torque0) > m_injectorMotorsTorqueLimit);
		bool m1_over_limit = !MOTOR_INJECTOR_B.StepsComplete() && (torque1 != TORQUE_SENTINEL_INVALID_VALUE && fabsf(torque1) > m_injectorMotorsTorqueLimit);

		if (m0_over_limit || m1_over_limit) {
			abortMove();
			char torque_msg[STATUS_MESSAGE_BUFFER_SIZE];
			snprintf(torque_msg, sizeof(torque_msg), "TORQUE LIMIT REACHED (%.1f%%)", m_injectorMotorsTorqueLimit);
			m_comms->sendStatus(STATUS_PREFIX_INFO, torque_msg);
			return true;
		}
	}
	return false;
}

void MotorController::finalizeAndResetActiveDispenseOperation(bool success) {
	if (m_active_dispense_INJECTION_ongoing) {
		if (m_active_op_steps_per_ml > 0.0001f) {
			long steps_moved_this_segment = MOTOR_INJECTOR_A.PositionRefCommanded() - m_active_op_segment_initial_axis_steps;
			float segment_dispensed_ml = (float)fabs(steps_moved_this_segment) / m_active_op_steps_per_ml;
			m_active_op_total_dispensed_ml += segment_dispensed_ml;
		}
		m_last_completed_dispense_ml = m_active_op_total_dispensed_ml;
	}
	m_active_dispense_INJECTION_ongoing = false;
	m_active_op_target_ml = 0.0f;
	m_active_op_remaining_steps = 0;
	m_activeFeedCommand = nullptr;
}

void MotorController::fullyResetActiveDispenseOperation() {
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
