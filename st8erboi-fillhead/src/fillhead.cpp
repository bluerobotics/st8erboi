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
    m_injector(&MOTOR_INJECTOR_A, &MOTOR_INJECTOR_B, this),
    m_injectorValve("inj_valve", &MOTOR_INJECTION_VALVE, this),
    m_vacuumValve("vac_valve", &MOTOR_VACUUM_VALVE, this),
    m_heater(this),
    m_vacuum(this)
{
    // Initialize the main system state.
    m_mainState = STATE_STANDBY;

    // Initialize timers for periodic tasks.
    m_lastTelemetryTime = 0;
    m_lastSensorSampleTime = 0;
}


// --- Public Methods: Setup and Main Loop ---

/**
 * @brief Initializes all hardware and sub-controllers for the entire system.
 * @details This method should be called once at startup. It sequentially calls the
 * setup() method for each component in the correct order.
 */
void Fillhead::setup() {
    // Configure all motors for step and direction control mode.
    MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);

    m_comms.setup();
    m_injector.setup();
    m_injectorValve.setup();
    m_vacuumValve.setup();
    m_heater.setup();
    m_vacuum.setup();
    m_comms.reportEvent(STATUS_PREFIX_INFO, "Fillhead system setup complete. All components initialized.");
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

    // 3. Update the main state machine and all sub-controllers.
    updateState();

    // 4. Handle time-based periodic tasks.
    uint32_t now = Milliseconds();
    if (now - m_lastSensorSampleTime >= SENSOR_SAMPLE_INTERVAL_MS) {
        m_lastSensorSampleTime = now;
        m_heater.updateTemperature();
        m_vacuum.updateVacuum();
    }
	
    if (m_comms.isGuiDiscovered() && (now - m_lastTelemetryTime >= TELEMETRY_INTERVAL_MS)) {
        m_lastTelemetryTime = now;
        publishTelemetry();
    }
}

// --- Private Methods: State, Command, and Telemetry Handling ---

/**
 * @brief Updates the main system state and the state machines of all sub-controllers.
 * @details This function is called once per loop. It first checks for motor faults
 * to determine if the system should be in an error state. If not, it checks if any
 * subsystem is busy. Otherwise, it remains in standby. It then calls the update
 * functions for all sub-controllers.
 */
void Fillhead::updateState() {
    // First, update the state of all sub-controllers to ensure their fault status is current.
    m_injector.updateState();
    m_injectorValve.updateState();
    m_vacuumValve.updateState();
    m_heater.updateState();
    m_vacuum.updateState();

    // Now, update the main Fillhead state based on the sub-controller states.
    switch (m_mainState) {
        case STATE_STANDBY:
        case STATE_BUSY: {
            // In normal operation, constantly monitor for faults.
            if (m_injector.isInFault() || m_injectorValve.isInFault() || m_vacuumValve.isInFault()) {
                m_mainState = STATE_ERROR;
                reportEvent(STATUS_PREFIX_ERROR, "Motor fault detected. System entering ERROR state. Use CLEAR_ERRORS to reset.");
                break; // Transition to error state and exit.
            }

            // If no faults, update the state based on whether any component is busy.
            if (m_injector.isBusy() || m_injectorValve.isBusy() || m_vacuumValve.isBusy() || m_vacuum.isBusy()) {
                m_mainState = STATE_BUSY;
            } else {
                m_mainState = STATE_STANDBY;
            }
            break;
        }

        case STATE_CLEARING_ERRORS: {
            // Wait for all components to finish their reset sequences.
            if (!m_injector.isBusy() && !m_injectorValve.isBusy() && !m_vacuumValve.isBusy() && !m_vacuum.isBusy()) {
                // Now it's safe to cycle motor power.
                m_injector.disable();
                m_injectorValve.disable();
                m_vacuumValve.disable();
                Delay_ms(10); // Hardware requires a brief delay.
                m_injector.enable();
                m_injectorValve.enable();
                m_vacuumValve.enable();

                // Recovery is complete.
                m_mainState = STATE_STANDBY;
                reportEvent(STATUS_PREFIX_DONE, "CLEAR_ERRORS complete. System is in STANDBY state.");
            }
            break;
        }

        case STATE_ERROR:
        case STATE_DISABLED:
            // These are terminal states. No logic is performed here.
            // They are only exited by explicit commands (CLEAR_ERRORS, ENABLE).
            break;
    }
}


