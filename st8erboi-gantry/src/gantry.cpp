/**
 * @file gantry.cpp
 * @author Your Name
 * @date August 19, 2025
 * @brief Implementation of the Gantry master controller.
 *
 * This file provides the logic for the Gantry class methods declared in gantry.h.
 * It also contains the global instance of the Gantry class and the main()
 * function, which serves as the application's entry point.
 */

#include "gantry.h"
#include <math.h>

/**
 * @brief Construct the Gantry controller.
 */
Gantry::Gantry()
    : xAxis(&MotorX, "X"),
      yAxis(&MotorY1, "Y"), // Using Y1 as the primary motor for the Y-axis
      zAxis(&MotorZ, "Z") {
    standby();
    m_lastTelemetryTime = 0;
}

/**
 * @brief Perform one-time hardware and software initialization.
 */
void Gantry::setup() {
    // Configure all motors for step and direction mode.
    MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);
    
    // Setup communications
    m_comms.setup();

    // Setup each axis controller
    xAxis.setup(this, nullptr, STEPS_PER_MM_X, X_MIN_POS, X_MAX_POS, &SENSOR_X, nullptr, nullptr, nullptr);
    yAxis.setup(this, &MotorY2, STEPS_PER_MM_Y, Y_MIN_POS, Y_MAX_POS, &SENSOR_Y1, &SENSOR_Y2, &LIMIT_Y_BACK, nullptr);
    zAxis.setup(this, nullptr, STEPS_PER_MM_Z, Z_MIN_POS, Z_MAX_POS, &SENSOR_Z, nullptr, nullptr, &Z_BRAKE);

    xAxis.setupMotors();
    yAxis.setupMotors();
    zAxis.setupMotors();
    
    // Wait for up to 2 seconds for all motors to report as enabled.
    uint32_t timeout = Milliseconds() + 2000;
    while(Milliseconds() < timeout) {
        if (MotorX.StatusReg().bit.Enabled &&
        MotorY1.StatusReg().bit.Enabled &&
        MotorY2.StatusReg().bit.Enabled &&
        MotorZ.StatusReg().bit.Enabled) {
            break;
        }
    }
}

/**
 * @brief The main, non-blocking update loop for the gantry system.
 * @details This function is intended to be called continuously. It drives all
 * real-time operations, including processing network messages, updating axis
 * state machines, and publishing telemetry.
 */
void Gantry::loop() {
    // 1. Service the communications controller. This handles reading UDP packets
    // into the Rx queue and sending any pending messages from the Tx queue.
    m_comms.update();

    // 2. Dequeue and process a single command from the Rx queue.
    Message msg;
    if (m_comms.dequeueRx(msg)) {
        dispatchCommand(msg);
    }
    
    // 3. Update the internal state machines of each axis.
    xAxis.updateState();
    yAxis.updateState();
    zAxis.updateState();

    // 4. Update the overall gantry state based on the axis states.
    updateState();

    // 5. Publish telemetry at a fixed interval if the GUI is connected.
    uint32_t now = Milliseconds();
    if (m_comms.isGuiDiscovered() && (now - m_lastTelemetryTime >= TELEMETRY_INTERVAL_MS)) {
        m_lastTelemetryTime = now;
        publishTelemetry();
    }
}

/**
 * @brief Public interface to send a status message.
 * @details This wrapper method allows owned objects (like an Axis) to send
 * status messages (INFO, DONE, ERROR) through the Gantry's CommsController
 * without needing a direct pointer to it.
 * @param statusType The prefix for the message (e.g., "INFO: ").
 * @param message The content of the message to send.
 */
void Gantry::reportEvent(const char* statusType, const char* message) {
    m_comms.reportEvent(statusType, message);
}

/**
 * @brief Assembles and queues a telemetry packet for transmission.
 * @details Gathers state information from all axes and system components,
 * formats it into a single string, and enqueues it for sending via the
 * CommsController.
 */
