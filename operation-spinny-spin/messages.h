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
	CMD_UNKNOWN = 0,
	CMD_DISCOVER_TELEM,
	CMD_ENABLE,
	CMD_DISABLE,
	CMD_ABORT,
	CMD_CLEAR_ERRORS,
	CMD_STANDBY_MODE,
	CMD_TEST_MODE,
	CMD_JOG_MODE,
	CMD_HOMING_MODE,
	CMD_FEED_MODE,
	CMD_SET_TORQUE_OFFSET,
	CMD_JOG_MOVE,
	CMD_MACHINE_HOME_MOVE,
	CMD_CARTRIDGE_HOME_MOVE,
	CMD_INJECT_MOVE,
	CMD_PURGE_MOVE,
	CMD_MOVE_TO_CARTRIDGE_HOME,
	CMD_MOVE_TO_CARTRIDGE_RETRACT,
	CMD_PAUSE_OPERATION,
	CMD_RESUME_OPERATION,
	CMD_CANCEL_OPERATION,
	CMD_DAVID_STATE,
	CMD_COUNT
} MessageCommand;

// Function Prototypes
MessageCommand parseMessageCommand(const char *msg);
void handleMessage(const char *msg, Spinny_Spin_Machine *machine_1);

void SetupEthernet(void);
void SetupUsbSerial(void);
void checkUdpBuffer(Spinny_Spin_Machine *machine_1);
void sendTelem(Spinny_Spin_Machine *machine_1);
void sendToPC(const char *msg);
void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp, Spinny_Spin_Machine *machine_1);

void handleEnable(Spinny_Spin_Machine *machine_1);
void handleDisable(Spinny_Spin_Machine *machine_1);
void handleAbort(Spinny_Spin_Machine *machine_1); // Will call finalizeAndResetActiveDispenseOperation
void handleClearErrors(Spinny_Spin_Machine *machine_1);

void handleStandbyMode(Spinny_Spin_Machine *machine_1); // Will call finalizeAndResetActiveDispenseOperation
void handleTestMode(Spinny_Spin_Machine *machine_1);    // Will call finalizeAndResetActiveDispenseOperation
void handleJogMode(Spinny_Spin_Machine *machine_1);     // Will call finalizeAndResetActiveDispenseOperation
void handleHomingMode(Spinny_Spin_Machine *machine_1);  // Will call finalizeAndResetActiveDispenseOperation
void handleFeedMode(Spinny_Spin_Machine *machine_1);    // Mode entry

void handleSetTorqueOffset(const char *msg);
void handleJogMove(const char *msg, Spinny_Spin_Machine *machine_1);
void handleMachineHomeMove(const char *msg, Spinny_Spin_Machine *machine_1);
void handleCartridgeHomeMove(const char *msg, Spinny_Spin_Machine *machine_1);
void handleInjectMove(const char *msg, Spinny_Spin_Machine *machine_1);
void handlePurgeMove(const char *msg, Spinny_Spin_Machine *machine_1);
void handleMoveToCartridgeHome(Spinny_Spin_Machine *machine_1);
void handleMoveToCartridgeRetract(const char *msg, Spinny_Spin_Machine *machine_1);
void handlePauseOperation(Spinny_Spin_Machine *machine_1);
void handleResumeOperation(Spinny_Spin_Machine *machine_1);
void handleCancelOperation(Spinny_Spin_Machine *machine_1); // Will call finalizeAndResetActiveDispenseOperation

void handleDavidMode(Spinny_Spin_Machine *machine_1);

// Helper function prototypes if they are called from outside messages.cpp (e.g. main.cpp)
// If they are only static/internal to messages.cpp, they don't need to be here.
// Based on previous main.cpp, finalizeAndResetActiveDispenseOperation was called from main.cpp
void finalizeAndResetActiveDispenseOperation(Spinny_Spin_Machine *machine_1, bool operationCompletedSuccessfully);
void fullyResetActiveDispenseOperation(Spinny_Spin_Machine *machine_1); // This one was used by handlers within messages.cpp