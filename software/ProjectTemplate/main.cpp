#include "ClearCore.h"
#include "EthernetUdp.h"
#include <cmath>

#define LOCAL_PORT          8888
#define MAX_PACKET_LENGTH   100

// --- For Torque Smoothing (EWMA) ---
#define EWMA_ALPHA 0.2f
float smoothedTorqueValue = 0.0f;
bool firstTorqueReading = true;
#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f
// --- END NEW ---

// --- Torque Limit Variable ---
float torqueAbortLimit = 100.0f;
// --- END NEW ---

// --- NEW: Custom Commanded Step Counter ---
int32_t customCommandedStepCounter = 0; // Our new, user-resettable counter
// --- END NEW ---

EthernetUdp Udp;
unsigned char packetBuffer[MAX_PACKET_LENGTH];
IpAddress terminalIp;
uint16_t terminalPort = 0;
bool terminalDiscovered = false;

uint32_t pulsesPerRev = 6400;
uint32_t lastTorqueTime = 0;
const uint32_t torqueInterval = 20;
bool motorIsEnabled = false;

// Function prototypes
void SetupUsbSerial();
void SetupEthernet();
void SetupMotor();
void sendToPC(const char *msg);
void sendTorqueDebug();
void handleDiscoveryPacket(const char *msg, IpAddress senderIp);
void moveWithSpeed(bool useAltSpeed, int steps, const char *label);
void handleRevCommand(const char *msg);
void handleFastCommand(const char *msg);
void handleSlowCommand(const char *msg);
void handlePprCommand(const char *msg);
void handleSetTorqueLimit(const char *msg);
void resetMotor();
void attemptMotorReEnable(const char* reason_message);
void disableMotorAndNotify(const char* reason_message);
// --- NEW: Function to reset custom counter ---
void resetCustomStepCounter();
// --- END NEW ---
void handleUdpMessage(const char *msg);
void checkUdpDiscovery();
void checkTorqueLimit();


float getSmoothedTorqueEWMA() {
	float currentRawTorque = ConnectorM0.HlfbPercent();

	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return TORQUE_SENTINEL_INVALID_VALUE;
	}

	if (firstTorqueReading) {
		smoothedTorqueValue = currentRawTorque;
		firstTorqueReading = false;
		} else {
		smoothedTorqueValue = EWMA_ALPHA * currentRawTorque + (1.0f - EWMA_ALPHA) * smoothedTorqueValue;
	}
	return smoothedTorqueValue;
}

void SetupUsbSerial() {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(9600);
	ConnectorUsb.PortOpen();
	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);
}

void SetupEthernet() {
	EthernetMgr.Setup();
	if (!EthernetMgr.DhcpBegin()) while (true);
	while (!EthernetMgr.PhyLinkActive()) Delay_ms(1000);
	Udp.Begin(LOCAL_PORT);
}

void SetupMotor() {
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);
	ConnectorM0.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM0.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM0.VelMax(INT32_MAX);
	ConnectorM0.AccelMax(INT32_MAX);
	ConnectorM0.EnableRequest(true);
	motorIsEnabled = true;
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) Delay_ms(100);
}

void sendToPC(const char *msg) {
	if (!terminalDiscovered) return;
	Udp.Connect(terminalIp, terminalPort);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}

// --- MODIFIED: sendTorqueDebug to include custom counter ---
void sendTorqueDebug() {
	float displayTorque = getSmoothedTorqueEWMA();
	int hlfb = (int)ConnectorM0.HlfbState();
	// int32_t commandedPosition = ConnectorM0.PositionRefCommanded(); // Original: now optional

	char msg[128];

	if (displayTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		// Send both original commanded position and new custom counter
		snprintf(msg, sizeof(msg), "torque: ---, hlfb: %d, enabled: %d, pos_cmd: %ld, custom_pos: %ld",
		hlfb, motorIsEnabled ? 1 : 0, ConnectorM0.PositionRefCommanded(), customCommandedStepCounter);
		} else {
		// Send both original commanded position and new custom counter
		snprintf(msg, sizeof(msg), "torque: %.2f%%, hlfb: %d, enabled: %d, pos_cmd: %ld, custom_pos: %ld",
		displayTorque, hlfb, motorIsEnabled ? 1 : 0, ConnectorM0.PositionRefCommanded(), customCommandedStepCounter);
	}

	sendToPC(msg);
}
// --- END MODIFIED ---

void handleDiscoveryPacket(const char *msg, IpAddress senderIp) {
	char *portStr = strstr(msg, "PORT=");
	if (portStr) {
		terminalPort = atoi(portStr + 5);
		terminalIp = senderIp;
		terminalDiscovered = true;
		sendToPC("CLEARCORE_ACK");
	}
}

// --- MODIFIED: moveWithSpeed to update custom counter ---
void moveWithSpeed(bool useAltSpeed, int steps, const char *label) {
	if (!motorIsEnabled) {
		sendToPC("MOVE BLOCKED: Motor is disabled");
		return;
	}

	if (useAltSpeed) {
		ConnectorM0.EnableTriggerPulse(1, 25, true);
		Delay_ms(5);
	}

	ConnectorM0.Move(steps);
	customCommandedStepCounter += steps; // Update our custom counter
	Delay_ms(2);

	while (!ConnectorM0.StepsComplete() || ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) {
		checkUdpDiscovery();
		checkTorqueLimit();
		uint32_t now = Milliseconds();
		if (now - lastTorqueTime >= torqueInterval) {
			sendTorqueDebug();
			lastTorqueTime = now;
		}

		if (!motorIsEnabled) {
			sendToPC("MOVE ABORTED: motor disabled during move");
			return;
		}
	}

	sendToPC(label);
}
// --- END MODIFIED ---

