#pragma once

#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "states.h"

extern EthernetUdp Udp;
extern unsigned char packetBuffer[];
extern IpAddress terminalIp;
extern uint16_t terminalPort;
extern bool terminalDiscovered;
extern const uint16_t MAX_PACKET_LENGTH;

typedef enum {
	CMD_UNKNOWN = 0, CMD_DISCOVER_TELEM, CMD_ENABLE, CMD_DISABLE, CMD_ABORT, CMD_CLEAR_ERRORS,
	CMD_STANDBY_MODE, CMD_TEST_MODE, CMD_JOG_MODE, CMD_HOMING_MODE, CMD_FEED_MODE,
	CMD_SET_TORQUE_OFFSET, CMD_JOG_MOVE, CMD_MACHINE_HOME_MOVE, CMD_CARTRIDGE_HOME_MOVE,
	CMD_INJECT_MOVE, CMD_PURGE_MOVE,
	CMD_MOVE_TO_CARTRIDGE_HOME, CMD_MOVE_TO_CARTRIDGE_RETRACT,
	CMD_PAUSE_OPERATION, CMD_RESUME_OPERATION, CMD_CANCEL_OPERATION,
	CMD_COUNT
} MessageCommand;

// Function Prototypes
MessageCommand parseMessageCommand(const char *msg);
void handleMessage(const char *msg, SystemStates *states);

void SetupEthernet(void);
void checkUdpBuffer(SystemStates *states);
void sendTelem(SystemStates *states);
void sendToPC(const char *msg);
void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp, SystemStates *states);

void handleEnable(SystemStates *states);
void handleDisable(SystemStates *states);
void handleAbort(SystemStates *states); // Will call finalizeAndResetActiveDispenseOperation
void handleClearErrors(SystemStates *states);

void handleStandbyMode(SystemStates *states); // Will call finalizeAndResetActiveDispenseOperation
void handleTestMode(SystemStates *states);    // Will call finalizeAndResetActiveDispenseOperation
void handleJogMode(SystemStates *states);     // Will call finalizeAndResetActiveDispenseOperation
void handleHomingMode(SystemStates *states);  // Will call finalizeAndResetActiveDispenseOperation
void handleFeedMode(SystemStates *states);    // Mode entry

void handleSetTorqueOffset(const char *msg);
void handleJogMove(const char *msg, SystemStates *states);
void handleMachineHomeMove(const char *msg, SystemStates *states);
void handleCartridgeHomeMove(const char *msg, SystemStates *states);
void handleInjectMove(const char *msg, SystemStates *states);
void handlePurgeMove(const char *msg, SystemStates *states);
void handleMoveToCartridgeHome(SystemStates *states);
void handleMoveToCartridgeRetract(const char *msg, SystemStates *states);
void handlePauseOperation(SystemStates *states);
void handleResumeOperation(SystemStates *states);
void handleCancelOperation(SystemStates *states); // Will call finalizeAndResetActiveDispenseOperation

// Helper function prototypes if they are called from outside messages.cpp (e.g. main.cpp)
// If they are only static/internal to messages.cpp, they don't need to be here.
// Based on previous main.cpp, finalizeAndResetActiveDispenseOperation was called from main.cpp
void finalizeAndResetActiveDispenseOperation(SystemStates *states, bool operationCompletedSuccessfully);
void fullyResetActiveDispenseOperation(SystemStates *states); // This one was used by handlers within messages.cpp