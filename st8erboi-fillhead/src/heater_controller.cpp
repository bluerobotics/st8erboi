#include "heater_controller.h"
#include <stdio.h>
#include <stdlib.h>

HeaterController::HeaterController(CommsController* comms) {
	m_comms = comms;
	m_heaterState = HEATER_OFF;
	m_temperatureCelsius = 0.0f;
	m_smoothedTemperatureCelsius = 0.0f;
	m_firstTempReading = true;

	// Initialize PID values from config.h
	m_pid_setpoint = DEFAULT_HEATER_SETPOINT_C;
	m_pid_kp = DEFAULT_HEATER_KP;
	m_pid_ki = DEFAULT_HEATER_KI;
	m_pid_kd = DEFAULT_HEATER_KD;
	resetPID();
}

void HeaterController::setup() {
	PIN_THERMOCOUPLE.Mode(Connector::INPUT_ANALOG);
	PIN_HEATER_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_HEATER_RELAY.State(false); // Ensure heater is off on startup
}

/**
 * @param cmd The command to be executed.
 * @param args A string containing any arguments for the command.
 */
void HeaterController::handleCommand(Command cmd, const char* args) {
	switch (cmd) {
	case CMD_HEATER_ON:
		heaterOn();
		break;
	case CMD_HEATER_OFF:
		heaterOff();
		break;
	case CMD_SET_HEATER_GAINS:
		setGains(args);
		break;
	case CMD_SET_HEATER_SETPOINT:
		setSetpoint(args);
		break;
	default:
		// This command was not for us
		break;
	}
}

void HeaterController::heaterOn() {
	// This function now enables PID control.
	if (m_heaterState != HEATER_PID_ACTIVE) {
		resetPID(); // Reset PID terms before starting
		m_heaterState = HEATER_PID_ACTIVE;
		m_comms->sendStatus(STATUS_PREFIX_DONE, "HEATER_ON: PID control activated.");
		} else {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "HEATER_ON ignored: PID was already active.");
	}
}

void HeaterController::heaterOff() {
	if (m_heaterState != HEATER_OFF) {
		m_heaterState = HEATER_OFF;
		PIN_HEATER_RELAY.State(false); // Ensure relay is off
		m_pid_output = 0.0f;
		m_comms->sendStatus(STATUS_PREFIX_DONE, "HEATER_OFF: PID control deactivated.");
		} else {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "HEATER_OFF ignored: Heater was already off.");
	}
}

void HeaterController::setGains(const char* args) {
	if (sscanf(args, "%f %f %f", &m_pid_kp, &m_pid_ki, &m_pid_kd) == 3) {
		char response[128];
		snprintf(response, sizeof(response), "Heater gains set: P=%.2f, I=%.2f, D=%.2f", m_pid_kp, m_pid_ki, m_pid_kd);
		m_comms->sendStatus(STATUS_PREFIX_DONE, response);
		resetPID(); // Reset integral and derivative terms after changing gains
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid format for SET_HEATER_GAINS. Expected: P I D");
	}
}

void HeaterController::setSetpoint(const char* args) {
	float newSetpoint = atof(args);
	if (newSetpoint > 20.0f && newSetpoint < 200.0f) { // Basic safety check
		m_pid_setpoint = newSetpoint;
		char response[128];
		snprintf(response, sizeof(response), "Heater setpoint changed to %.1f C", m_pid_setpoint);
		m_comms->sendStatus(STATUS_PREFIX_DONE, response);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid setpoint. Must be between 20 and 200 C.");
	}
}

void HeaterController::updateTemperature() {
	uint16_t adc_val = PIN_THERMOCOUPLE.State();
	float voltage_from_sensor = (float)adc_val * (TC_V_REF / 4095.0f);
	float raw_celsius = (voltage_from_sensor - TC_V_OFFSET) * TC_GAIN;

	if (m_firstTempReading) {
		m_smoothedTemperatureCelsius = raw_celsius;
		m_firstTempReading = false;
		} else {
		m_smoothedTemperatureCelsius = (EWMA_ALPHA_SENSORS * raw_celsius) + ((1.0f - EWMA_ALPHA_SENSORS) * m_smoothedTemperatureCelsius);
	}
	m_temperatureCelsius = m_smoothedTemperatureCelsius;
}

void HeaterController::updateState() {
	if (m_heaterState != HEATER_PID_ACTIVE) {
		// If PID is not active, ensure the output is zero and the relay is off.
		if (m_pid_output != 0.0f) {
			m_pid_output = 0.0f;
			PIN_HEATER_RELAY.State(false);
		}
		return;
	}

	uint32_t now = Milliseconds();
	float time_change_ms = (float)(now - m_pid_last_time);

	// Only run PID calculation at the specified interval
	if (time_change_ms < PID_UPDATE_INTERVAL_MS) {
		return;
	}

	float error = m_pid_setpoint - m_temperatureCelsius;
	m_pid_integral += error * (time_change_ms / 1000.0f);
	float derivative = ((error - m_pid_last_error) * 1000.0f) / time_change_ms;

	m_pid_output = (m_pid_kp * error) + (m_pid_ki * m_pid_integral) + (m_pid_kd * derivative);

	// Clamp the output and integral to prevent windup and invalid values
	if (m_pid_output > 100.0f) m_pid_output = 100.0f;
	if (m_pid_output < 0.0f) m_pid_output = 0.0f;
	if (m_pid_ki > 0) { // Avoid division by zero if Ki is zero
		if (m_pid_integral > 100.0f / m_pid_ki) m_pid_integral = 100.0f / m_pid_ki;
	}
	if (m_pid_integral < 0.0f) m_pid_integral = 0.0f;

	m_pid_last_error = error;
	m_pid_last_time = now;

	// Time-proportioned control for the relay
	uint32_t on_duration_ms = (uint32_t)(PID_PWM_PERIOD_MS * (m_pid_output / 100.0f));
	PIN_HEATER_RELAY.State((now % PID_PWM_PERIOD_MS) < on_duration_ms);
}

void HeaterController::resetPID() {
	m_pid_integral = 0.0f;
	m_pid_last_error = 0.0f;
	m_pid_output = 0.0f;
	m_pid_last_time = Milliseconds();
}

const char* HeaterController::getTelemetryString() {
	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"h_st:%d,h_sp:%.1f,h_pv:%.1f,h_op:%.1f",
	(int)m_heaterState, m_pid_setpoint, m_temperatureCelsius, m_pid_output);
	return m_telemetryBuffer;
}