void handleRevCommand(const char *msg) {
	int revs = atoi(msg + 4);
	moveWithSpeed(false, revs * pulsesPerRev, "rev move");
}

void handleFastCommand(const char *msg) {
	int revs = atoi(msg + 5);
	moveWithSpeed(false, revs * pulsesPerRev, "fast move");
}

void handleSlowCommand(const char *msg) {
	int revs = atoi(msg + 5);
	moveWithSpeed(true, revs * pulsesPerRev, "slow move");
}

void handlePprCommand(const char *msg) {
	int val = atoi(msg + 4);
	if (val == 6400 || val == 800) {
		pulsesPerRev = val;
		sendToPC("ppr updated");
		} else {
		sendToPC("invalid ppr value");
	}
}

void handleSetTorqueLimit(const char *msg) {
	char *limitStr = strstr(msg, " ");
	if (limitStr) {
		float newLimit = atof(limitStr + 1);
		if (newLimit >= 0.0f && newLimit <= 100.0f) {
			torqueAbortLimit = newLimit;
			char response[64];
			snprintf(response, sizeof(response), "torque limit set %.1f", torqueAbortLimit);
			sendToPC(response);
			} else {
			sendToPC("invalid torque limit value");
		}
		} else {
		sendToPC("malformed torque limit command");
	}
}

// --- NEW: Function to reset custom counter ---
void resetCustomStepCounter() {
	customCommandedStepCounter = 0;
}
// --- END NEW ---

void attemptMotorReEnable(const char* reason_message) {
	ConnectorM0.EnableRequest(true);
	motorIsEnabled = true;
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) Delay_ms(50);
	sendToPC(reason_message);
	smoothedTorqueValue = 0.0f;
	firstTorqueReading = true;
}

void disableMotorAndNotify(const char* reason_message) {
	if (!motorIsEnabled) return;
	ConnectorM0.EnableRequest(false);
	motorIsEnabled = false;
	sendToPC(reason_message);
	Delay_ms(50);
}

// resetMotor clears custom commanded position
void resetMotor() {
	disableMotorAndNotify("motor reset initiated");
	ConnectorM0.ClearAlerts();
	resetCustomStepCounter(); // Clear our custom counter on full reset
	attemptMotorReEnable("motor reset complete");
}

void handleUdpMessage(const char *msg) {
	if (strncmp(msg, "DISCOVER_CLEARCORE", 18) == 0) {
		handleDiscoveryPacket(msg, Udp.RemoteIp());
		} else if (strncmp(msg, "REV ", 4) == 0) {
		handleRevCommand(msg);
		} else if (strncmp(msg, "FAST ", 5) == 0) {
		handleFastCommand(msg);
		} else if (strncmp(msg, "SLOW ", 5) == 0) {
		handleSlowCommand(msg);
		} else if (strncmp(msg, "PPR ", 4) == 0) {
		handlePprCommand(msg);
		} else if (strncmp(msg, "SET_TORQUE_LIMIT ", 17) == 0) {
		handleSetTorqueLimit(msg);
		} else if (strcmp(msg, "RESET") == 0) {
		resetMotor();
		} else if (strcmp(msg, "DISABLE") == 0) {
		disableMotorAndNotify("MOTOR DISABLED");
		smoothedTorqueValue = 0.0f;
		firstTorqueReading = true;
		} else if (strcmp(msg, "ENABLE") == 0) {
		attemptMotorReEnable("MOTOR ENABLED");
		// ABORT clears custom commanded position
		} else if (strcmp(msg, "ABORT") == 0) {
		disableMotorAndNotify("MOTOR DISABLED VIA ABORT");
		resetCustomStepCounter(); // Clear our custom counter on manual abort
		Delay_ms(200);
		attemptMotorReEnable("MOTOR RE-ENABLED AFTER MANUAL ABORT");
		} else if (strcmp(msg, "PING") == 0) {
		sendToPC("PONG");
	}
}

void checkUdpDiscovery() {
	uint16_t packetSize = Udp.PacketParse();
	if (packetSize > 0) {
		int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		packetBuffer[bytesRead] = '\0';
		handleUdpMessage((char *)packetBuffer);
	}
}

// checkTorqueLimit clears custom commanded position on auto-abort
void checkTorqueLimit() {
	if (motorIsEnabled) {
		float currentSmoothedTorque = getSmoothedTorqueEWMA();
		if (currentSmoothedTorque != TORQUE_SENTINEL_INVALID_VALUE &&
		currentSmoothedTorque > torqueAbortLimit) {
			disableMotorAndNotify("TORQUE LIMIT EXCEEDED");
			resetCustomStepCounter(); // Clear our custom counter on torque limit abort
			Delay_ms(200);
			attemptMotorReEnable("MOTOR RE-ENABLED AFTER TORQUE ABORT");
		}
	}
}

int main() {
	SetupUsbSerial();
	SetupEthernet();
	SetupMotor();

	torqueAbortLimit = 100.0f;
	customCommandedStepCounter = 0; // Initialize custom counter on boot

	while (true) {
		checkUdpDiscovery();
		checkTorqueLimit();
		uint32_t now = Milliseconds();
		if (terminalDiscovered && now - lastTorqueTime >= torqueInterval) {
			sendTorqueDebug();
			lastTorqueTime = now;
		}
		Delay_ms(1);
	}
}