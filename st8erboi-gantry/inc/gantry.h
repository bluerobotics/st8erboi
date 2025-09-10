/**
 * @file gantry.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Defines the master controller for the XYZ gantry system.
 *
 * @details This file defines the `Gantry` class, which serves as the central
 * orchestrator for all gantry operations. It owns and manages the individual
 * `Axis` controllers (X, Y, and Z), and is responsible for the main application
 * state machine, command dispatch, and telemetry reporting.
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
 * @enum GantryState
 * @brief Defines the primary operational states of the entire gantry system.
 * @details The overall state is determined by the collective state of the individual axes.
 */
typedef enum {
    GANTRY_STANDBY, ///< The gantry is idle and ready to accept commands.
    GANTRY_HOMING,  ///< One or more axes are currently executing a homing routine.
    GANTRY_MOVING   ///< One or more axes are currently executing a move command.
} GantryState;

/**
 * @class Gantry
 * @brief The main orchestrator for the gantry system.
 *
 * @details This class encapsulates the top-level logic for the gantry. It follows
 * the Singleton pattern (instantiated once globally) and is responsible for the entire
 * application lifecycle. It does not handle low-level motor control directly, but
 * rather delegates those responsibilities to its `Axis` member objects. Its primary
 * role is to process incoming commands, update the state of its components, and
 * report status and telemetry.
 */
class Gantry {
public:
    /**
     * @brief Constructs the Gantry controller.
     * @details Initializes all member objects, including the communications
     * controller and each physical axis, preparing them for the `setup()` phase.
     */
    Gantry();

    /**
     * @brief Performs one-time hardware and software initialization.
     * @details This method should be called once at startup. It configures motors,
     * initializes network communication via the `CommsController`, and calls the
     * `setup()` method for each `Axis` to ensure all components are ready for operation.
     */
    void setup();

    /**
     * @brief The main, non-blocking update loop for the gantry system.
     * @details This function is intended to be called continuously from `main()`. It drives
     * all real-time operations, including processing network messages, updating axis
     * state machines, and periodically publishing telemetry.
     */
    void loop();

    /**
     * @brief Public interface for sub-controllers to send status messages.
     * @details This wrapper method allows owned objects (like an `Axis`) to send
     * status messages (e.g., INFO, DONE, ERROR) through the `CommsController`
     * without needing a direct pointer to it, reducing coupling.
     * @param statusType The prefix for the message (e.g., "GANTRY_INFO: "). See `config.h`.
     * @param message The content of the message to send.
     */
    void reportEvent(const char* statusType, const char* message);

private:
    /**
     * @brief Central dispatcher for all incoming commands.
     * @details Parses a received message and routes the command and its arguments
     * to the appropriate handler function or `Axis` object.
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
     * @brief Sets the gantry to standby mode and resets all axes.
     */
    void standby();

    /**
     * @brief Assembles and queues a telemetry packet for transmission.
     * @details Gathers state information from all axes and system components,
     * formats it into a single string, and enqueues it for sending via the
     * `CommsController`.
     */
    void publishTelemetry();

    /**
     * @brief Updates the overall gantry state based on individual axis states.
     * @details Consolidates the status of all axes into a single, high-level
     * `GantryState` (e.g., if any axis is moving, the whole gantry is `GANTRY_MOVING`).
     */
    void updateState();

    /**
     * @brief Converts the current `GantryState` enum to a human-readable string.
     * @return A `const char*` representing the current state (e.g., "STANDBY").
     */
    const char* getState();

    CommsController m_comms;    ///< Handles all network communication, message queuing, and command parsing.

    Axis xAxis;                 ///< Controller for the X-axis.
    Axis yAxis;                 ///< Controller for the dual-motor Y-axis.
    Axis zAxis;                 ///< Controller for the Z-axis.

    GantryState m_state;        ///< The overall state of the gantry system.

    char m_telemetryBuffer[300];///< A reusable buffer for formatting the telemetry string.
    uint32_t m_lastTelemetryTime;///< Timestamp of the last telemetry transmission to regulate send frequency.
};