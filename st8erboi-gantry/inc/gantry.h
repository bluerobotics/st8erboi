/**
 * @file gantry.h
 * @author Your Name
 * @date August 19, 2025
 * @brief Master controller for the XYZ gantry system.
 *
 * This file defines the main Gantry class, which serves as the central controller
 * for the physical gantry apparatus. It owns and orchestrates the individual Axis
 * controllers (X, Y, and Z), manages the overall system state (e.g., STANDBY,
 * HOMING), and delegates all communication tasks to a dedicated CommsController.
 */

#pragma once

#include "ClearCore.h"
#include "config.h"
#include "axis_controller.h"
#include "comms_controller.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Defines the primary operational states of the entire gantry system.
 * The overall state is determined by the collective state of the individual axes.
 */
typedef enum {
    GANTRY_STANDBY, ///< The gantry is idle and ready to accept commands.
    GANTRY_HOMING,  ///< One or more axes are currently executing a homing routine.
    GANTRY_MOVING,   ///< One or more axes are currently executing a move command.
    GANTRY_ERROR    ///< An axis motor has faulted, requiring CLEAR_ERRORS.
} GantryState;

/**
 * @class Gantry
 * @brief The main orchestrator for the gantry system.
 *
 * This class encapsulates the top-level logic for the gantry. It does not
 * handle low-level communication or motor control directly, but rather delegates
 * those responsibilities to its member objects (CommsController and Axis).
 * Its primary role is to process incoming commands, update the state of its
 * components, and report status and telemetry.
 */
class Gantry {
public:
    /**
     * @brief Constructs the Gantry controller.
     * @details Initializes all member objects, including the communications
     * controller and each physical axis, preparing them for setup.
     */
    Gantry();

    /**
     * @brief Performs one-time hardware and software initialization.
     * @details This method should be called once at startup. It configures motors,
     * initializes network communication, and ensures all components are ready
     * for operation.
     */
    void setup();

    /**
     * @brief The main, non-blocking update loop for the gantry system.
     * @details This function is intended to be called continuously. It drives all
     * real-time operations, including processing network messages, updating axis
     * state machines, and publishing telemetry.
     */
    void loop();

    /**
     * @brief Public interface to send a status message.
     * @details This wrapper method allows owned objects (like an Axis) to send
     * status messages (INFO, DONE, ERROR) through the Gantry's CommsController
     * without needing a direct pointer to it.
     * @param statusType The prefix for the message (e.g., "INFO: ").
     * @param message The content of the message to send.
     */
    void reportEvent(const char* statusType, const char* message);

private:
    //================================================================================
    // Private Methods
    //================================================================================

    /**
     * @brief Central dispatcher for all incoming commands.
     * @details Parses a received message and routes the command and its arguments
     * to the appropriate handler function or Axis object.
     * @param msg The message object received from the communications queue.
     */
    void dispatchCommand(const Message& msg);

    /**
     * @brief Aborts all motion on all axes immediately.
     */
    void abort();
    
    /**
     * @brief Enables all gantry axes.
     */
    void enable();

    /**
     * @brief Disables all gantry axes.
     */
    void disable();

    /**
     * @brief Resets all axes after a fault condition.
     */
    void clearErrors();

    /**
     * @brief Sets the gantry to standby mode.
     */
    void standby();

    /**
     * @brief Assembles and queues a telemetry packet for transmission.
     * @details Gathers state information from all axes and system components,
     * formats it into a single string, and enqueues it for sending via the
     * CommsController.
     */
    void publishTelemetry();

    /**
     * @brief Updates the overall gantry state based on individual axis states.
     * @details Consolidates the status of all axes into a single, high-level
     * GantryState (e.g., if any axis is moving, the whole gantry is MOVING).
     */
    void updateState();

    /**
     * @brief Converts the current GantryState enum to a human-readable string.
     * @return A const char* representing the current state (e.g., "STANDBY").
     */
    const char* getState();

    //================================================================================
    // Member Variables
    //================================================================================

    /// @brief Handles all network communication, message queuing, and command parsing.
    CommsController m_comms;

    /// @brief Controller for the X-axis.
    Axis xAxis;
    /// @brief Controller for the Y-axis gantry.
    Axis yAxis;
    /// @brief Controller for the Z-axis.
    Axis zAxis;

    /// @brief The overall state of the gantry system.
    GantryState m_state;

    char m_ip_address_str[16];

    /// @brief A reusable buffer for formatting the telemetry string.
    char m_telemetryBuffer[300];
    /// @brief Timestamp of the last telemetry transmission to regulate send frequency.
    uint32_t m_lastTelemetryTime;
};