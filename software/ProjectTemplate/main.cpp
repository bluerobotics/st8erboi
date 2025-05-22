#include "ClearCore.h"
#include "EthernetUdp.h"
#include <cmath>
#include <cstring>
#include <cstdlib>

#define LOCAL_PORT          8888
#define MAX_PACKET_LENGTH   100

#define EWMA_ALPHA 0.2f
float smoothedTorqueValue1 = 0.0f;
float smoothedTorqueValue2 = 0.0f;
bool firstTorqueReading1 = true;
bool firstTorqueReading2 = true;
#define TORQUE_SENTINEL_INVALID_VALUE -9999.0f

float torqueAbortLimit = 10.0f; // Default torque limit is 10%
float torqueOffset = -2.4f;     // Configurable, default -2.4

int32_t customCommandedStepCounter = 0;

EthernetUdp Udp;
unsigned char packetBuffer[MAX_PACKET_LENGTH];
IpAddress terminalIp;
uint16_t terminalPort = 0;
bool terminalDiscovered = false;

uint32_t pulsesPerRev = 6400;
uint32_t lastTorqueTime = 0;
const uint32_t torqueInterval = 20;
bool motorsAreEnabled = false;

void SetupUsbSerial();
void SetupEthernet();
void SetupMotors();
void sendToPC(const char *msg);
void sendTorqueDebug();
void handleDiscoveryPacket(const char *msg, IpAddress senderIp);
void moveWithSpeed(bool useAltSpeed, int steps, const char *label);
void handleRevCommand(const char *msg);
void handleFastCommand(const char *msg);
void handleSlowCommand(const char *msg);
void handlePprCommand(const char *msg);
void handleSetTorqueLimit(const char *msg);
void handleSetTorqueOffset(const char *msg);
void resetMotors();
void attemptMotorsReEnable(const char* reason_message);
void disableMotorsAndNotify(const char* reason_message);
void resetCustomStepCounter();
void handleUdpMessage(const char *msg);
void checkUdpDiscovery();
void checkTorqueLimit();
void handleJogCommand(const char *msg);

float getSmoothedTorqueEWMA(MotorDriver &motor, float &smoothedValue, bool &firstRead) {
	float currentRawTorque = motor.HlfbPercent();
	if (currentRawTorque == TORQUE_SENTINEL_INVALID_VALUE) {
		return TORQUE_SENTINEL_INVALID_VALUE;
	}
	if (firstRead) {
		smoothedValue = currentRawTorque;
		firstRead = false;
		} else {
		smoothedValue = EWMA_ALPHA * currentRawTorque + (1.0f - EWMA_ALPHA) * smoothedValue;
	}
	float adjusted = smoothedValue + torqueOffset;
	return adjusted; // now signed!
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

void SetupMotors() {
	MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);
	// Motor 1: M0
	ConnectorM0.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM0.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM0.VelMax(INT32_MAX);
	ConnectorM0.AccelMax(INT32_MAX);
	ConnectorM0.EnableRequest(true);
	// Motor 2: M1
	ConnectorM1.HlfbMode(MotorDriver::HLFB_MODE_HAS_BIPOLAR_PWM);
	ConnectorM1.HlfbCarrier(MotorDriver::HLFB_CARRIER_482_HZ);
	ConnectorM1.VelMax(INT32_MAX);
	ConnectorM1.AccelMax(INT32_MAX);
	ConnectorM1.EnableRequest(true);

	motorsAreEnabled = true;
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED ||
	ConnectorM1.HlfbState() != MotorDriver::HLFB_ASSERTED)
	Delay_ms(100);
}

void sendToPC(const char *msg) {
	if (!terminalDiscovered) return;
	Udp.Connect(terminalIp, terminalPort);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}

