/**
 * @file fillhead.cpp
 * @author Eldin Miller-Stead
 * @date August 7, 2025
 * @brief Implementation of the Fillhead master controller.
 *
 * This file contains the implementation for the main Fillhead class, which serves
 * as the central "brain" for the entire dispensing system. It owns and orchestrates all
 * specialized sub-controllers (Injector, PinchValves, Heater, Vacuum) and manages
 * the main application state. The program's entry point, main(), is also located here.
 */

//==================================================================================================
// --- Includes ---
//==================================================================================================
#include "fillhead.h"
#include <cstring> // For C-style string functions like strchr, strstr
#include <cstdlib> // For C-style functions like atoi

//==================================================================================================
// --- Fillhead Class Implementation ---
//==================================================================================================

// --- Constructor & Destructor ---

/**
 * @brief Constructs the Fillhead master controller.
 * @details This constructor uses a member initializer list to instantiate all the
 * specialized sub-controllers. The CommsController object is created first,
 * as its pointer is passed to the other controllers, enabling them to
 * send status messages and telemetry.
 */
Fillhead::Fillhead() :
    // The CommsController object MUST be constructed first.
    m_comms(),
    // Pass the comms pointer to all sub-controllers that need it.
    m_injector(&MOTOR_INJECTOR_A, &MOTOR_INJECTOR_B, &m_comms),
    m_injectorValve("inj_valve", &MOTOR_INJECTION_VALVE, &m_comms),
    m_vacuumValve("vac_valve", &MOTOR_VACUUM_VALVE, &m_comms),
    m_heater(&m_comms),
    m_vacuum(&m_comms)
{
    // Initialize the main system state.
    m_mainState = STANDBY_MODE;
    m_errorState = ERROR_NONE;

    // Initialize timers for periodic tasks.
    m_lastGuiTelemetryTime = 0;
    m_lastSensorSampleTime = 0;
}


// --- Public Methods: Setup and Main Loop ---

/**
 * @brief Initializes all hardware and sub-controllers for the entire system.
 * @details This method should be called once at startup. It sequentially calls the
 * setup() method for each component in the correct order.
 */
void Fillhead::setup() {
    m_comms.setup();
    m_injector.setup();
    m_injectorValve.setup();
    m_vacuumValve.setup();
    m_heater.setup();
    m_vacuum.setup();
    m_comms.sendStatus(STATUS_PREFIX_INFO, "Fillhead system setup complete. All components initialized.");
}

/**
 * @brief The main execution loop for the Fillhead system.
 * @details This function is called continuously from main(). It orchestrates all
 * real-time operations in a non-blocking manner:
 * 1. Processes communication queues.
 * 2. Dequeues and handles one command per loop.
 * 3. Updates the state machines of all active components.
 * 4. Manages periodic tasks like sensor polling and telemetry transmission.
 */
void Fillhead::loop() {
    // 1. Process all incoming/outgoing communication queues.
    m_comms.update();

    // 2. Check for and handle one new command from the receive queue.
    Message msg;
    if (m_comms.dequeueRx(msg)) {
        dispatchCommand(msg);
    }

    // 3. Update the internal state of all active sub-controllers.
    m_injector.updateState();
    m_injectorValve.update();
    m_vacuumValve.update();
    m_heater.updatePid();
    m_vacuum.updateState();

    // If in a busy state, check if the sub-controllers have finished.
    if (m_mainState == BUSY_MODE) {
        if (!m_injector.isBusy() && !m_injectorValve.isBusy() && !m_vacuumValve.isBusy()) {
            m_mainState = STANDBY_MODE;
            m_comms.sendStatus(STATUS_PREFIX_INFO, "All operations complete. Returning to STANDBY_MODE.");
        }
    }


    // 4. Handle time-based periodic tasks.
    uint32_t now = Milliseconds();
    if (now - m_lastSensorSampleTime >= SENSOR_SAMPLE_INTERVAL_MS) {
        m_lastSensorSampleTime = now;
        m_heater.updateTemperature();
        m_vacuum.updateVacuum();
    }

    if (m_comms.isGuiDiscovered() && (now - m_lastGuiTelemetryTime >= 100)) {
        m_lastGuiTelemetryTime = now;
        publishTelemetry();
    }
}


// --- Private Methods: Command Handling ---

/**
 * @brief Master command handler; acts as a switchboard to delegate tasks.
 * @param msg The message object containing the command string and remote IP details.
 * @details This function parses the command from the message buffer and uses a
 * switch statement to delegate it to the appropriate sub-controller or
 * handles system-level commands itself.
 */
