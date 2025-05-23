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

typedef enum
{
	CMD_UNKNOWN = 0,
	CMD_DISCOVER_TELEM,
	CMD_SET_TORQUE_LIMIT,
	CMD_SET_TORQUE_OFFSET,
	CMD_DISABLE,
	CMD_ENABLE,
	CMD_ABORT,
	CMD_JOG_MODE,
	CMD_JOG_UP,
	CMD_JOG_DOWN,
	CMD_STANDBY,
	CMD_HOMING_MACHINE,
	CMD_HOMING_CARTRIDGE,
	CMD_FEED,
	CMD_MACHINE_HOME,
	CMD_CARTRIDGE_HOME,
	CMD_COUNT
} MessageCommand;

MessageCommand parseMessageCommand(const char *msg);

void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp, SystemStates *states);
void handleSetTorqueLimit(const char *msg);
void handleSetTorqueOffset(const char *msg);
void handleDisable(SystemStates *states);
void handleEnable(SystemStates *states);
void handleAbort(SystemStates *states);
void handleJogMode(SystemStates *states);
void handleJogUp(const char *msg, SystemStates *states);
void handleJogDown(const char *msg, SystemStates *states);
void handleStandby(SystemStates *states);
void handleHomingMachine(SystemStates *states);
void handleHomingCartridge(SystemStates *states);
void handleFeed(SystemStates *states);
void handleMachineHome(void);
void handleCartridgeHome(void);
void sendToPC(const char *msg);
void SetupUsbSerial(void);
void SetupEthernet(void);

void handleMessage(const char *msg, SystemStates *states);
void checkUdpBuffer(SystemStates *states);

// Only send when needed, not on a timer!
void sendTelem(SystemStates *states);
