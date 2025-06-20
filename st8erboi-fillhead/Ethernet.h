#pragma once

#include "ClearCore.h"
#include "EthernetUdp.h"
#include "IpAddress.h"
#include "InjectorStates.h"

extern EthernetUdp Udp;
extern unsigned char packetBuffer[];
extern IpAddress terminalIp;
extern uint16_t terminalPort;
extern bool terminalDiscovered;
extern const uint16_t MAX_PACKET_LENGTH;

typedef enum {
	USER_CMD_UNKNOWN = 0,
	USER_CMD_DISCOVER_TELEM,
	USER_CMD_ENABLE,
	USER_CMD_DISABLE,
	USER_CMD_ABORT,
	USER_CMD_CLEAR_ERRORS,
	USER_CMD_STANDBY_MODE,
	USER_CMD_TEST_MODE,
	USER_CMD_JOG_MODE,
	USER_CMD_HOMING_MODE,
	USER_CMD_FEED_MODE,
	USER_CMD_SET_TORQUE_OFFSET,
	USER_CMD_JOG_MOVE,
	USER_CMD_MACHINE_HOME_MOVE,
	USER_CMD_CARTRIDGE_HOME_MOVE,
	USER_CMD_INJECT_MOVE,
	USER_CMD_PURGE_MOVE,
	USER_CMD_MOVE_TO_CARTRIDGE_HOME,
	USER_CMD_MOVE_TO_CARTRIDGE_RETRACT,
	USER_CMD_PAUSE_INJECTION,
	USER_CMD_RESUME_INJECTION,
	USER_CMD_CANCEL_INJECTION,
	USER_CMD_PINCH_HOME_MOVE,
	USER_CMD_PINCH_OPEN,
	USER_CMD_PINCH_CLOSE,
	USER_CMD_HOME_X,
	USER_CMD_HOME_Y,
	USER_CMD_HOME_Z,
	USER_CMD_START_CYCLE,
	USER_CMD_START_CYCLE,
	USER_CMD_COUNT
} UserCommand;

typedef enum {
	FILLHEAD_CMD_UNKNOWN = 0,
	FILLHEAD_CMD_HOME_X,
	FILLHEAD_CMD_HOME_Y,
	FILLHEAD_CMD_HOME_Z,
	FILLHEAD_CMD_MOVE_X,
	FILLHEAD_CMD_MOVE_Y,
	FILLHEAD_CMD_MOVE_Z,
	FILLHEAD_CMD_COUNT
} FillheadCommand;

// Function Prototypes
UserCommand parseUserCommand(const char *msg);

void SetupEthernet(void);
void SetupUsbSerial(void);
void checkUdpBuffer(Injector *injector);
void sendToPC(const char *msg);
void handleDiscoveryTelemPacket(const char *msg, IpAddress senderIp);

void sendTelem(Injector *injector);
void handleMessage(const char *msg, Injector *injector);