void Fillhead::dispatchCommand(const Message& msg) {
    UserCommand command = m_comms.parseCommand(msg.buffer);

    // Isolate arguments by finding the first space in the command string.
    const char* args = strchr(msg.buffer, ' ');
    if (args) {
        args++; // Move pointer past the space to the start of the arguments.
    }

    // --- Master Command Delegation Switchboard ---
    switch (command) {
        // --- System-Level Commands (Handled by Fillhead) ---
        case CMD_DISCOVER: {
            char* portStr = strstr(msg.buffer, "PORT=");
            if (portStr) {
                m_comms.setGuiIp(msg.remoteIp);
                m_comms.setGuiPort(atoi(portStr + 5));
                m_comms.setGuiDiscovered(true);
                m_comms.sendStatus(STATUS_PREFIX_DISCOVERY, "FILLHEAD DISCOVERED");
            }
            break;
        }
        case CMD_REQUEST_TELEM: publishTelemetry(); break;
        case CMD_ENABLE:        handleEnable(); break;
        case CMD_DISABLE:       handleDisable(); break;
        case CMD_ABORT:         handleAbort(); break;
        case CMD_CLEAR_ERRORS:  handleClearErrors(); break;
        case CMD_SET_PEER_IP:   handleSetPeerIp(msg.buffer); break;
        case CMD_CLEAR_PEER_IP: handleClearPeerIp(); break;

        // --- Injector Motor Commands (Delegated to Injector) ---
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
            m_mainState = BUSY_MODE;
            m_injector.handleCommand(command, args);
            break;

        // --- Pinch Valve Commands (Delegated to respective PinchValve) ---
        case CMD_INJECTION_VALVE_HOME:
        case CMD_INJECTION_VALVE_OPEN:
        case CMD_INJECTION_VALVE_CLOSE:
        case CMD_INJECTION_VALVE_JOG:
            m_mainState = BUSY_MODE;
            m_injectorValve.handleCommand(command, args);
            break;
        case CMD_VACUUM_VALVE_HOME:
        case CMD_VACUUM_VALVE_OPEN:
        case CMD_VACUUM_VALVE_CLOSE:
        case CMD_VACUUM_VALVE_JOG:
            m_mainState = BUSY_MODE;
            m_vacuumValve.handleCommand(command, args);
            break;

        // --- Heater Commands (Delegated to HeaterController) ---
        case CMD_HEATER_ON:
        case CMD_HEATER_OFF:
        case CMD_SET_HEATER_GAINS:
        case CMD_SET_HEATER_SETPOINT:
            m_heater.handleCommand(command, args);
            break;

        // --- Vacuum Commands (Delegated to VacuumController) ---
        case CMD_VACUUM_ON:
        case CMD_VACUUM_OFF:
        case CMD_VACUUM_LEAK_TEST:
        case CMD_SET_VACUUM_TARGET:
        case CMD_SET_VACUUM_TIMEOUT_S:
        case CMD_SET_LEAK_TEST_DELTA:
        case CMD_SET_LEAK_TEST_DURATION_S:
            m_vacuum.handleCommand(command, args);
            break;

        // --- Default/Unknown ---
        case CMD_UNKNOWN:
        default:
            m_comms.sendStatus(STATUS_PREFIX_ERROR, "Unknown command sent to Fillhead.");
            break;
    }
}

/**
 * @brief Enables all motors and places the system in a ready state.
 */
void Fillhead::handleEnable() {
    if (m_mainState == DISABLED_MODE) {
        m_mainState = STANDBY_MODE;
        m_errorState = ERROR_NONE;
        m_injector.enable();
        m_injectorValve.enable();
        m_vacuumValve.enable();
        m_comms.sendStatus(STATUS_PREFIX_DONE, "System ENABLE complete. Now in STANDBY_MODE.");
    } else {
        m_comms.sendStatus(STATUS_PREFIX_INFO, "System already enabled.");
    }
}

/**
 * @brief Disables all motors and stops all operations.
 */
void Fillhead::handleDisable() {
    handleAbort(); // Safest to abort any motion first.
    m_mainState = DISABLED_MODE;
    m_errorState = ERROR_NONE;
    m_injector.disable();
    m_injectorValve.disable();
    m_vacuumValve.disable();
    m_comms.sendStatus(STATUS_PREFIX_DONE, "System DISABLE complete.");
}

