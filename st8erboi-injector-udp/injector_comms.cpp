// ============================================================================
// injector_comms.cpp
//
// UDP communication and telemetry interface for the Injector class.
//
// Handles discovery, message reception, parsing, routing, and response logic
// for communication with the remote terminal. Encodes system state into
// structured telemetry messages and manages connection state.
//
// Also defines command string constants and maps them to `UserCommand` enums
// for interpretation within the handler layer.
//
// Corresponds to declarations in `injector.h`.
//
// Copyright 2025 Blue Robotics Inc.
// Author: Eldin Miller-Stead <eldin@bluerobotics.com>
// ============================================================================

#include "injector.h"

// -----------------------------------------------------------------------------
// Queue Management
// -----------------------------------------------------------------------------

bool Injector::enqueueRx(const char* msg, const IpAddress& ip, uint16_t port) {
    int next_head = (m_rxQueueHead + 1) % RX_QUEUE_SIZE;
    if (next_head == m_rxQueueTail) {
        // Rx Queue is full. Send an immediate error back to the GUI.
        if(guiDiscovered) {
            char errorMsg[] = "INJ_ERROR: RX QUEUE OVERFLOW - COMMAND DROPPED";
            udp.Connect(guiIp, guiPort);
            udp.PacketWrite(errorMsg);
            udp.PacketSend();
        }
        return false;
    }
    strncpy(m_rxQueue[m_rxQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
    m_rxQueue[m_rxQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0'; // Ensure null termination
    m_rxQueue[m_rxQueueHead].remoteIp = ip;
    m_rxQueue[m_rxQueueHead].remotePort = port;
    m_rxQueueHead = next_head;
    return true;
}

bool Injector::dequeueRx(Message& msg) {
    if (m_rxQueueHead == m_rxQueueTail) {
        // Queue is empty
        return false;
    }
    msg = m_rxQueue[m_rxQueueTail];
    m_rxQueueTail = (m_rxQueueTail + 1) % RX_QUEUE_SIZE;
    return true;
}

bool Injector::enqueueTx(const char* msg, const IpAddress& ip, uint16_t port) {
    int next_head = (m_txQueueHead + 1) % TX_QUEUE_SIZE;
    if (next_head == m_txQueueTail) {
        // Tx Queue is full. Send an immediate error message, bypassing the queue.
        // This is a critical error, as status messages are being dropped.
        if(guiDiscovered) {
            char errorMsg[] = "INJ_ERROR: TX QUEUE OVERFLOW - MESSAGE DROPPED";
            udp.Connect(guiIp, guiPort);
            udp.PacketWrite(errorMsg);
            udp.PacketSend();
        }
        return false;
    }
    strncpy(m_txQueue[m_txQueueHead].buffer, msg, MAX_MESSAGE_LENGTH);
    m_txQueue[m_txQueueHead].buffer[MAX_MESSAGE_LENGTH - 1] = '\0'; // Ensure null termination
    m_txQueue[m_txQueueHead].remoteIp = ip;
    m_txQueue[m_txQueueHead].remotePort = port;
    m_txQueueHead = next_head;
    return true;
}

bool Injector::dequeueTx(Message& msg) {
    if (m_txQueueHead == m_txQueueTail) {
        // Queue is empty
        return false;
    }
    msg = m_txQueue[m_txQueueTail];
    m_txQueueTail = (m_txQueueTail + 1) % TX_QUEUE_SIZE;
    return true;
}


// -----------------------------------------------------------------------------
// Network and Message Processing
// -----------------------------------------------------------------------------

void Injector::processUdp() {
    // Process all available packets in the UDP hardware buffer to avoid dropping any during a burst.
    while (udp.PacketParse()) {
        IpAddress remoteIp = udp.RemoteIp();
        uint16_t remotePort = udp.RemotePort();
        int32_t bytesRead = udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
        if (bytesRead > 0) {
            packetBuffer[bytesRead] = '\0';
            // Enqueue the received message. If the queue is full, enqueueRx will handle the error.
            enqueueRx((char*)packetBuffer, remoteIp, remotePort);
        }
    }
}

void Injector::processRxQueue() {
    // Process only ONE message per loop. This keeps the main loop consistently fast.
    Message msg;
    if (dequeueRx(msg)) {
        handleMessage(msg);
    }
}

void Injector::processTxQueue() {
    Message msg;
    // Send only one message per update loop to keep the loop fast and non-blocking.
    if (dequeueTx(msg)) {
        udp.Connect(msg.remoteIp, msg.remotePort);
        udp.PacketWrite(msg.buffer);
        udp.PacketSend();
    }
}

void Injector::sendStatus(const char* statusType, const char* message) {
    char fullMsg[MAX_MESSAGE_LENGTH];
    snprintf(fullMsg, sizeof(fullMsg), "%s%s", statusType, message);
    
    // Enqueue the message for the GUI
    if (guiDiscovered) {
        enqueueTx(fullMsg, guiIp, guiPort);
    }
    // Enqueue the message for the peer if needed
    if (peerDiscovered) {
        // Example: enqueueTx(fullMsg, peerIp, LOCAL_PORT);
    }
}


void Injector::setupSerial(void)
{
    ConnectorUsb.Mode(Connector::USB_CDC);
    ConnectorUsb.Speed(9600);
    ConnectorUsb.PortOpen();
    uint32_t timeout = 5000;
    uint32_t start = Milliseconds();
    while (!ConnectorUsb && Milliseconds() - start < timeout);
}

void Injector::setupEthernet(void)
{
    ConnectorUsb.SendLine("SetupEthernet: Starting...");
    EthernetMgr.Setup();
    ConnectorUsb.SendLine("SetupEthernet: EthernetMgr.Setup() done.");

    if (!EthernetMgr.DhcpBegin()) {
        ConnectorUsb.SendLine("SetupEthernet: DhcpBegin() FAILED. System Halted.");
        while (1);
    }
    
    char ipAddrStr[100];
    snprintf(ipAddrStr, sizeof(ipAddrStr), "SetupEthernet: DhcpBegin() successful. IP: %s", EthernetMgr.LocalIp().StringValue());
    ConnectorUsb.SendLine(ipAddrStr);

    ConnectorUsb.SendLine("SetupEthernet: Waiting for PhyLinkActive...");
    while (!EthernetMgr.PhyLinkActive()) {
        Delay_ms(1000);
        ConnectorUsb.SendLine("SetupEthernet: Still waiting for PhyLinkActive...");
    }
    ConnectorUsb.SendLine("SetupEthernet: PhyLinkActive is active.");

    udp.Begin(LOCAL_PORT);
    ConnectorUsb.SendLine("SetupEthernet: udp.Begin() called for port 8888. Setup Complete.");
}

void Injector::sendGuiTelemetry(void){
    if (!guiDiscovered) return;
    
    float displayTorque0 = getSmoothedTorqueEWMA(&ConnectorM0, &smoothedTorqueValue0, &firstTorqueReading0);
    float displayTorque1 = getSmoothedTorqueEWMA(&ConnectorM1, &smoothedTorqueValue1, &firstTorqueReading1);
    float displayTorque2 = getSmoothedTorqueEWMA(&ConnectorM2, &smoothedTorqueValue2, &firstTorqueReading2);
    float displayTorque3 = getSmoothedTorqueEWMA(&ConnectorM3, &smoothedTorqueValue3, &firstTorqueReading3);
    
    int hlfb0_val = (int)ConnectorM0.HlfbState();
    int hlfb1_val = (int)ConnectorM1.HlfbState();
    int hlfb2_val = (int)ConnectorM2.HlfbState();
    int hlfb3_val = (int)ConnectorM3.HlfbState();

    long current_pos_steps_m0 = ConnectorM0.PositionRefCommanded();
    long current_pos_steps_m1 = ConnectorM1.PositionRefCommanded();
    long current_pos_steps_m2 = ConnectorM2.PositionRefCommanded();
    long current_pos_steps_m3 = ConnectorM3.PositionRefCommanded();

    float pos_mm_m0 = (float)current_pos_steps_m0 / STEPS_PER_MM_INJECTOR;
    float pos_mm_m1 = (float)current_pos_steps_m1 / STEPS_PER_MM_INJECTOR;
    float pos_mm_m2 = (float)current_pos_steps_m2 / STEPS_PER_MM_PINCH;
    float pos_mm_m3 = (float)current_pos_steps_m3 / STEPS_PER_MM_PINCH;

    int is_homed0 = homingMachineDone ? 1 : 0;
    int is_homed1 = homingMachineDone ? 1 : 0;
    int is_homed2 = homingPinchDone ? 1 : 0;
    int is_homed3 = homingPinchDone ? 1 : 0; // Both pinch motors are homed together

    float machine_pos_mm = (float)(current_pos_steps_m0 - machineHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;
    float cartridge_pos_mm = (float)(current_pos_steps_m0 - cartridgeHomeReferenceSteps) / STEPS_PER_MM_INJECTOR;

    float current_dispensed_for_telemetry = 0.0f;
    float current_target_for_telemetry = 0.0f;
    if (active_dispense_INJECTION_ongoing) {
        current_target_for_telemetry = active_op_target_ml;
        if (feedState == FEED_INJECT_ACTIVE || feedState == FEED_PURGE_ACTIVE ||
        feedState == FEED_INJECT_RESUMING || feedState == FEED_PURGE_RESUMING ) {
            if (active_op_steps_per_ml > 0.0001f) {
                long total_steps_moved_for_op = current_pos_steps_m0 - active_op_initial_axis_steps;
                current_dispensed_for_telemetry = fabs(total_steps_moved_for_op) / active_op_steps_per_ml;
            }
            } else if (feedState == FEED_INJECT_PAUSED || feedState == FEED_PURGE_PAUSED) {
            current_dispensed_for_telemetry = active_op_total_dispensed_ml;
        }
        } else {
        current_dispensed_for_telemetry = last_completed_dispense_ml;
        current_target_for_telemetry = 0.0f;
    }

    char torque0Str[16], torque1Str[16], torque2Str[16], torque3Str[16];
    if (displayTorque0 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque0Str, "0.0"); } else { snprintf(torque0Str, sizeof(torque0Str), "%.2f", displayTorque0); }
    if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque1Str, "0.0"); } else { snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1); }
    if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque2Str, "0.0"); } else { snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2); }
    if (displayTorque3 == TORQUE_SENTINEL_INVALID_VALUE) { strcpy(torque3Str, "0.0"); } else { snprintf(torque3Str, sizeof(torque3Str), "%.2f", displayTorque3); }

    // Assemble telemetry message into buffer, now with the required prefix
    snprintf(telemetryBuffer, sizeof(telemetryBuffer),
    TELEM_PREFIX_GUI
    "MAIN_STATE:%s,HOMING_STATE:%s,HOMING_PHASE:%s,FEED_STATE:%s,ERROR_STATE:%s,"
    "torque0:%s,hlfb0:%d,enabled0:%d,pos_mm0:%.2f,homed0:%d,"
    "torque1:%s,hlfb1:%d,enabled1:%d,pos_mm1:%.2f,homed1:%d,"
    "torque2:%s,hlfb2:%d,enabled2:%d,pos_mm2:%.2f,homed2:%d,"
    "torque3:%s,hlfb3:%d,enabled3:%d,pos_mm3:%.2f,homed3:%d,"
    "machine_mm:%.2f,cartridge_mm:%.2f,"
    "dispensed_ml:%.2f,target_ml:%.2f,"
    "peer_disc:%d,peer_ip:%s,"
    "temp_c:%.1f,vacuum:%d,vacuum_psig:%.2f,heater_mode:%s,"
    "pid_setpoint:%.1f,pid_kp:%.2f,pid_ki:%.2f,pid_kd:%.2f,pid_output:%.1f",
    mainStateStr(), homingStateStr(), homingPhaseStr(), feedStateStr(), errorStateStr(),
    torque0Str, hlfb0_val, (int)ConnectorM0.StatusReg().bit.Enabled, pos_mm_m0, is_homed0,
    torque1Str, hlfb1_val, (int)ConnectorM1.StatusReg().bit.Enabled, pos_mm_m1, is_homed1,
    torque2Str, hlfb2_val, (int)ConnectorM2.StatusReg().bit.Enabled, pos_mm_m2, is_homed2,
    torque3Str, hlfb3_val, (int)ConnectorM3.StatusReg().bit.Enabled, pos_mm_m3, is_homed3,
    machine_pos_mm, cartridge_pos_mm,
    current_dispensed_for_telemetry, current_target_for_telemetry,
    (int)peerDiscovered, peerIp.StringValue(),
    temperatureCelsius, (int)vacuumOn, vacuumPressurePsig, heaterStateStr(),
    pid_setpoint, pid_kp, pid_ki, pid_kd, pid_output);

    // Enqueue the telemetry message instead of sending directly
    enqueueTx(telemetryBuffer, guiIp, guiPort);
}