void sendTorqueDebug() {
	float displayTorque1 = getSmoothedTorqueEWMA(ConnectorM0, smoothedTorqueValue1, firstTorqueReading1);
	float displayTorque2 = getSmoothedTorqueEWMA(ConnectorM1, smoothedTorqueValue2, firstTorqueReading2);
	int hlfb1 = (int)ConnectorM0.HlfbState();
	int hlfb2 = (int)ConnectorM1.HlfbState();

	char torque1Str[16], torque2Str[16];
	if (displayTorque1 == TORQUE_SENTINEL_INVALID_VALUE) {
		strcpy(torque1Str, "---");
		} else {
		snprintf(torque1Str, sizeof(torque1Str), "%.2f", displayTorque1);
	}
	if (displayTorque2 == TORQUE_SENTINEL_INVALID_VALUE) {
		strcpy(torque2Str, "---");
		} else {
		snprintf(torque2Str, sizeof(torque2Str), "%.2f", displayTorque2);
	}

	char msg[200];
	snprintf(msg, sizeof(msg),
	"torque1: %s, hlfb1: %d, enabled1: %d, pos_cmd1: %ld, "
	"torque2: %s, hlfb2: %d, enabled2: %d, pos_cmd2: %ld, custom_pos: %ld",
	torque1Str,
	hlfb1,
	motorsAreEnabled ? 1 : 0,
	ConnectorM0.PositionRefCommanded(),
	torque2Str,
	hlfb2,
	motorsAreEnabled ? 1 : 0,
	ConnectorM1.PositionRefCommanded(),
	customCommandedStepCounter
	);
	sendToPC(msg);
}

void handleDiscoveryPacket(const char *msg, IpAddress senderIp) {
	char *portStr = strstr(msg, "PORT=");
	if (portStr) {
		terminalPort = atoi(portStr + 5);
		terminalIp = senderIp;
		terminalDiscovered = true;
		sendToPC("CLEARCORE_ACK");
	}
}

void moveWithSpeed(bool useAltSpeed, int steps, const char *label) {
	if (!motorsAreEnabled) {
		sendToPC("MOVE BLOCKED: Motors are disabled");
		return;
	}

	if (useAltSpeed) {
		ConnectorM0.EnableTriggerPulse(1, 25, true);
		ConnectorM1.EnableTriggerPulse(1, 25, true);
		Delay_ms(5);
	}

	ConnectorM0.Move(steps);
	ConnectorM1.Move(steps);
	customCommandedStepCounter += steps;
	Delay_ms(2);

	while ((!ConnectorM0.StepsComplete() || ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) ||
	(!ConnectorM1.StepsComplete() || ConnectorM1.HlfbState() != MotorDriver::HLFB_ASSERTED)) {
		checkUdpDiscovery();
		checkTorqueLimit();
		uint32_t now = Milliseconds();
		if (now - lastTorqueTime >= torqueInterval) {
			sendTorqueDebug();
			lastTorqueTime = now;
		}
		if (!motorsAreEnabled) {
			sendToPC("MOVE ABORTED: motors disabled during move");
			return;
		}
	}
	sendToPC(label);
}

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

// New: Parse "SET_TORQUE_OFFSET <val>"
void handleSetTorqueOffset(const char *msg) {
	char *offsetStr = strstr(msg, " ");
	if (offsetStr) {
		float newOffset = atof(offsetStr + 1);
		torqueOffset = newOffset;
		char response[64];
		snprintf(response, sizeof(response), "torque offset set %.2f", torqueOffset);
		sendToPC(response);
		} else {
		sendToPC("malformed torque offset command");
	}
}

void resetCustomStepCounter() {
	customCommandedStepCounter = 0;
}

void attemptMotorsReEnable(const char* reason_message) {
	ConnectorM0.EnableRequest(true);
	ConnectorM1.EnableRequest(true);
	motorsAreEnabled = true;
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED ||
	ConnectorM1.HlfbState() != MotorDriver::HLFB_ASSERTED)
	Delay_ms(50);
	sendToPC(reason_message);
	smoothedTorqueValue1 = 0.0f;
	smoothedTorqueValue2 = 0.0f;
	firstTorqueReading1 = true;
	firstTorqueReading2 = true;
}

