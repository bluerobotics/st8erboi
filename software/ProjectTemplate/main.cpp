#include "ClearCore.h"
#include "EthernetUdp.h"

#define LOCAL_PORT         8888
#define MAX_PACKET_LENGTH  100

EthernetUdp Udp;
unsigned char packetBuffer[MAX_PACKET_LENGTH];
IpAddress terminalIp;
uint16_t terminalPort = 0;
bool terminalDiscovered = false;

uint32_t pulsesPerRev = 6400;
uint32_t lastTorqueTime = 0;
const uint32_t torqueInterval = 20;
bool motorIsEnabled = false;
volatile bool abortRequested = false;  // NEW: Set by ABORT command

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
void resetMotor();
void handleUdpMessage(const char *msg);
void checkUdpDiscovery();

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

void sendTorqueDebug() {
	float torque = ConnectorM0.HlfbPercent();
	int hlfb = (int)ConnectorM0.HlfbState();
	char msg[128];
	if (torque > -9999.0f) {
		snprintf(msg, sizeof(msg), "torque: %.2f%%, hlfb: %d, enabled: %d", torque, hlfb, motorIsEnabled ? 1 : 0);
		} else {
		snprintf(msg, sizeof(msg), "torque: ---, hlfb: %d, enabled: %d", hlfb, motorIsEnabled ? 1 : 0);
	}
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
	if (!motorIsEnabled) {
		sendToPC("MOVE BLOCKED: Motor is disabled");
		return;
	}

	abortRequested = false;  // Clear previous abort state

	if (useAltSpeed) {
		ConnectorM0.EnableTriggerPulse(1, 25, true);
		Delay_ms(5);
	}

	ConnectorM0.Move(steps);
	Delay_ms(2);

	while (!ConnectorM0.StepsComplete() || ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) {
		checkUdpDiscovery();

		if (abortRequested) {
			sendToPC("MOVE ABORTED mid-move");
			return;
		}

		if (!motorIsEnabled) {
			sendToPC("MOVE ABORTED: motor disabled during move");
			return;
		}

		uint32_t now = Milliseconds();
		if (now - lastTorqueTime >= torqueInterval) {
			sendTorqueDebug();
			lastTorqueTime = now;
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

void resetMotor() {
	ConnectorM0.EnableRequest(false);
	motorIsEnabled = false;
	Delay_ms(100);
	ConnectorM0.ClearAlerts();
	ConnectorM0.EnableRequest(true);
	motorIsEnabled = true;
	while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) Delay_ms(50);
	sendToPC("motor reset complete");
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
		} else if (strcmp(msg, "RESET") == 0) {
		resetMotor();
		} else if (strcmp(msg, "DISABLE") == 0) {
		ConnectorM0.EnableRequest(false);
		motorIsEnabled = false;
		sendToPC("MOTOR DISABLED VIA DISABLE");
		} else if (strcmp(msg, "ABORT") == 0) {
		ConnectorM0.EnableRequest(false);
		motorIsEnabled = false;
		sendToPC("MOTOR DISABLED VIA ABORT");
		Delay_ms(100);
		ConnectorM0.EnableRequest(true);
		motorIsEnabled = true;
		while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) Delay_ms(50);
		sendToPC("MOTOR ENABLED");
		} else if (strcmp(msg, "ENABLE") == 0) {
		ConnectorM0.EnableRequest(true);
		motorIsEnabled = true;
		while (ConnectorM0.HlfbState() != MotorDriver::HLFB_ASSERTED) Delay_ms(50);
		sendToPC("MOTOR ENABLED");
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

int main() {
	SetupUsbSerial();
	SetupEthernet();
	SetupMotor();

	while (true) {
		checkUdpDiscovery();
		uint32_t now = Milliseconds();
		if (terminalDiscovered && now - lastTorqueTime >= torqueInterval) {
			sendTorqueDebug();
			lastTorqueTime = now;
		}
		Delay_ms(1);
	}
}
