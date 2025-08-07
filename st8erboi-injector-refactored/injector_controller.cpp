#include "injector_controller.h"

InjectorController::InjectorController() :
m_motorController(&m_comms),
m_heaterController(&m_comms),
m_vacuumController(&m_comms)
{
	m_mainState = STANDBY_MODE;
	m_errorState = ERROR_NONE;
	m_lastGuiTelemetryTime = 0;
	m_lastSensorSampleTime = 0;
}

void InjectorController::setup() {
	m_comms.setup();
	m_motorController.setup();
	m_heaterController.setup();
	m_vacuumController.setup();
}

void InjectorController::loop() {
	m_comms.update();

	Message msg;
	if (m_comms.dequeueRx(msg)) {
		handleMessage(msg);
	}

	m_motorController.updateState();
	
	uint32_t now = Milliseconds();
	if (now - m_lastSensorSampleTime >= SENSOR_SAMPLE_INTERVAL_MS) {
		m_lastSensorSampleTime = now;
		m_heaterController.updateTemperature();
		m_vacuumController.updateVacuum();
	}
	
	m_heaterController.updatePid();

	if (m_comms.isGuiDiscovered() && (now - m_lastGuiTelemetryTime >= 100)) {
		m_lastGuiTelemetryTime = now;
		sendGuiTelemetry();
	}
}

void InjectorController::sendGuiTelemetry() {
	if (!m_comms.isGuiDiscovered()) return;

	char telemetryBuffer[1024];
	
	const char* mainStateStr = (m_mainState == STANDBY_MODE) ? "STANDBY" : "DISABLED";
	// This is a placeholder. A real implementation would have a function to get the current error string.
	const char* errorStateStr = "NO_ERROR";

	snprintf(telemetryBuffer, sizeof(telemetryBuffer),
	"%s"
	"MAIN_STATE:%s,ERROR_STATE:%s,"
	"%s," // Motor Telemetry
	"%s," // Heater Telemetry
	"%s," // Vacuum Telemetry
	"peer_disc:%d,peer_ip:%s",
	TELEM_PREFIX_GUI,
	mainStateStr,
	errorStateStr,
	m_motorController.getTelemetryString(),
	m_heaterController.getTelemetryString(),
	m_vacuumController.getTelemetryString(),
	(int)m_comms.isPeerDiscovered(), m_comms.getPeerIp().StringValue()
	);

	m_comms.enqueueTx(telemetryBuffer, m_comms.getGuiIp(), m_comms.getGuiPort());
}

void InjectorController::handleMessage(const Message& msg) {
	UserCommand command = m_comms.parseCommand(msg.buffer);
	const char* args = strchr(msg.buffer, ' ');
	if (args) args++;

	switch (command) {
		case CMD_REQUEST_TELEM:             sendGuiTelemetry(); break;
		case CMD_DISCOVER: {
			char* portStr = strstr(msg.buffer, "PORT=");
			if (portStr) {
				m_comms.setGuiIp(msg.remoteIp);
				m_comms.setGuiPort(atoi(portStr + 5));
				m_comms.setGuiDiscovered(true);
				m_comms.sendStatus(STATUS_PREFIX_DISCOVERY, "INJECTOR DISCOVERED");
			}
			break;
		}
		case CMD_ENABLE:                    handleEnable(); break;
		case CMD_DISABLE:                   handleDisable(); break;
		case CMD_ABORT:                     handleAbort(); break;
		case CMD_CLEAR_ERRORS:              handleClearErrors(); break;
		case CMD_SET_PEER_IP:               handleSetPeerIp(msg.buffer); break;
		case CMD_CLEAR_PEER_IP:             handleClearPeerIp(); break;
		
		// --- Motor Commands ---
		case CMD_JOG_MOVE:
		case CMD_MACHINE_HOME_MOVE:
		case CMD_CARTRIDGE_HOME_MOVE:
		case CMD_MOVE_TO_CARTRIDGE_HOME:
		case CMD_MOVE_TO_CARTRIDGE_RETRACT:
		case CMD_INJECT_MOVE:
		case CMD_PAUSE_INJECTION:
		case CMD_RESUME_INJECTION:
		case CMD_CANCEL_INJECTION:
		case CMD_SET_INJECTOR_TORQUE_OFFSET:
		case CMD_INJECTION_VALVE_HOME:
		case CMD_INJECTION_VALVE_OPEN:
		case CMD_INJECTION_VALVE_CLOSE:
		case CMD_INJECTION_VALVE_JOG:
		case CMD_VACUUM_VALVE_HOME:
		case CMD_VACUUM_VALVE_OPEN:
		case CMD_VACUUM_VALVE_CLOSE:
		case CMD_VACUUM_VALVE_JOG:
		m_motorController.handleCommand(command, args);
		break;

		// --- Heater Commands ---
		case CMD_HEATER_ON:
		case CMD_HEATER_OFF:
		case CMD_SET_HEATER_GAINS:
		case CMD_SET_HEATER_SETPOINT:
		case CMD_HEATER_PID_ON:
		case CMD_HEATER_PID_OFF:
		m_heaterController.handleCommand(command, args);
		break;

		// --- Vacuum Commands ---
		case CMD_VACUUM_ON:
		case CMD_VACUUM_OFF:
		m_vacuumController.handleCommand(command, args);
		break;
		
		case CMD_UNKNOWN:
		default:
		break;
	}
}