void Gantry::publishTelemetry() {
    if (!m_comms.isGuiDiscovered()) return;
    
    // Get peer IP from comms object for telemetry string.

    // Format the telemetry string with data from all relevant components.
    snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
        "%s"
        "gantry_state:%s,"
        "x_p:%.2f,x_t:%.2f,x_e:%d,x_h:%d,x_st:%s,"
        "y_p:%.2f,y_t:%.2f,y_e:%d,y_h:%d,y_st:%s,"
        "z_p:%.2f,z_t:%.2f,z_e:%d,z_h:%d,z_st:%s",
        TELEM_PREFIX,
        getState(),
        xAxis.getPositionMm(), xAxis.getSmoothedTorque(), xAxis.isEnabled(), xAxis.isHomed(), xAxis.getState(),
        yAxis.getPositionMm(), yAxis.getSmoothedTorque(), yAxis.isEnabled(), yAxis.isHomed(), yAxis.getState(),
        zAxis.getPositionMm(), zAxis.getSmoothedTorque(), zAxis.isEnabled(), zAxis.isHomed(), zAxis.getState());

    // Enqueue the formatted string for transmission.
    m_comms.enqueueTx(m_telemetryBuffer, m_comms.getGuiIp(), m_comms.getGuiPort());
}

/**
 * @brief Central dispatcher for all incoming commands.
 * @details Parses a received message and routes the command and its arguments
 * to the appropriate handler function or Axis object.
 * @param msg The message object received from the communications queue.
 */
void Gantry::dispatchCommand(const Message& msg) {
    // The DISCOVER command is broadcast, so we'll receive messages not intended for us.
    // If the message is a discovery command but not OUR discovery command, ignore it.
    if (strncmp(msg.buffer, "DISCOVER_", 9) == 0 && strstr(msg.buffer, CMD_STR_DISCOVER) == NULL) {
        return; // This is a discovery command for another device.
    }

    // Parse the command string into a GantryCommand enum.
    Command command = m_comms.parseCommand(msg.buffer);

    // Find the beginning of the arguments (the character after the first space).
    const char* args = strchr(msg.buffer, ' ');
    if(args) args++; // Increment pointer to skip the space.

    switch(command) {
        // --- System & Communication Commands ---
        case CMD_ABORT:         abort(); break;
        case CMD_ENABLE:        enable(); break;
        case CMD_DISABLE:       disable(); break;
        case CMD_CLEAR_ERRORS:  clearErrors(); break;

        // --- Axis Motion Commands ---
        case CMD_MOVE_X: xAxis.move(args); break;
        case CMD_MOVE_Y: yAxis.move(args); break;
        case CMD_MOVE_Z: zAxis.move(args); break;

        // --- Axis Homing Commands ---
        case CMD_HOME_X: xAxis.home(args); break;
        case CMD_HOME_Y: yAxis.home(args); break;
        case CMD_HOME_Z: zAxis.home(args); break;
        
        // --- Axis Enable/Disable Commands ---
        case CMD_ENABLE_X:
            xAxis.enable();
            reportEvent(STATUS_PREFIX_DONE, "ENABLE_X complete.");
            break;
        case CMD_DISABLE_X:
            xAxis.disable();
            reportEvent(STATUS_PREFIX_DONE, "DISABLE_X complete.");
            break;
        case CMD_ENABLE_Y:
            yAxis.enable();
            reportEvent(STATUS_PREFIX_DONE, "ENABLE_Y complete.");
            break;
        case CMD_DISABLE_Y:
            yAxis.disable();
            reportEvent(STATUS_PREFIX_DONE, "DISABLE_Y complete.");
            break;
        case CMD_ENABLE_Z:
            zAxis.enable();
            reportEvent(STATUS_PREFIX_DONE, "ENABLE_Z complete.");
            break;
        case CMD_DISABLE_Z:
            zAxis.disable();
            reportEvent(STATUS_PREFIX_DONE, "DISABLE_Z complete.");
            break;

        // --- Miscellaneous Commands ---
        case CMD_DISCOVER: {
            // This is a special case. The discover command is broadcast, so we might
            // receive one intended for the fillhead. If the command string doesn't
            // match ours, we should just silently ignore it.
            if (strstr(msg.buffer, CMD_STR_DISCOVER) == NULL) {
                return; // Not for us, so exit quietly.
            }
            // Extract the GUI's listening port from the discovery message.
            char* portStr = strstr(msg.buffer, "PORT=");
            if (portStr) {
                m_comms.setGuiIp(msg.remoteIp);
                m_comms.setGuiPort(atoi(portStr + 5));
                m_comms.setGuiDiscovered(true);
                reportEvent(STATUS_PREFIX_DISCOVERY, "GANTRY DISCOVERED");
            }
            break;
        }
        case CMD_UNKNOWN:
        default:
            // For robustness, unknown commands are silently ignored.
            break;
    }
}

