#include "ClearCore.h"
#include "EthernetUdp.h"

// === Configuration ===
#define LOCAL_PORT          8888
#define MAX_PACKET_LENGTH   100
#define USB_BAUD_RATE       9600
#define STARTUP_WAIT_MS     10000

EthernetUdp Udp;
unsigned char packetBuffer[MAX_PACKET_LENGTH];
IpAddress terminalIp;
uint16_t terminalPort = 0;
bool terminalDiscovered = false;

// === Setup ===
void SetupUsbSerial() {
	ConnectorUsb.Mode(Connector::USB_CDC);
	ConnectorUsb.Speed(USB_BAUD_RATE);
	ConnectorUsb.PortOpen();

	uint32_t timeout = 5000;
	uint32_t start = Milliseconds();
	while (!ConnectorUsb && Milliseconds() - start < timeout);

	ConnectorUsb.SendLine("=== USB Ready ===");
	ConnectorUsb.Send("Waiting ");
	ConnectorUsb.Send(STARTUP_WAIT_MS / 1000);
	ConnectorUsb.SendLine("s for you to connect a terminal...");
	Delay_ms(STARTUP_WAIT_MS);
}

void SetupEthernet() {
	EthernetMgr.Setup();

	if (!EthernetMgr.DhcpBegin()) {
		ConnectorUsb.SendLine("DHCP failed!");
		while (true);
	}

	while (!EthernetMgr.PhyLinkActive()) {
		ConnectorUsb.SendLine("Waiting for Ethernet link...");
		Delay_ms(1000);
	}

	Udp.Begin(LOCAL_PORT);
	ConnectorUsb.SendLine("Ethernet initialized.");
	ConnectorUsb.Send("My IP: ");
	ConnectorUsb.SendLine(EthernetMgr.LocalIp().StringValue());
}

void sendToPC(const char *msg) {
	if (!terminalDiscovered) {
		ConnectorUsb.SendLine("No PC discovered yet — skipping send.");
		return;
	}

	ConnectorUsb.Send("Sending to ");
	ConnectorUsb.Send(terminalIp.StringValue());
	ConnectorUsb.Send(":");
	ConnectorUsb.Send(terminalPort);
	ConnectorUsb.Send(" => ");
	ConnectorUsb.SendLine(msg);

	Udp.Connect(terminalIp, terminalPort);
	Udp.PacketWrite(msg);
	Udp.PacketSend();
}

void handleDiscoveryPacket(const char *msg, IpAddress senderIp) {
	ConnectorUsb.Send("Discovery message: ");
	ConnectorUsb.SendLine(msg);

	char *portStr = strstr(msg, "PORT=");
	if (portStr) {
		terminalPort = atoi(portStr + 5);
		terminalIp = senderIp;
		terminalDiscovered = true;

		ConnectorUsb.Send("Discovered PC at ");
		ConnectorUsb.Send(terminalIp.StringValue());
		ConnectorUsb.Send(":");
		ConnectorUsb.SendLine(terminalPort);

		sendToPC("CLEARCORE_ACK");
		} else {
		ConnectorUsb.SendLine("No valid PORT= found in discovery message.");
	}
}

void checkUdpDiscovery() {
	uint16_t packetSize = Udp.PacketParse();
	if (packetSize > 0) {
		int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
		packetBuffer[bytesRead] = '\0';

		ConnectorUsb.Send("Received packet (");
		ConnectorUsb.Send(packetSize);
		ConnectorUsb.SendLine(" bytes):");
		ConnectorUsb.SendLine((char *)packetBuffer);

		handleDiscoveryPacket((char *)packetBuffer, Udp.RemoteIp());
	}
}

// === Main Loop ===
int main() {
	SetupUsbSerial();
	SetupEthernet();

	ConnectorUsb.SendLine("ClearCore running. Waiting for discovery...");

	while (true) {
		checkUdpDiscovery();
		Delay_ms(10);
	}
}
