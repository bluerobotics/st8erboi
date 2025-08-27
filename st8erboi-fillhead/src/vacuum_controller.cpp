#include "vacuum_controller.h"
#include <cstdio>
#include <cstdlib>

VacuumController::VacuumController(CommsController* comms) {
	m_comms = comms;
	m_state = VACUUM_OFF;
	m_vacuumPressurePsig = 0.0f;
	m_smoothedVacuumPsig = 0.0f;
	m_firstVacuumReading = true;
	m_stateStartTimeMs = 0;
	m_leakTestStartPressure = 0.0f;

	// Initialize parameters with default values from config, converting MS to S
	m_targetPsig = DEFAULT_VACUUM_TARGET_PSIG;
	m_rampTimeoutSec = (float)DEFAULT_VACUUM_RAMP_TIMEOUT_MS / 1000.0f;
	m_leakTestDeltaPsig = DEFAULT_LEAK_TEST_DELTA_PSIG;
	m_leakTestDurationSec = (float)DEFAULT_LEAK_TEST_DURATION_MS / 1000.0f;
}

void VacuumController::setup() {
	PIN_VACUUM_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_TRANSDUCER.Mode(Connector::INPUT_ANALOG);
	PIN_VACUUM_VALVE_RELAY.Mode(Connector::OUTPUT_DIGITAL);

	PIN_VACUUM_RELAY.State(false);
	PIN_VACUUM_VALVE_RELAY.State(false);
}

void VacuumController::handleCommand(UserCommand cmd, const char* args) {
	// Prevent starting a new operation if one is already in progress (except for turning off)
	if (m_state != VACUUM_OFF && m_state != VACUUM_ACTIVE_HOLD) {
		if (cmd == CMD_VACUUM_ON || cmd == CMD_VACUUM_LEAK_TEST) {
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "Vacuum command ignored: An operation is already in progress.");
			return;
		}
	}

	switch(cmd) {
		case CMD_VACUUM_ON:                 handleVacuumOn(); break;
		case CMD_VACUUM_OFF:                handleVacuumOff(); break;
		case CMD_VACUUM_LEAK_TEST:          handleLeakTest(); break;
		case CMD_SET_VACUUM_TARGET:         handleSetTarget(args); break;
		case CMD_SET_VACUUM_TIMEOUT_S:      handleSetTimeout(args); break;
		case CMD_SET_LEAK_TEST_DELTA:       handleSetLeakDelta(args); break;
		case CMD_SET_LEAK_TEST_DURATION_S:  handleSetLeakDuration(args); break;
		default:
		// Not a vacuum command, ignore.
		break;
	}
}

void VacuumController::handleVacuumOn() {
	m_comms->sendStatus(STATUS_PREFIX_START, "VACUUM_ON received. Actively holding target pressure.");
	m_state = VACUUM_ACTIVE_HOLD;
	PIN_VACUUM_RELAY.State(true);
	PIN_VACUUM_VALVE_RELAY.State(true); // Assuming valve should be open
}

void VacuumController::handleVacuumOff() {
	if (m_state == VACUUM_OFF) {
		m_comms->sendStatus(STATUS_PREFIX_INFO, "VACUUM_OFF ignored: System is already OFF.");
		return;
	}
	resetState();
	m_comms->sendStatus(STATUS_PREFIX_DONE, "VACUUM_OFF complete.");
}

void VacuumController::handleLeakTest() {
	m_comms->sendStatus(STATUS_PREFIX_START, "LEAK_TEST initiated.");
	m_state = VACUUM_PULLDOWN;
	m_stateStartTimeMs = Milliseconds();
	PIN_VACUUM_RELAY.State(true);
	PIN_VACUUM_VALVE_RELAY.State(true); // Open valve to pull vacuum
}