/**
 * @brief Updates the overall gantry state based on individual axis states.
 * @details Consolidates the status of all axes into a single, high-level
 * GantryState (e.g., if any axis is moving, the whole gantry is MOVING).
 */
void Gantry::updateState() {
    // Homing takes precedence over all other states.
    if (xAxis.getStateEnum() == Axis::STATE_HOMING || yAxis.getStateEnum() == Axis::STATE_HOMING || zAxis.getStateEnum() == Axis::STATE_HOMING) {
        m_state = GANTRY_HOMING;
        return;
    }
    // If not homing, any motion puts the system in a moving state.
    if (xAxis.isMoving() || yAxis.isMoving() || zAxis.isMoving()) {
        m_state = GANTRY_MOVING;
        return;
    }
    // Otherwise, the system is idle.
    m_state = GANTRY_STANDBY;
}

/**
 * @brief Converts the current GantryState enum to a human-readable string.
 * @return A const char* representing the current state (e.g., "STANDBY").
 */
const char* Gantry::getState() {
    switch (m_state) {
        case GANTRY_STANDBY: return "STANDBY";
        case GANTRY_HOMING:  return "HOMING";
        case GANTRY_MOVING:  return "MOVING";
        default:             return "UNKNOWN";
    }
    return "UNKNOWN"; // Should not be reached.
}

/**
 * @brief Aborts all motion on all axes immediately.
 */
void Gantry::abort() {
    xAxis.abort();
    yAxis.abort();
    zAxis.abort();
    reportEvent(STATUS_PREFIX_DONE, "ABORT complete.");
}

void Gantry::clearErrors() {
    reportEvent(STATUS_PREFIX_INFO, "CLEAR_ERRORS received. Resetting all axes...");

    // First, abort any active motion to ensure a clean state.
    abort();

    // Power cycle the motors to clear hardware faults.
    disable();
    Delay_ms(100);
    enable();
    
    // The system is now fully reset and ready.
    standby();
    reportEvent(STATUS_PREFIX_DONE, "CLEAR_ERRORS complete.");
}

void Gantry::standby() {
    // Reset all axes to their idle states.
    xAxis.reset();
    yAxis.reset();
    zAxis.reset();

    // Set the main state and report.
    m_state = GANTRY_STANDBY;
    reportEvent(STATUS_PREFIX_INFO, "System is in STANDBY state.");
}

void Gantry::enable() {
    xAxis.enable();
    yAxis.enable();
    zAxis.enable();
    reportEvent(STATUS_PREFIX_DONE, "ENABLE complete.");
}

void Gantry::disable() {
    xAxis.disable();
    yAxis.disable();
    zAxis.disable();
    reportEvent(STATUS_PREFIX_DONE, "DISABLE complete.");
}

//================================================================================
// Program Entry Point
//================================================================================

/**
 * @brief Global instance of the Gantry controller class.
 * The C++ runtime ensures this object's constructor is called before main().
 */
Gantry gantry;

/**
 * @brief Main application entry point.
 */
int main(void) {
    // Perform one-time system initialization.
    gantry.setup();

    // Enter the main non-blocking application loop.
    while (true) {
        gantry.loop();
    }
}