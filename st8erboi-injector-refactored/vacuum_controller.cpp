#include "vacuum_controller.h"

VacuumController::VacuumController(InjectorComms* comms) {
	m_comms = comms;
	m_vacuumOn = false;
	m_vacuumValveOn = false;
	m_vacuumPressurePsig = 0.0f;
	m_smoothedVacuumPsig = 0.0f;
	m_firstVacuumReading = true;
}

void VacuumController::setup() {
	PIN_VACUUM_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_VALVE_RELAY.Mode(Connector::OUTPUT_DIGITAL);
	PIN_VACUUM_TRANSDUCER.Mode(Connector::INPUT_ANALOG);
	PIN_VACUUM_RELAY.State(false);
	PIN_VACUUM_VALVE_RELAY.State(false);
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
	if (m_vacuumPressurePsig < VAC_PRESSURE_MIN) m_vacuumPressurePsig = VAC_PRESSURE_MIN;
	if (m_vacuumPressurePsig > VAC_PRESSURE_MAX) m_vacuumPressurePsig = VAC_PRESSURE_MAX;
}

const char* VacuumController::getTelemetryString() {
	snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
	"vac_on:%d,vac_psig:%.2f",
	(int)m_vacuumOn, m_vacuumPressurePsig
	);
	return m_telemetryBuffer;
}

void VacuumController::handleCommand(UserCommand cmd, const char* args) {
	switch(cmd) {
		case CMD_VACUUM_ON:     handleVacuumOn(); break;
		case CMD_VACUUM_OFF:    handleVacuumOff(); break;
		default: break;
	}
}

void VacuumController::handleVacuumOn() {
	PIN_VACUUM_RELAY.State(true);
	m_vacuumOn = true;
	PIN_VACUUM_VALVE_RELAY.State(true);
	m_vacuumValveOn = true;
	m_comms->sendStatus(STATUS_PREFIX_DONE, "VACUUM_ON complete.");
}

void VacuumController::handleVacuumOff() {
	PIN_VACUUM_RELAY.State(false);
	m_vacuumOn = false;
	PIN_VACUUM_VALVE_RELAY.State(false);
	m_vacuumValveOn = false;
	m_comms->sendStatus(STATUS_PREFIX_DONE, "VACUUM_OFF complete.");
}
