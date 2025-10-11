/**
 * @file commands.h
 * @author Eldin Miller-Stead
 * @date September 10, 2025
 * @brief Defines the command interface for the Gantry controller.
 *
 * @details This header file centralizes the entire command vocabulary for the Gantry system.
 * It defines all raw command strings sent from the GUI, maps them to a comprehensive
 * `Command` enum for structured handling, and specifies the prefixes used for telemetry
 * and status messages. This approach decouples the command parsing logic from the main
 * application and ensures a single source of truth for the communication protocol.
 */
#pragma once

//==================================================================================================
// Command Strings & Prefixes
//==================================================================================================

/**
 * @name General System Commands
 * @{
 */
#define CMD_STR_DISCOVER_DEVICE             "DISCOVER_DEVICE"          ///< Generic command for any device to respond to.
#define CMD_STR_ABORT        "ABORT"           ///< Command to halt all ongoing operations.
#define CMD_STR_ENABLE       "ENABLE"          ///< Command to enable all motors.
#define CMD_STR_DISABLE      "DISABLE"         ///< Command to disable all motors.
#define CMD_STR_CLEAR_ERRORS "CLEAR_ERRORS"    ///< Command to clear any existing fault states.
/** @} */

/**
 * @name Gantry Motion Commands
 * @{
 */
#define CMD_STR_MOVE_X      "MOVE_X "         ///< Command to move the X-axis.
#define CMD_STR_MOVE_Y      "MOVE_Y "         ///< Command to move the Y-axis.
#define CMD_STR_MOVE_Z      "MOVE_Z "         ///< Command to move the Z-axis.
#define CMD_STR_HOME_X      "HOME_X"          ///< Command to home the X-axis.
#define CMD_STR_HOME_Y      "HOME_Y"          ///< Command to home the Y-axis.
#define CMD_STR_HOME_Z      "HOME_Z"          ///< Command to home the Z-axis.
#define CMD_STR_ENABLE_X    "ENABLE_X"        ///< Command to enable the X-axis motor(s).
#define CMD_STR_DISABLE_X   "DISABLE_X"       ///< Command to disable the X-axis motor(s).
#define CMD_STR_ENABLE_Y    "ENABLE_Y"        ///< Command to enable the Y-axis motor(s).
#define CMD_STR_DISABLE_Y   "DISABLE_Y"       ///< Command to disable the Y-axis motor(s).
#define CMD_STR_ENABLE_Z    "ENABLE_Z"        ///< Command to enable the Z-axis motor(s).
#define CMD_STR_DISABLE_Z   "DISABLE_Z"       ///< Command to disable the Z-axis motor(s).
/** @} */

/**
 * @name Telemetry and Status Prefixes
 * @{
 */
#define TELEM_PREFIX            "GANTRY_TELEM: "    ///< Prefix for all telemetry messages.
#define STATUS_PREFIX_INFO      "GANTRY_INFO: "     ///< Prefix for informational status messages.
#define STATUS_PREFIX_START     "GANTRY_START: "    ///< Prefix for messages indicating the start of an operation.
#define STATUS_PREFIX_DONE      "GANTRY_DONE: "     ///< Prefix for messages indicating the successful completion of an operation.
#define STATUS_PREFIX_ERROR     "GANTRY_ERROR: "    ///< Prefix for messages indicating an error or fault.
#define STATUS_PREFIX_DISCOVERY "DISCOVERY_RESPONSE: "       ///< Prefix for the device discovery response.
/** @} */

/**
 * @enum Command
 * @brief Enumerates all possible commands that can be processed by the Gantry.
 * @details This enum provides a type-safe way to handle incoming commands. The
 * `CommsController` parses raw command strings into these enum values, which
 * are then used in switch statements for command dispatch.
 */
typedef enum {
	CMD_UNKNOWN,        ///< Represents an unrecognized or invalid command.

	// General Commands
	CMD_DISCOVER_DEVICE,
	CMD_ABORT,          ///< @see CMD_STR_ABORT
    CMD_ENABLE,         ///< @see CMD_STR_ENABLE
    CMD_DISABLE,        ///< @see CMD_STR_DISABLE
    CMD_CLEAR_ERRORS,   ///< @see CMD_STR_CLEAR_ERRORS

	// Motion Commands
	CMD_MOVE_X,         ///< @see CMD_STR_MOVE_X
	CMD_MOVE_Y,         ///< @see CMD_STR_MOVE_Y
	CMD_MOVE_Z,         ///< @see CMD_STR_MOVE_Z
	CMD_HOME_X,         ///< @see CMD_STR_HOME_X
	CMD_HOME_Y,         ///< @see CMD_STR_HOME_Y
	CMD_HOME_Z,         ///< @see CMD_STR_HOME_Z

	// Axis-Specific Enable/Disable
	CMD_ENABLE_X,       ///< @see CMD_STR_ENABLE_X
	CMD_DISABLE_X,      ///< @see CMD_STR_DISABLE_X
	CMD_ENABLE_Y,       ///< @see CMD_STR_ENABLE_Y
	CMD_DISABLE_Y,      ///< @see CMD_STR_DISABLE_Y
	CMD_ENABLE_Z,       ///< @see CMD_STR_ENABLE_Z
	CMD_DISABLE_Z       ///< @see CMD_STR_DISABLE_Z
} Command;