/**
 * @brief Master command handler; acts as a switchboard to delegate tasks.
 * @param msg The message object containing the command string and remote IP details.
 * @details This function parses the command from the message buffer and uses a
 * switch statement to delegate it to the appropriate sub-controller or
 * handles system-level commands itself.
 */
void Fillhead::dispatchCommand(const Message& msg) {
    // The DISCOVER command is broadcast, so we'll receive messages not intended for us.
    // If the message is a discovery command but not OUR discovery command, ignore it.
    if (strncmp(msg.buffer, "DISCOVER_", 9) == 0 && strstr(msg.buffer, CMD_STR_DISCOVER) == NULL) {
        return; // This is a discovery command for another device.
    }

    Command command = m_comms.parseCommand(msg.buffer);
    
    // If the system is in an error state, block most commands.
    if (m_mainState == STATE_ERROR) {
        if (command != CMD_CLEAR_ERRORS && command != CMD_DISABLE && command != CMD_DISCOVER) {
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Command ignored: System is in ERROR state. Send CLEAR_ERRORS to reset.");
            return;
        }
    }

    // Isolate arguments by finding the first space in the command string.
    const char* args = strchr(msg.buffer, ' ');
    if (args) {
        args++; // Move pointer past the space to the start of the arguments.
    }

    // --- Master Command Delegation Switchboard ---
    switch (command) {
        // --- System-Level Commands (Handled by Fillhead) ---
        case CMD_DISCOVER: {
            // This is a special case. The discover command is broadcast, so we might
            // receive one intended for the gantry. If the command string doesn't
            // match ours, we should just silently ignore it.
            if (strstr(msg.buffer, CMD_STR_DISCOVER) == NULL) {
                return; // Not for us, so exit quietly.
            }

            char* portStr = strstr(msg.buffer, "PORT=");
            if (portStr) {
                m_comms.setGuiIp(msg.remoteIp);
                m_comms.setGuiPort(atoi(portStr + 5));
                m_comms.setGuiDiscovered(true);
                m_comms.reportEvent(STATUS_PREFIX_DISCOVERY, "FILLHEAD DISCOVERED");
            }
            break;
        }
        case CMD_ENABLE:        enable(); break;
        case CMD_DISABLE:       disable(); break;
        case CMD_ABORT:         abort(); break;
        case CMD_CLEAR_ERRORS:  clearErrors(); break;

        // --- Injector Motor Commands (Delegated to Injector) ---
        case CMD_JOG_MOVE:
        case CMD_MACHINE_HOME_MOVE:
        case CMD_CARTRIDGE_HOME_MOVE:
        case CMD_MOVE_TO_CARTRIDGE_HOME:
        case CMD_MOVE_TO_CARTRIDGE_RETRACT:
        case CMD_PAUSE_INJECTION:
        case CMD_RESUME_INJECTION:
        case CMD_CANCEL_INJECTION:
            m_injector.handleCommand(command, args);
            break;

        // --- Pinch Valve Commands (Delegated to respective PinchValve) ---
        case CMD_INJECTION_VALVE_HOME:
        case CMD_INJECTION_VALVE_OPEN:
        case CMD_INJECTION_VALVE_CLOSE:
        case CMD_INJECTION_VALVE_JOG:
            m_injectorValve.handleCommand(command, args);
            break;
        case CMD_VACUUM_VALVE_HOME:
        case CMD_VACUUM_VALVE_OPEN:
        case CMD_VACUUM_VALVE_CLOSE:
        case CMD_VACUUM_VALVE_JOG:
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
            m_comms.reportEvent(STATUS_PREFIX_ERROR, "Unknown command sent to Fillhead.");
            break;
    }
}

/**
 * @brief Aggregates telemetry data from all sub-controllers and sends it as a single UDP packet.
 */
