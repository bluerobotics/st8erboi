#include "heater_controller.h"

HeaterController::HeaterController(InjectorComms* comms) {
	m_comms = comms;
	m_heaterState = HEATER_OFF;
	m_temperatureCelsius = 0.0f;
	m_smoothedTemperatureCelsius = 0.0f;
	m_firstTempReading = true;
	m_pid_setpoint = 70.0f;
	m_pid_kp = 60.0f;
	m_pid_ki = 2.5f;
	m_pid_kd = 40.0f;
	resetPid();
}

void HeaterController::setup() {
	PIN_HEATER_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_HEATER_RELAY.State(false);
	PIN_THERMOCOUPLE.Mode(Connector::INPUT_ANALOG);
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

void HeaterController::updatePid() {
	if (m_heaterState != HEATER_PID_ACTIVE) {
		if (m_pid_output != 0.0f) {
			m_pid_output = 0.0f;
		}
		return;
	}

	uint32_t now = Milliseconds();
	float time_change_ms = (float)(now - m_pid_last_time);

	if (time_change_ms < 10.0f) {
		return;
	}

	float error = m_pid_setpoint - m_temperatureCelsius;
	m_pid_integral += error * (time_change_ms / 1000.0f);
	float derivative = ((error - m_pid_last_error) * 1000.0f) / time_change_ms;
	
	m_pid_output = (m_pid_kp * error) + (m_pid_ki * m_pid_integral) + (m_pid_kd * derivative);

	if (m_pid_output > 100.0f) m_pid_output = 100.0f;
	if (m_pid_output < 0.0f) m_pid_output = 0.0f;
	if (m_pid_integral > 100.0f / m_pid_ki) m_pid_integral = 100.0f / m_pid_ki;
	if (m_pid_integral < 0.0f) m_pid_integral = 0.0f;

	m_pid_last_error = error;
	m_pid_last_time = now;

	uint32_t on_duration_ms = (uint32_t)(PID_PWM_PERIOD_MS * (m_pid_output / 100.0f));
	PIN_HEATER_RELAY.State((now % PID_PWM_PERIOD_MS) < on_duration_ms);
}

const char* HeaterController::getTelemetryString() {
	const char* mode_str = "OFF";
	if (m_heaterState == HEATER_MANUAL_ON) mode_str = "MANUAL";
	else if (m_heaterState == HEATER_PID_ACTIVE) mode_str = "PID";

	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"temp_c:%.1f,heater_mode:%s,pid_sp:%.1f,pid_out:%.1f",
	m_temperatureCelsius, mode_str, m_pid_setpoint, m_pid_output
	);
	return m_telemetryBuffer;
}

void HeaterController::handleCommand(UserCommand cmd, const char* args) {
	switch(cmd) {
		case CMD_HEATER_ON:             handleHeaterOn(); break;
		case CMD_HEATER_OFF:            handleHeaterOff(); break;
		case CMD_SET_HEATER_GAINS:      handleSetGains(args); break;
		case CMD_SET_HEATER_SETPOINT:   handleSetSetpoint(args); break;
		case CMD_HEATER_PID_ON:         handlePidOn(); break;
		case CMD_HEATER_PID_OFF:        handlePidOff(); break;
		default: break;
	}
}

void HeaterController::resetPid() {
	m_pid_integral = 0.0f;
	m_pid_last_error = 0.0f;
	m_pid_output = 0.0f;
	m_pid_last_time = Milliseconds();
}

void HeaterController::handleHeaterOn() {
	m_heaterState = HEATER_MANUAL_ON;
	PIN_HEATER_RELAY.State(true);
	m_comms->sendStatus(STATUS_PREFIX_DONE, "HEATER_ON complete.");
}

void HeaterController::handleHeaterOff() {
	m_heaterState = HEATER_OFF;
	PIN_HEATER_RELAY.State(false);
	m_comms->sendStatus(STATUS_PREFIX_DONE, "HEATER_OFF complete.");
}

void HeaterController::handleSetGains(const char* args) {
	float kp, ki, kd;
	if (sscanf(args, "%f %f %f", &kp, &ki, &kd) == 3) {
		m_pid_kp = kp;
		m_pid_ki = ki;
		m_pid_kd = kd;
		char response[100];
		snprintf(response, sizeof(response), "SET_HEATER_GAINS complete: Kp=%.2f, Ki=%.2f, Kd=%.2f", m_pid_kp, m_pid_ki, m_pid_kd);
		m_comms->sendStatus(STATUS_PREFIX_DONE, response);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid SET_HEATER_GAINS format.");
	}
}

void HeaterController::handleSetSetpoint(const char* args) {
	float sp = atof(args);
	m_pid_setpoint = sp;
	char response[64];
	snprintf(response, sizeof(response), "SET_HEATER_SETPOINT complete: %.1f C", m_pid_setpoint);
	m_comms->sendStatus(STATUS_PREFIX_DONE, response);
}

void HeaterController::handlePidOn() {
	if (m_heaterState != HEATER_PID_ACTIVE) {
		resetPid();
		m_heaterState = HEATER_PID_ACTIVE;
		m_comms->sendStatus(STATUS_PREFIX_DONE, "HEATER_PID_ON complete.");
	}
}

void HeaterController::handlePidOff() {
	if (m_heaterState == HEATER_PID_ACTIVE) {
		m_heaterState = HEATER_OFF;
		PIN_HEATER_RELAY.State(false);
		m_comms->sendStatus(STATUS_PREFIX_DONE, "HEATER_PID_OFF complete.");
	}
}
