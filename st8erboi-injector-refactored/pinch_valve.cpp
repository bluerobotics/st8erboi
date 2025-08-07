#include "pinch_valve.h"
#include <math.h>

PinchValve::PinchValve(const char* name, MotorDriver* motor, InjectorComms* comms) {
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
	m_motor->VelMax(25000);
	m_motor->AccelMax(100000);
	m_motor->EnableRequest(true);
}

void PinchValve::update() {
	if (m_state == VALVE_IDLE) {
		return;
	}

	switch (m_state) {
		case VALVE_HOMING:
		if (Milliseconds() - m_homingStartTime > MAX_HOMING_DURATION_MS) {
			m_motor->MoveStopAbrupt();
			m_state = VALVE_IDLE;
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve homing timeout.");
			} else if (getSmoothedTorque() > m_torqueLimit || !m_motor->StatusReg().bit.StepsActive) {
			m_motor->MoveStopAbrupt();
			m_motor->PositionRefSet(0);
			m_isHomed = true;
			m_state = VALVE_IDLE;
			char msg[64];
			snprintf(msg, sizeof(msg), "%s homing complete.", m_name);
			m_comms->sendStatus(STATUS_PREFIX_DONE, msg);
		}
		break;

		case VALVE_MOVING:
		if (getSmoothedTorque() > m_torqueLimit) {
			m_motor->MoveStopAbrupt();
			m_state = VALVE_IDLE;
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve move stopped on torque limit.");
			} else if (!m_motor->StatusReg().bit.StepsActive) {
			m_state = VALVE_IDLE;
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

void PinchValve::home() {
	if (m_state != VALVE_IDLE) {
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

	move( (long)(stroke_mm * STEPS_PER_MM_PINCH),
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
	move(target_steps - current_steps, 10000, 50000, 30);
}

void PinchValve::close() {
	if (!m_isHomed) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Cannot close pinch valve, must be homed first.");
		return;
	}
	// Assuming 'closed' is position 0
	long current_steps = m_motor->PositionRefCommanded();
	move(0 - current_steps, 10000, 50000, 30);
}

void PinchValve::jog(float dist_mm, float vel_mms, float accel_mms2, int torque_percent) {
	if (m_state != VALVE_IDLE) {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Pinch valve is busy.");
		return;
	}
	long steps = (long)(dist_mm * STEPS_PER_MM_PINCH);
	int vel_sps = (int)(vel_mms * STEPS_PER_MM_PINCH);
	int accel_sps2_val = (int)(accel_mms2 * STEPS_PER_MM_PINCH);
	move(steps, vel_sps, accel_sps2_val, torque_percent);
}

void PinchValve::move(long steps, int velocity_sps, int accel_sps2, int torque_percent) {
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