UserCommand Injector::parseCommand(const char *msg) {
    if (strcmp(msg, CMD_STR_REQUEST_TELEM) == 0) return CMD_REQUEST_TELEM;
    if (strncmp(msg, CMD_STR_DISCOVER, strlen(CMD_STR_DISCOVER)) == 0) return CMD_DISCOVER;
    if (strcmp(msg, CMD_STR_ENABLE) == 0) return CMD_ENABLE;
    if (strcmp(msg, CMD_STR_DISABLE) == 0) return CMD_DISABLE;
    if (strcmp(msg, CMD_STR_ABORT) == 0) return CMD_ABORT;
    if (strcmp(msg, CMD_STR_CLEAR_ERRORS) == 0) return CMD_CLEAR_ERRORS;
    if (strncmp(msg, CMD_STR_SET_INJECTOR_TORQUE_OFFSET, strlen(CMD_STR_SET_INJECTOR_TORQUE_OFFSET)) == 0) return CMD_SET_TORQUE_OFFSET;
    if (strncmp(msg, CMD_STR_JOG_MOVE, strlen(CMD_STR_JOG_MOVE)) == 0) return CMD_JOG_MOVE;
    if (strcmp(msg, CMD_STR_MACHINE_HOME_MOVE) == 0) return CMD_MACHINE_HOME_MOVE;
    if (strcmp(msg, CMD_STR_CARTRIDGE_HOME_MOVE) == 0) return CMD_CARTRIDGE_HOME_MOVE;
    if (strncmp(msg, CMD_STR_INJECT_MOVE, strlen(CMD_STR_INJECT_MOVE)) == 0) return CMD_INJECT_MOVE;
    if (strncmp(msg, CMD_STR_PURGE_MOVE, strlen(CMD_STR_PURGE_MOVE)) == 0) return CMD_PURGE_MOVE;
    if (strcmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_HOME) == 0) return CMD_MOVE_TO_CARTRIDGE_HOME;
    if (strncmp(msg, CMD_STR_MOVE_TO_CARTRIDGE_RETRACT, strlen(CMD_STR_MOVE_TO_CARTRIDGE_RETRACT)) == 0) return CMD_MOVE_TO_CARTRIDGE_RETRACT;
    if (strcmp(msg, CMD_STR_PAUSE_INJECTION) == 0) return CMD_PAUSE_INJECTION;
    if (strcmp(msg, CMD_STR_RESUME_INJECTION) == 0) return CMD_RESUME_INJECTION;
    if (strcmp(msg, CMD_STR_CANCEL_INJECTION) == 0) return CMD_CANCEL_INJECTION;
    if (strcmp(msg, CMD_STR_PINCH_HOME_MOVE) == 0) return CMD_PINCH_HOME_MOVE;
    if (strncmp(msg, CMD_STR_PINCH_JOG_MOVE, strlen(CMD_STR_PINCH_JOG_MOVE)) == 0) return CMD_PINCH_JOG_MOVE;
    if (strcmp(msg, CMD_STR_ENABLE_PINCH) == 0) return CMD_ENABLE_PINCH;
    if (strcmp(msg, CMD_STR_DISABLE_PINCH) == 0) return CMD_DISABLE_PINCH;
    if (strncmp(msg, CMD_STR_SET_PEER_IP, strlen(CMD_STR_SET_PEER_IP)) == 0) return CMD_SET_PEER_IP;
    if (strcmp(msg, CMD_STR_CLEAR_PEER_IP) == 0) return CMD_CLEAR_PEER_IP;
    if (strcmp(msg, CMD_STR_HEATER_ON) == 0) return CMD_HEATER_ON;
    if (strcmp(msg, CMD_STR_HEATER_OFF) == 0) return CMD_HEATER_OFF;
    if (strcmp(msg, CMD_STR_VACUUM_ON) == 0) return CMD_VACUUM_ON;
    if (strcmp(msg, CMD_STR_VACUUM_OFF) == 0) return CMD_VACUUM_OFF;
    if (strncmp(msg, CMD_STR_SET_HEATER_GAINS, strlen(CMD_STR_SET_HEATER_GAINS)) == 0) return CMD_SET_HEATER_GAINS;
    if (strncmp(msg, CMD_STR_SET_HEATER_SETPOINT, strlen(CMD_STR_SET_HEATER_SETPOINT)) == 0) return CMD_SET_HEATER_SETPOINT;
    if (strcmp(msg, CMD_STR_HEATER_PID_ON) == 0) return CMD_HEATER_PID_ON;
    if (strcmp(msg, CMD_STR_HEATER_PID_OFF) == 0) return CMD_HEATER_PID_OFF;

    return CMD_UNKNOWN;
}