void VacuumController::updateState() {
	// These states are terminal or passive, no automatic transitions needed.
	if (m_state == VACUUM_OFF || m_state == VACUUM_ACTIVE_HOLD || m_state == VACUUM_ERROR) {
		return;
	}

	float elapsed_sec = (float)(Milliseconds() - m_stateStartTimeMs) / 1000.0f;

	if (m_state == VACUUM_PULLDOWN) {
		if (m_vacuumPressurePsig <= m_targetPsig) {
			PIN_VACUUM_RELAY.State(false);      // Pump off
			PIN_VACUUM_VALVE_RELAY.State(false);  // Close valve to seal system
			m_comms->sendStatus(STATUS_PREFIX_INFO, "Leak Test: Target reached. Pump off. Settling...");
			m_state = VACUUM_SETTLING;
			m_stateStartTimeMs = Milliseconds();
			} else if (elapsed_sec > m_rampTimeoutSec) {
			m_state = VACUUM_ERROR;
			PIN_VACUUM_RELAY.State(false);
			m_comms->sendStatus(STATUS_PREFIX_ERROR, "LEAK_TEST FAILED: Did not reach target pressure in time.");
		}
	}
	else if (m_state == VACUUM_SETTLING) {
		if (elapsed_sec > VACUUM_SETTLE_TIME_S) {
			m_leakTestStartPressure = m_smoothedVacuumPsig;
			char startMsg[STATUS_MESSAGE_BUFFER_SIZE];
			snprintf(startMsg, sizeof(startMsg), "Leak Test: Starting measurement at %.3f PSI.", m_leakTestStartPressure);
			m_comms->sendStatus(STATUS_PREFIX_INFO, startMsg);
			m_state = VACUUM_LEAK_TESTING;
			m_stateStartTimeMs = Milliseconds();
		}
	}
	else if (m_state == VACUUM_LEAK_TESTING) {
		float pressure_delta = m_smoothedVacuumPsig - m_leakTestStartPressure;
		if (pressure_delta > m_leakTestDeltaPsig) {
			m_state = VACUUM_ERROR;
			char errorMsg[STATUS_MESSAGE_BUFFER_SIZE];
			snprintf(errorMsg, sizeof(errorMsg), "LEAK_TEST FAILED. Loss of %.3f PSI exceeded limit.", pressure_delta);
			m_comms->sendStatus(STATUS_PREFIX_ERROR, errorMsg);
			} else if (elapsed_sec > m_leakTestDurationSec) {
			char passMsg[STATUS_MESSAGE_BUFFER_SIZE];
			snprintf(passMsg, sizeof(passMsg), "LEAK_TEST PASSED. Pressure loss was %.3f PSI.", pressure_delta);
			m_comms->sendStatus(STATUS_PREFIX_DONE, passMsg);
			resetState();
		}
	}
}

void VacuumController::resetState() {
	m_state = VACUUM_OFF;
	PIN_VACUUM_RELAY.State(false);
	PIN_VACUUM_VALVE_RELAY.State(false);
}

void VacuumController::handleSetTarget(const char* args) {
	float val = std::atof(args);
	if (val <= 0 && val > -15.0f) {
		m_targetPsig = val;
		char response[STATUS_MESSAGE_BUFFER_SIZE];
		snprintf(response, sizeof(response), "Vacuum target set to %.2f PSIG.", m_targetPsig);
		m_comms->sendStatus(STATUS_PREFIX_DONE, response);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid vacuum target. Must be between 0 and -15.");
	}
}

void VacuumController::handleSetTimeout(const char* args) {
	float val = std::atof(args);
	if (val >= 0.5f && val <= 60.0f) {
		m_rampTimeoutSec = val;
		char response[STATUS_MESSAGE_BUFFER_SIZE];
		snprintf(response, sizeof(response), "Vacuum ramp timeout set to %.1f seconds.", m_rampTimeoutSec);
		m_comms->sendStatus(STATUS_PREFIX_DONE, response);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid timeout. Must be between 0.5 and 60.0 seconds.");
	}
}

void VacuumController::handleSetLeakDelta(const char* args) {
	float val = std::atof(args);
	if (val > 0.0f && val < 5.0f) {
		m_leakTestDeltaPsig = val;
		char response[STATUS_MESSAGE_BUFFER_SIZE];
		snprintf(response, sizeof(response), "Leak test delta P set to %.3f PSI.", m_leakTestDeltaPsig);
		m_comms->sendStatus(STATUS_PREFIX_DONE, response);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid leak delta. Must be between 0 and 5 PSI.");
	}
}

void VacuumController::handleSetLeakDuration(const char* args) {
	float val = std::atof(args);
	if (val >= 1.0f && val <= 120.0f) {
		m_leakTestDurationSec = val;
		char response[STATUS_MESSAGE_BUFFER_SIZE];
		snprintf(response, sizeof(response), "Leak test duration set to %.1f seconds.", m_leakTestDurationSec);
		m_comms->sendStatus(STATUS_PREFIX_DONE, response);
		} else {
		m_comms->sendStatus(STATUS_PREFIX_ERROR, "Invalid leak duration. Must be between 1.0 and 120.0 seconds.");
	}
}

void VacuumController::updateVacuum() {
	uint16_t adc_val = PIN_VACUUM_TRANSDUCER.State();
	float measured_voltage = (float)adc_val * (TC_V_REF / 4095.0f);
	float voltage_span = VAC_V_OUT_MAX - VAC_V_OUT_MIN;
	float pressure_span = VAC_PRESSURE_MAX - VAC_PRESSURE_MIN;
	float voltage_percent = (measured_voltage - VAC_V_OUT_MIN) / voltage_span;
	float raw_psig = (voltage_percent * pressure_span) + VAC_PRESSURE_MIN;

	if (m_firstVacuumReading) {
		m_smoothedVacuumPsig = raw_psig;
		m_firstVacuumReading = false;
		} else {
		m_smoothedVacuumPsig = (EWMA_ALPHA_SENSORS * raw_psig) + ((1.0f - EWMA_ALPHA_SENSORS) * m_smoothedVacuumPsig);
	}
	m_vacuumPressurePsig = m_smoothedVacuumPsig + VACUUM_PSIG_OFFSET;
}

const char* VacuumController::getTelemetryString() {
	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"vac_st:%d,vac_pv:%.2f,vac_sp:%.1f",
	(int)m_state, m_vacuumPressurePsig, m_targetPsig);
	return m_telemetryBuffer;
}
