#pragma once

#include "config.h"
#include "comms_controller.h"
#include "injector_controller.h"
#include "pinch_valve_controller.h"
#include "heater_controller.h"
#include "vacuum_controller.h"

// Defines the top-level operational state of the entire Fillhead system.
enum MainState : uint8_t {
	STATE_STANDBY,       // System is idle and ready to accept commands.
	STATE_BUSY,          // A non-error operation is in progress.
	STATE_ERROR,         // A fault has occurred, typically a motor fault.
	STATE_DISABLED,       // System is disabled; motors will not move.
	STATE_CLEARING_ERRORS // A special state to manage the non-blocking error recovery process.
};

/**
 * @enum ErrorState
 * @brief Defines various error conditions the system can be in.
 */
enum ErrorState : uint8_t {
	ERROR_NONE,                   ///< No error.
	ERROR_MANUAL_ABORT,           ///< Operation aborted by user command.
	ERROR_TORQUE_ABORT,           ///< Operation aborted due to exceeding torque limits.
	ERROR_MOTION_EXCEEDED_ABORT,  ///< Operation aborted because motion limits were exceeded.
	ERROR_NO_CARTRIDGE_HOME,      ///< A required cartridge home position is not set.
	ERROR_NO_MACHINE_HOME,        ///< A required machine home position is not set.
	ERROR_HOMING_TIMEOUT,         ///< Homing operation took too long to complete.
	ERROR_HOMING_NO_TORQUE_RAPID, ///< Homing failed to detect torque during the rapid move.
	ERROR_HOMING_NO_TORQUE_TOUCH, ///< Homing failed to detect torque during the touch-off move.
	ERROR_INVALID_INJECTION,      ///< An injection command was invalid.
	ERROR_NOT_HOMED,              ///< An operation required homing, but the system is not homed.
	ERROR_INVALID_PARAMETERS,     ///< A command was received with invalid parameters.
	ERROR_MOTORS_DISABLED         ///< An operation was blocked because motors are disabled.
};


class Fillhead {
public:
    /**
     * @brief Construct a new Fillhead master controller.
     */
    Fillhead();

    /**
     * @brief Sets up all sub-controllers and hardware.
     */
    void setup();

    /**
     * @brief The main application loop. Continuously updates all components.
     */
    void loop();

    /**
     * @brief Public interface to send a status message.
     * @details This wrapper method allows owned objects (like an Injector) to send
     * status messages (INFO, DONE, ERROR) through the Fillhead's CommsController
     * without needing a direct pointer to it.
     * @param statusType The prefix for the message (e.g., "INFO: ").
     * @param message The content of the message to send.
     */
    void reportEvent(const char* statusType, const char* message);

private:
    /**
     * @brief Updates the main system state and the state machines of all sub-controllers.
     * @details This function is called once per loop. It first checks for motor faults
     * to determine if the system should be in an error state. If not, it checks if any
     * subsystem is busy. Otherwise, it remains in standby. It then calls the update
     * functions for all sub-controllers.
     */
    void updateState();

	/**
	 * @brief Processes and routes an incoming command to the correct sub-system.
	 * @details This is the entry point for handling all received commands. It takes a
	 * message that has just been pulled from the communications queue, determines the
	 * required action, and then delegates the task to the appropriate component.
	 * @param msg The incoming message object containing the command to be executed.
	 */
    void dispatchCommand(const Message& msg);

	/**
	 * @brief Assembles and queues a comprehensive outgoing telemetry report.
	 * @details This is the primary function for generating outgoing data. It polls
	 * each sub-controller for its status, aggregates the information into a single
	 * packet, and then hands it off to the communications queue to be sent.
	 */
    void publishTelemetry();

    // --- Component Ownership ---
    // The Fillhead "owns" all the specialized sub-controllers.
    CommsController  m_comms;            // The communications channel
    Injector         m_injector;         // The two main dispensing motors
    PinchValve       m_injectorValve;   // The motorized injection pinch valve (M2)
    PinchValve       m_vacuumValve;      // The motorized vacuum pinch valve (M3)
    HeaterController m_heater; // The heater system
    VacuumController m_vacuum; // The vacuum pump system

    // Main system state machine
    MainState m_mainState;

    // Timers for periodic tasks
    uint32_t m_lastTelemetryTime;
    uint32_t m_lastSensorSampleTime;

    // --- Top-Level Command Handlers ---
    void enable();
    void disable();
    void abort();
    void clearErrors();
    void standby();
};