void Injector::handleMessage(const Message& msg) {
    UserCommand command = parseCommand(msg.buffer);

    switch (command) {
        case CMD_REQUEST_TELEM:             sendGuiTelemetry(); break;
        case CMD_DISCOVER: {
            char* portStr = strstr(msg.buffer, "PORT=");
            if (portStr) {
                guiIp = msg.remoteIp;
                guiPort = atoi(portStr + 5);
                guiDiscovered = true;
                sendStatus(STATUS_PREFIX_DISCOVERY, "INJECTOR DISCOVERED");
            }
            break;
        }
        case CMD_ENABLE:                    handleEnable(); break;
        case CMD_DISABLE:                   handleDisable(); break;
        case CMD_ABORT:                     handleAbort(); break;
        case CMD_CLEAR_ERRORS:              handleClearErrors(); break;
        case CMD_SET_TORQUE_OFFSET:         handleSetinjectorMotorsTorqueOffset(msg.buffer); break;
        case CMD_JOG_MOVE:                  handleJogMove(msg.buffer); break;
        case CMD_MACHINE_HOME_MOVE:         handleMachineHomeMove(); break;
        case CMD_CARTRIDGE_HOME_MOVE:       handleCartridgeHomeMove(); break;
        case CMD_INJECT_MOVE:               handleInjectMove(msg.buffer); break;
        case CMD_PURGE_MOVE:                handlePurgeMove(msg.buffer); break;
        case CMD_MOVE_TO_CARTRIDGE_HOME:    handleMoveToCartridgeHome(); break;
        case CMD_MOVE_TO_CARTRIDGE_RETRACT: handleMoveToCartridgeRetract(msg.buffer); break;
        case CMD_PAUSE_INJECTION:           handlePauseOperation(); break;
        case CMD_RESUME_INJECTION:          handleResumeOperation(); break;
        case CMD_CANCEL_INJECTION:          handleCancelOperation(); break;
        case CMD_PINCH_HOME_MOVE:           handlePinchHomeMove(); break;
        case CMD_PINCH_JOG_MOVE:            handlePinchJogMove(msg.buffer); break;
        case CMD_ENABLE_PINCH:              handleEnablePinch(); break;
        case CMD_DISABLE_PINCH:             handleDisablePinch(); break;
        case CMD_SET_PEER_IP:               handleSetPeerIp(msg.buffer); break;
        case CMD_CLEAR_PEER_IP:             handleClearPeerIp(); break;
        case CMD_HEATER_ON:                 handleHeaterOn(); break;
        case CMD_HEATER_OFF:                handleHeaterOff(); break;
        case CMD_VACUUM_ON:                 handleVacuumOn(); break;
        case CMD_VACUUM_OFF:                handleVacuumOff(); break;
        case CMD_SET_HEATER_GAINS:          handleSetHeaterGains(msg.buffer); break;
        case CMD_SET_HEATER_SETPOINT:       handleSetHeaterSetpoint(msg.buffer); break;
        case CMD_HEATER_PID_ON:             handleHeaterPidOn(); break;
        case CMD_HEATER_PID_OFF:            handleHeaterPidOff(); break;

        case CMD_UNKNOWN:
        default:
        break;
    }
}

void Injector::handleSetPeerIp(const char* msg) {
    const char* ipStr = msg + strlen(CMD_STR_SET_PEER_IP);
    IpAddress newPeerIp(ipStr);
    if (strcmp(newPeerIp.StringValue(), "0.0.0.0") == 0) {
        peerDiscovered = false;
        sendStatus(STATUS_PREFIX_ERROR, "Failed to parse peer IP address");
        } else {
        peerIp = newPeerIp;
        peerDiscovered = true;
        char response[100];
        snprintf(response, sizeof(response), "Peer IP set to %s", ipStr);
        sendStatus(STATUS_PREFIX_INFO, response);
    }
}

void Injector::handleClearPeerIp() {
    peerDiscovered = false;
    sendStatus(STATUS_PREFIX_INFO, "Peer IP cleared");
}