/**
 * @brief Halts all motion and resets the system state to standby.
 */
void Fillhead::handleAbort() {
    m_comms.sendStatus(STATUS_PREFIX_INFO, "ABORT received. Stopping all motion.");
    m_injector.abortMove();
    m_injectorValve.abort();
    m_vacuumValve.abort();
    handleStandbyMode(); // Reset states after stopping motion.
    m_comms.sendStatus(STATUS_PREFIX_DONE, "ABORT complete.");
}

/**
 * @brief Resets any error states and returns the system to standby.
 */
void Fillhead::handleClearErrors() {
    m_comms.sendStatus(STATUS_PREFIX_INFO, "CLEAR_ERRORS received. Resetting system state...");
    handleStandbyMode();
    m_comms.sendStatus(STATUS_PREFIX_DONE, "CLEAR_ERRORS complete.");
}

/**
 * @brief Resets all component states to their default/idle configuration.
 */
void Fillhead::handleStandbyMode() {
    bool wasError = (m_errorState != ERROR_NONE);
    m_injector.resetState();
    m_injectorValve.reset();
    m_vacuumValve.reset();
    m_mainState = STANDBY_MODE;
    m_errorState = ERROR_NONE;

    if (wasError) {
        m_comms.sendStatus(STATUS_PREFIX_INFO, "Error cleared. System is in STANDBY_MODE.");
    } else {
        m_comms.sendStatus(STATUS_PREFIX_INFO, "System is in STANDBY_MODE.");
    }
}

/**
 * @brief Sets the IP address of the peer device for peer-to-peer communication.
 * @param msg The full command message containing the IP address.
 */
void Fillhead::handleSetPeerIp(const char* msg) {
    const char* ipStr = msg + strlen(CMD_STR_SET_PEER_IP);
    m_comms.setPeerIp(IpAddress(ipStr));
}

/**
 * @brief Clears the stored peer IP address.
 */
void Fillhead::handleClearPeerIp() {
    m_comms.clearPeerIp();
}


// --- Private Methods: Telemetry ---

/**
 * @brief Aggregates telemetry data from all sub-controllers and sends it as a single UDP packet.
 */
void Fillhead::publishTelemetry() {
    if (!m_comms.isGuiDiscovered()) return;

    char telemetryBuffer[1024];
    const char* mainStateStr;
    switch (m_mainState) {
        case STANDBY_MODE: mainStateStr = "STANDBY"; break;
        case BUSY_MODE:    mainStateStr = "BUSY"; break;
        case DISABLED_MODE:mainStateStr = "DISABLED"; break;
        default:           mainStateStr = "UNKNOWN"; break;
    }
    const char* errorStateStr = "NO_ERROR"; // TODO: Implement a function to convert m_errorState enum to a string.

    // Create a non-const copy of the peer IP to safely call StringValue()
    IpAddress peerIp = m_comms.getPeerIp();

    // Assemble the full telemetry string from all components.
    snprintf(telemetryBuffer, sizeof(telemetryBuffer),
        "%s"
        "MAIN_STATE:%s,ERROR_STATE:%s,"
        "%s," // Injector Telemetry
        "%s," // Injection Valve Telemetry
        "%s," // Vacuum Valve Telemetry
        "%s," // Heater Telemetry
        "%s," // Vacuum Telemetry
        "peer_disc:%d,peer_ip:%s",
        TELEM_PREFIX_GUI,
        mainStateStr, errorStateStr,
        m_injector.getTelemetryString(),
        m_injectorValve.getTelemetryString(),
        m_vacuumValve.getTelemetryString(),
        m_heater.getTelemetryString(),
        m_vacuum.getTelemetryString(),
        (int)m_comms.isPeerDiscovered(), peerIp.StringValue()
    );

    // Enqueue the message for sending.
    m_comms.enqueueTx(telemetryBuffer, m_comms.getGuiIp(), m_comms.getGuiPort());
}


//==================================================================================================
// --- Program Entry Point ---
//==================================================================================================

/**
 * @brief Global instance of the entire Fillhead system.
 * @details The C++ runtime ensures the Fillhead constructor is called before main() begins.
 */
Fillhead fillhead_system;

/**
 * @brief The main function and entry point of the application.
 */
int main(void) {
    // Initialize all hardware and controllers.
    fillhead_system.setup();

    // Enter the main application loop, which runs forever.
    while (true) {
        fillhead_system.loop();
    }
}
