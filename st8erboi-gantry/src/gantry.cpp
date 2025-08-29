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
    m_state = GANTRY_STANDBY;
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
    xAxis.setup(this);
    yAxis.setup(this);
    zAxis.setup(this);
    
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
        handleMessage(msg);
    }
    
    // 3. Update the internal state machines of each axis.
    xAxis.updateState();
    yAxis.updateState();
    zAxis.updateState();

    // 4. Update the overall gantry state based on the axis states.
    updateGantryState();

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
void Gantry::sendStatus(const char* statusType, const char* message) {
    m_comms.sendStatus(statusType, message);
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
    IpAddress peerIp = m_comms.getPeerIp();

    // Format the telemetry string with data from all relevant components.
    snprintf(m_telemetryBuffer, sizeof(m_telemetryBuffer),
        "%s"
        "gantry_state:%s,"
        "x_p:%.2f,x_t:%.2f,x_e:%d,x_h:%d,"
        "y_p:%.2f,y_t:%.2f,y_e:%d,y_h:%d,"
        "z_p:%.2f,z_t:%.2f,z_e:%d,z_h:%d,"
        "pd:%d,pip:%s",
        TELEM_PREFIX,
        getGantryStateString(),
        xAxis.getPositionMm(), xAxis.getSmoothedTorque(), xAxis.isEnabled(), xAxis.isHomed(),
        yAxis.getPositionMm(), yAxis.getSmoothedTorque(), yAxis.isEnabled(), yAxis.isHomed(),
        zAxis.getPositionMm(), zAxis.getSmoothedTorque(), zAxis.isEnabled(), zAxis.isHomed(),
        (int)m_comms.isPeerDiscovered(), peerIp.StringValue());

    // Enqueue the formatted string for transmission.
    m_comms.enqueueTx(m_telemetryBuffer, m_comms.getGuiIp(), m_comms.getGuiPort());
}

/**
 * @brief Central dispatcher for all incoming commands.
 * @details Parses a received message and routes the command and its arguments
 * to the appropriate handler function or Axis object.
 * @param msg The message object received from the communications queue.
 */
void Gantry::handleMessage(const Message& msg) {
    // Parse the command string into a GantryCommand enum.
    GantryCommand cmd = m_comms.parseCommand(msg.buffer);

    // Find the beginning of the arguments (the character after the first space).
    const char* args = strchr(msg.buffer, ' ');
    if(args) args++; // Increment pointer to skip the space.

    switch(cmd) {
        // --- System & Communication Commands ---
        case CMD_SET_PEER_IP:   handleSetPeerIp(msg.buffer); break;
        case CMD_CLEAR_PEER_IP: handleClearPeerIp(); break;
        case CMD_ABORT:         abortAll(); break;

        // --- Axis Motion Commands ---
        case CMD_MOVE_X: xAxis.handleMove(args); break;
        case CMD_MOVE_Y: yAxis.handleMove(args); break;
        case CMD_MOVE_Z: zAxis.handleMove(args); break;

        // --- Axis Homing Commands ---
        case CMD_HOME_X: xAxis.handleHome(args); break;
        case CMD_HOME_Y: yAxis.handleHome(args); break;
        case CMD_HOME_Z: zAxis.handleHome(args); break;

        // --- Axis Enable/Disable Commands ---
        case CMD_ENABLE_X:
            xAxis.enable();
            sendStatus(STATUS_PREFIX_DONE, "ENABLE_X complete.");
            break;
        case CMD_DISABLE_X:
            xAxis.disable();
            sendStatus(STATUS_PREFIX_DONE, "DISABLE_X complete.");
            break;
        case CMD_ENABLE_Y:
            yAxis.enable();
            sendStatus(STATUS_PREFIX_DONE, "ENABLE_Y complete.");
            break;
        case CMD_DISABLE_Y:
            yAxis.disable();
            sendStatus(STATUS_PREFIX_DONE, "DISABLE_Y complete.");
            break;
        case CMD_ENABLE_Z:
            zAxis.enable();
            sendStatus(STATUS_PREFIX_DONE, "ENABLE_Z complete.");
            break;
        case CMD_DISABLE_Z:
            zAxis.disable();
            sendStatus(STATUS_PREFIX_DONE, "DISABLE_Z complete.");
            break;

        // --- Miscellaneous Commands ---
        case CMD_REQUEST_TELEM:
            publishTelemetry();
            break;
        case CMD_DISCOVER: {
            // Extract the GUI's listening port from the discovery message.
            char* portStr = strstr(msg.buffer, "PORT=");
            if (portStr) {
                m_comms.setGuiIp(msg.remoteIp);
                m_comms.setGuiPort(atoi(portStr + 5));
                m_comms.setGuiDiscovered(true);
                sendStatus(STATUS_PREFIX_DISCOVERY, "GANTRY DISCOVERED");
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
 * @brief Handles the SET_PEER_IP command.
 * @param msg The full message string containing the peer's IP address.
 */
void Gantry::handleSetPeerIp(const char* msg) {
    const char* ipStr = msg + strlen(CMD_STR_SET_PEER_IP);
    m_comms.setPeerIp(IpAddress(ipStr));
}

/**
 * @brief Handles the CLEAR_PEER_IP command.
 */
void Gantry::handleClearPeerIp() {
    m_comms.clearPeerIp();
}

/**
 * @brief Updates the overall gantry state based on individual axis states.
 * @details Consolidates the status of all axes into a single, high-level
 * GantryState (e.g., if any axis is moving, the whole gantry is MOVING).
 */
void Gantry::updateGantryState() {
    // Homing takes precedence over all other states.
    if (xAxis.getState() == Axis::STATE_HOMING || yAxis.getState() == Axis::STATE_HOMING || zAxis.getState() == Axis::STATE_HOMING) {
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
const char* Gantry::getGantryStateString() {
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
void Gantry::abortAll() {
    xAxis.abort();
    yAxis.abort();
    zAxis.abort();
    sendStatus(STATUS_PREFIX_DONE, "ABORT complete.");
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