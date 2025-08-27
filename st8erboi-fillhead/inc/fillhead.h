#pragma once

#include "config.h"
#include "comms_controller.h"
#include "injector_controller.h"
#include "pinch_valve_controller.h"
#include "heater_controller.h"
#include "vacuum_controller.h"

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

private:
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
    ErrorState m_errorState;

    // Timers for periodic tasks
    uint32_t m_lastTelemetryTime;
    uint32_t m_lastSensorSampleTime;

    // --- Top-Level Command Handlers ---
    void handleEnable();
    void handleDisable();
    void handleAbort();
    void handleClearErrors();
    void handleStandbyMode();
    void handleSetPeerIp(const char* msg);
    void handleClearPeerIp();
};