void Fillhead::publishTelemetry() {
    if (!m_comms.isGuiDiscovered()) return;

    char telemetryBuffer[1024];
    const char* mainStateStr;
    switch(m_mainState) {
        case STATE_STANDBY:  mainStateStr = "STANDBY"; break;
        case STATE_BUSY:     mainStateStr = "BUSY"; break;
        case STATE_ERROR:    mainStateStr = "ERROR"; break;
        case STATE_DISABLED: mainStateStr = "DISABLED"; break;
        default:             mainStateStr = "UNKNOWN"; break;
    }

    // Assemble the full telemetry string from all components.
    snprintf(telemetryBuffer, sizeof(telemetryBuffer),
        "%s"
        "MAIN_STATE:%s,"
        "%s," // Injector Telemetry
        "%s," // Injection Valve Telemetry
        "%s," // Vacuum Valve Telemetry
        "%s," // Heater Telemetry
        "%s", // Vacuum Telemetry
        TELEM_PREFIX,
        mainStateStr,
        m_injector.getTelemetryString(),
        m_injectorValve.getTelemetryString(),
        m_vacuumValve.getTelemetryString(),
        m_heater.getTelemetryString(),
        m_vacuum.getTelemetryString()
    );

    // Enqueue the message for sending.
    m_comms.enqueueTx(telemetryBuffer, m_comms.getGuiIp(), m_comms.getGuiPort());
}

/**
 * @brief Enables all motors and places the system in a ready state.
 */
void Fillhead::enable() {
    if (m_mainState == STATE_DISABLED) {
        m_mainState = STATE_STANDBY;
        m_injector.enable();
        m_injectorValve.enable();
        m_vacuumValve.enable();
        m_comms.reportEvent(STATUS_PREFIX_DONE, "System ENABLE complete. Now in STANDBY state.");
    } else {
        m_comms.reportEvent(STATUS_PREFIX_INFO, "System already enabled.");
    }
}

/**
 * @brief Disables all motors and stops all operations.
 */
void Fillhead::disable() {
    abort(); // Safest to abort any motion first.
    m_mainState = STATE_DISABLED;
    m_injector.disable();
    m_injectorValve.disable();
    m_vacuumValve.disable();
    m_comms.reportEvent(STATUS_PREFIX_DONE, "System DISABLE complete.");
}

/**
 * @brief Halts all motion and resets the system state to standby.
 */
void Fillhead::abort() {
    m_comms.reportEvent(STATUS_PREFIX_INFO, "ABORT received. Stopping all motion.");
    m_injector.abortMove();
    m_injectorValve.abort();
    m_vacuumValve.abort();
    standbyMode(); // Reset states after stopping motion.
    m_comms.reportEvent(STATUS_PREFIX_DONE, "ABORT complete.");
}

/**
 * @brief Resets any error states, clears motor faults, and returns the system to standby.
 */
void Fillhead::clearErrors() {
    reportEvent(STATUS_PREFIX_INFO, "CLEAR_ERRORS received. Resetting all sub-systems...");

    // First, reset the software state of all components. This will trigger a non-blocking
    // stop of any moving parts.
    m_injector.reset();
    m_injectorValve.reset();
    m_vacuumValve.reset();
    m_vacuum.resetState();

    // Now, enter the error clearing state. The main updateState loop will handle
    // waiting for the components to become idle before cycling motor power.
    m_mainState = STATE_CLEARING_ERRORS;
}

/**
 * @brief Resets all component states to their default/idle configuration.
 */
void Fillhead::standbyMode() {
    m_injector.reset();
    m_injectorValve.reset();
    m_vacuumValve.reset();
    m_vacuum.resetState();
    m_mainState = STATE_STANDBY;
    m_comms.reportEvent(STATUS_PREFIX_INFO, "System is in STANDBY state.");
}

/**
 * @brief Public interface to send a status message.
 * @details This wrapper method allows owned objects (like an Injector) to send
 * status messages (INFO, DONE, ERROR) through the Fillhead's CommsController
 * without needing a direct pointer to it.
 * @param statusType The prefix for the message (e.g., "INFO: ").
 * @param message The content of the message to send.
 */
void Fillhead::reportEvent(const char* statusType, const char* message) {
    m_comms.reportEvent(statusType, message);
}


//==================================================================================================
// --- Program Entry Point ---
//==================================================================================================

/**
 * @brief Global instance of the entire Fillhead system.
 * @details The C++ runtime ensures the Fillhead constructor is called before main() begins.
 */
Fillhead fillhead;

/**
 * @brief The main function and entry point of the application.
 */
int main(void) {
    // Initialize all hardware and controllers.
    fillhead.setup();

    // Enter the main application loop, which runs forever.
    while (true) {
        fillhead.loop();
    }
}