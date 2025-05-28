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

	// --- System & Connection Commands ---
	CMD_DISCOVER_TELEM,
	CMD_ENABLE,
	CMD_DISABLE,
	CMD_ABORT,

	// --- Mode Setting Commands ---
	CMD_STANDBY_MODE,
	CMD_TEST_MODE,
	CMD_JOG_MODE,
	CMD_HOMING_MODE,
	CMD_FEED_MODE,

	// --- Parameter Setting Commands ---
	CMD_SET_TORQUE_OFFSET,      // General torque offset value

	// --- Jog Operation Commands ---
	// Format: JOG_MOVE <s0_steps> <s1_steps> <torque_%> <vel_sps> <accel_sps2>  // << NEW FORMAT
	CMD_JOG_MOVE,

	// --- Homing Operation Commands ---
	// Format: <CMD_STR> <stroke_mm> <rapid_vel_mm_s> <touch_vel_mm_s> <retract_dist_mm> <torque_%>
	CMD_MACHINE_HOME_MOVE,
	CMD_CARTRIDGE_HOME_MOVE,

	// --- Feed Operation Commands ---
	// Format: <CMD_STR> <volume_ml> <speed_ml_s> <steps_per_ml_calc> <torque_%>
	CMD_INJECT_MOVE,
	CMD_PURGE_MOVE,
	CMD_RETRACT_MOVE,
	
	CMD_MOVE_TO_CARTRIDGE_HOME,     // New
	CMD_MOVE_TO_CARTRIDGE_RETRACT,  // New (will take an offset parameter)

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
void handleAbort(SystemStates *states);

void handleStandbyMode(SystemStates *states);
void handleTestMode(SystemStates *states);

// Mode Entry Handlers
void handleJogMode(SystemStates *states);
void handleHomingMode(SystemStates *states);
void handleFeedMode(SystemStates *states);

// Parameter Handlers
void handleSetTorqueOffset(const char *msg);

// Operation Handlers
void handleJogMove(const char *msg, SystemStates *states);
void handleMachineHomeMove(const char *msg, SystemStates *states);
void handleCartridgeHomeMove(const char *msg, SystemStates *states);
void handleInjectMove(const char *msg, SystemStates *states); // Renamed
void handlePurgeMove(const char *msg, SystemStates *states);
void handleRetractMove(const char *msg, SystemStates *states);