void disableMotorsAndNotify(const char* reason_message) {
	if (!motorsAreEnabled) return;
	ConnectorM0.EnableRequest(false);
	ConnectorM1.EnableRequest(false);
	motorsAreEnabled = false;
	sendToPC(reason_message);
	Delay_ms(50);
}

void resetMotors() {
	disableMotorsAndNotify("motors reset initiated");
	ConnectorM0.ClearAlerts();
	ConnectorM1.ClearAlerts();
	resetCustomStepCounter();
	attemptMotorsReEnable("motors reset complete");
}

void handleJogCommand(const char *msg) {
	if (!motorsAreEnabled) {
		sendToPC("JOG IGNORED: Motors disabled");
		return;
	}
	// Format: "JOG M0 + 128"
	char motorName[3] = "";
	char dir = 0;
	int jogSteps = 128; // default
	int parsed = sscanf(msg, "JOG %2s %c %d", motorName, &dir, &jogSteps);
	MotorDriver *motor = nullptr;
	if (strncmp(motorName, "M0", 2) == 0) {
		motor = &ConnectorM0;
		} else if (strncmp(motorName, "M1", 2) == 0) {
		motor = &ConnectorM1;
		} else {
		sendToPC("JOG ERROR: Invalid motor");
		return;
	}
	if (parsed < 3) jogSteps = 128; // if not specified
	if (jogSteps < 1 || jogSteps > 100000) {
		sendToPC("JOG ERROR: Bad step value");
		return;
	}
	int steps = (dir == '+') ? jogSteps : (dir == '-' ? -jogSteps : 0);
	if (steps == 0) {
		sendToPC("JOG ERROR: Invalid direction");
		return;
	}
	motor->Move(steps);
	Delay_ms(2);
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
		} else if (strncmp(msg, "SET_TORQUE_OFFSET", 17) == 0) {
		handleSetTorqueOffset(msg);
		} else if (strcmp(msg, "RESET") == 0) {
		resetMotors();
		} else if (strcmp(msg, "DISABLE") == 0) {
		disableMotorsAndNotify("MOTORS DISABLED");
		smoothedTorqueValue1 = 0.0f;
		smoothedTorqueValue2 = 0.0f;
		firstTorqueReading1 = true;
		firstTorqueReading2 = true;
		} else if (strcmp(msg, "ENABLE") == 0) {
		attemptMotorsReEnable("MOTORS ENABLED");
		} else if (strcmp(msg, "ABORT") == 0) {
		disableMotorsAndNotify("MOTORS DISABLED VIA ABORT");
		resetCustomStepCounter();
		Delay_ms(200);
		attemptMotorsReEnable("MOTORS RE-ENABLED AFTER MANUAL ABORT");
		} else if (strcmp(msg, "PING") == 0) {
		sendToPC("PONG");
		} else if (strncmp(msg, "JOG ", 4) == 0) {
		handleJogCommand(msg);
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

void checkTorqueLimit() {
	if (motorsAreEnabled) {
		float currentSmoothedTorque1 = getSmoothedTorqueEWMA(ConnectorM0, smoothedTorqueValue1, firstTorqueReading1);
		float currentSmoothedTorque2 = getSmoothedTorqueEWMA(ConnectorM1, smoothedTorqueValue2, firstTorqueReading2);
		if ((currentSmoothedTorque1 != TORQUE_SENTINEL_INVALID_VALUE &&
		fabs(currentSmoothedTorque1) > torqueAbortLimit) ||
		(currentSmoothedTorque2 != TORQUE_SENTINEL_INVALID_VALUE &&
		fabs(currentSmoothedTorque2) > torqueAbortLimit)) {
			disableMotorsAndNotify("TORQUE LIMIT EXCEEDED");
			resetCustomStepCounter();
			Delay_ms(200);
			attemptMotorsReEnable("MOTORS RE-ENABLED AFTER TORQUE ABORT");
		}
	}
}


int main() {
	SetupUsbSerial();
	SetupEthernet();
	SetupMotors();

	torqueAbortLimit = 10.0f;
	torqueOffset = -2.4f;
	customCommandedStepCounter = 0;

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