void InjectorController::handleEnable() {
	if (m_mainState == DISABLED_MODE) {
		m_motorController.enableMotors("MOTORS ENABLED (transitioned to STANDBY_MODE)");
		m_mainState = STANDBY_MODE;
		m_errorState = ERROR_NONE;
		m_comms.sendStatus(STATUS_PREFIX_DONE, "ENABLE complete. System in STANDBY_MODE.");
		} else {
		if (!m_motorController.areMotorsEnabled()) {
			m_motorController.enableMotors("MOTORS re-enabled (state unchanged)");
			} else {
			m_comms.sendStatus(STATUS_PREFIX_INFO, "Motors already enabled, system not in DISABLED_MODE.");
		}
	}
}

void InjectorController::handleDisable() {
	m_motorController.abortMove();
	m_mainState = DISABLED_MODE;
	m_errorState = ERROR_NONE;
	m_motorController.disableMotors("MOTORS DISABLED (entered DISABLED state)");
	m_comms.sendStatus(STATUS_PREFIX_DONE, "DISABLE complete.");
}

void InjectorController::handleAbort() {
	m_comms.sendStatus(STATUS_PREFIX_INFO, "ABORT received. Stopping motion and resetting...");
	m_motorController.abortMove();
	handleStandbyMode();
	m_comms.sendStatus(STATUS_PREFIX_DONE, "ABORT complete.");
}

void InjectorController::handleClearErrors() {
	m_comms.sendStatus(STATUS_PREFIX_INFO, "CLEAR_ERRORS received. Resetting system...");
	handleStandbyMode();
	m_comms.sendStatus(STATUS_PREFIX_DONE, "CLEAR_ERRORS complete.");
}

void InjectorController::handleStandbyMode() {
	bool wasError = (m_errorState != ERROR_NONE);
	
	m_motorController.abortMove();
	m_mainState = STANDBY_MODE;
	m_errorState = ERROR_NONE;
	m_motorController.resetState();

	if (wasError) {
		m_comms.sendStatus(STATUS_PREFIX_DONE, "STANDBY_MODE set and error cleared.");
		} else {
		m_comms.sendStatus(STATUS_PREFIX_DONE, "STANDBY_MODE set.");
	}
}

void InjectorController::handleSetPeerIp(const char* msg) {
	const char* ipStr = msg + strlen(CMD_STR_SET_PEER_IP);
	m_comms.setPeerIp(IpAddress(ipStr));
}

void InjectorController::handleClearPeerIp() {
	m_comms.clearPeerIp();
}

// Global instance
InjectorController injector;

int main(void) {
	injector.setup();
	while (true) {
		injector.loop();
	}
}
