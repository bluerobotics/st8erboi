#include "pinch_valve_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
}

void PinchValve::setup() {
	m_motor->HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	m_motor->HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	m_motor->VelMax(MOTOR_DEFAULT_VEL_MAX_SPS);
	m_motor->AccelMax(MOTOR_DEFAULT_ACCEL_MAX_SPS2);
	m_motor->EnableRequest(true);
}

void PinchValve::update() {
	if (m_state == VALVE_IDLE || m_state == VALVE_OPEN || m_state == VALVE_CLOSED || m_state == VALVE_ERROR) {
		return;
	}

	switch (m_state) {
		case VALVE_HOMING:
		if (Milliseconds() - m_homingStartTime > MAX_HOMING_DURATION_MS) {
			abort();
			m_state = VALVE_ERROR;
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve homing timeout.");
			} else if (getSmoothedTorque() > m_torqueLimit || !m_motor->StatusReg().bit.StepsActive) {
			m_motor->MoveStopDecel();
			m_motor->PositionRefSet(0);
			m_isHomed = true;
			m_state = VALVE_CLOSED;
			char msg[64];
			snprintf(msg, sizeof(msg), "%s homing complete.", m_name);
			m_comms->sendStatus(STATUS_PREFIX_DONE, msg);
		}
		break;

		case VALVE_MOVING:
		if (getSmoothedTorque() > m_torqueLimit) {
			abort();
			m_state = VALVE_ERROR;
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve move stopped on torque limit.");
			} else if (!m_motor->StatusReg().bit.StepsActive) {
			m_state = VALVE_IDLE; // Or could be OPEN/CLOSED depending on target
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

void PinchValve::handleCommand(UserCommand cmd, const char* args) {
	// This function acts as a router for all pinch valve related commands.
	// It checks which specific command was sent and calls the appropriate handler.
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
		// This case should ideally not be reached if called from Fillhead,
		// but it's good practice to have a default.
		break;
	}
}


void PinchValve::home() {
	if (m_state != VALVE_IDLE && m_state != VALVE_OPEN && m_state != VALVE_CLOSED) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve is busy.");
		return;
	}
	m_isHomed = false;
	m_state = VALVE_HOMING;
	m_homingStartTime = Milliseconds();
	
	const float stroke_mm = 25.0f;
	const float vel_mms = 5.0f;
	const float accel_mms2 = 100.0f;
	const int torque_percent = 20;

	moveSteps( (long)(stroke_mm * STEPS_PER_MM_PINCH),
	(int)(vel_mms * STEPS_PER_MM_PINCH),
	(int)(accel_mms2 * STEPS_PER_MM_PINCH),
	torque_percent);
	
	char msg[64];
	snprintf(msg, sizeof(msg), "%s homing started.", m_name);
	m_comms->sendStatus(STATUS_PREFIX_INFO, msg);
}

void PinchValve::open() {
	if (!m_isHomed) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Cannot open pinch valve, must be homed first.");
		return;
	}
	// Assuming 'open' is moving to a negative position
	long target_steps = (long)(-10 * STEPS_PER_MM_PINCH); // 10mm open
	long current_steps = m_motor->PositionRefCommanded();
	moveSteps(target_steps - current_steps, 10000, 50000, 30);
}

void PinchValve::close() {
	if (!m_isHomed) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Cannot close pinch valve, must be homed first.");
		return;
	}
	// Assuming 'closed' is position 0
	long current_steps = m_motor->PositionRefCommanded();
	moveSteps(0 - current_steps, 10000, 50000, 30);
}

void PinchValve::jog(const char* args) {
	if (m_state != VALVE_IDLE && m_state != VALVE_OPEN && m_state != VALVE_CLOSED) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve is busy.");
		return;
	}
	if (!args) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid jog command arguments.");
		return;
	}
	float dist_mm = atof(args);
	long steps = (long)(dist_mm * STEPS_PER_MM_PINCH);
	int vel_sps = (int)(PINCH_JOG_DEFAULT_VEL_MMS * STEPS_PER_MM_PINCH);
	int accel_sps2_val = (int)(PINCH_JOG_DEFAULT_ACCEL_MMSS * STEPS_PER_MM_PINCH);
	moveSteps(steps, vel_sps, accel_sps2_val, JOG_DEFAULT_TORQUE_PERCENT);
}

void PinchValve::moveSteps(long steps, int velocity_sps, int accel_sps2, int torque_percent) {
	if (m_motor->StatusReg().bit.Enabled) {
		m_state = VALVE_MOVING;
		m_torqueLimit = (float)torque_percent;
		m_motor->VelMax(velocity_sps);
		m_motor->AccelMax(accel_sps2);
		m_motor->Move(steps);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve motor is not enabled.");
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

float PinchValve::getSmoothedTorque() {
	float currentRawTorque = m_motor->HlfbPercent();
	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return 0.0f; // Return a safe value if torque is invalid
	}
	if (m_firstTorqueReading) {
		m_smoothedTorque = currentRawTorque;
		m_firstTorqueReading = false;
		} else {
		m_smoothedTorque = EWMA_ALPHA_TORQUE * currentRawTorque + (1.0f - EWMA_ALPHA_TORQUE) * m_smoothedTorque;
	}
	return m_smoothedTorque;
}

const char* PinchValve::getTelemetryString() {
	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"%s_pos:%.2f,%s_homed:%d",
	m_name, (float)m_motor->PositionRefCommanded() / STEPS_PER_MM_PINCH,
	m_name, (int)m_isHomed
	);
	return m_telemetryBuffer;
}
