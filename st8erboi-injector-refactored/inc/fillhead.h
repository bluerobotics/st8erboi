#pragma once

#include "config.h"
#include "comms.h"
#include "injector.h"
#include "pinch_valve.h"
#include "heater.h"
#include "vacuum.h"

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
     * @brief Parses and delegates an incoming message to the correct sub-controller.
     * @param msg The message received from the communications queue.
     */
    void handleMessage(const Message& msg);

    /**
     * @brief Assembles telemetry from all components and sends it to the GUI.
     */
    void sendGuiTelemetry();

    // --- Component Ownership ---
    // The Fillhead "owns" all the specialized sub-controllers.
    InjectorComms    m_comms;            // The communications channel
    Injector         m_injector;         // The two main dispensing motors
    PinchValve       m_injectionValve;   // The motorized injection pinch valve (M2)
    PinchValve       m_vacuumValve;      // The motorized vacuum pinch valve (M3)
    HeaterController m_heaterController; // The heater system
    VacuumController m_vacuumController; // The vacuum pump system

    // Main system state machine
    MainState m_mainState;
    ErrorState m_errorState;

    // Timers for periodic tasks
    uint32_t m_lastGuiTelemetryTime;
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