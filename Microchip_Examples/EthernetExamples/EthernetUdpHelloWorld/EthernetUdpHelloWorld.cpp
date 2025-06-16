#include "ClearCore.h"
#include "EthernetUdp.h"

// === Config ===
#define LOCAL_PORT         8888
#define MAX_PACKET_LENGTH  100
#define STATUS_INTERVAL_MS 20
#define UDP_POLL_INTERVAL  10

// === Pulse Burst Pin Setup ===
#define PULSE_OUT_PIN      ConnectorIO0   // Use any digital out pin
#define PULSE_WIDTH_US     5              // Pulse HIGH time
#define PULSE_SPACING_US   20             // Delay between pulses

EthernetUdp Udp;
bool usingDhcp = true;
unsigned char packetBuffer[MAX_PACKET_LENGTH];

// === Terminal Info ===
IpAddress terminalIp;
uint16_t terminalPort = 0;
bool terminalDiscovered = false;

uint32_t lastStatusTime = 0;
uint32_t lastUdpCheck = 0;
int currentPosition = 0;

// === Setup ===
void SetupUsbSerial() {
    ConnectorUsb.Mode(Connector::USB_CDC);
    ConnectorUsb.Speed(9600);
    ConnectorUsb.PortOpen();

    uint32_t timeout = 5000;
    uint32_t startTime = Milliseconds();
    while (!ConnectorUsb && Milliseconds() - startTime < timeout);
}

void SetupEthernet() {
    EthernetMgr.Setup();

    if (usingDhcp) {
        if (!EthernetMgr.DhcpBegin()) {
            ConnectorUsb.SendLine("DHCP failed.");
            while (true);
        }
    } else {
        IpAddress staticIp = IpAddress(192, 168, 1, 200);
        EthernetMgr.LocalIp(staticIp);
    }

    while (!EthernetMgr.PhyLinkActive()) {
        ConnectorUsb.SendLine("Waiting for Ethernet link...");
        Delay_ms(1000);
    }

    Udp.Begin(LOCAL_PORT);
    ConnectorUsb.SendLine("Ethernet ready.");
}

void SetupPulsePin() {
    PULSE_OUT_PIN.Mode(Connector::DIGITAL_OUT);
    PULSE_OUT_PIN.State(false);
}

// === Messaging ===
void sendToPC(const char *msg) {
    if (!terminalDiscovered) return;

    Udp.Connect(terminalIp, terminalPort);
    Udp.PacketWrite(msg);
    Udp.PacketSend();
}

void handleDiscoveryPacket(const char *msg, IpAddress senderIp) {
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
    }
}

void sendPulseBurst(int pulseCount) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Sending %d pulse burst", pulseCount);
    sendToPC(msg);
    ConnectorUsb.SendLine(msg);

    for (int i = 0; i < pulseCount; i++) {
        PULSE_OUT_PIN.State(true);
        Delay_us(PULSE_WIDTH_US);
        PULSE_OUT_PIN.State(false);
        Delay_us(PULSE_SPACING_US);
    }
}

void handleUdpMessage(const char *msg) {
    if (strncmp(msg, "DISCOVER_CLEARCORE", 18) == 0) {
        handleDiscoveryPacket(msg, Udp.RemoteIp());
        return;
    }

    if (strncmp(msg, "MOVE ", 5) == 0) {
        int target = atoi(msg + 5);
        currentPosition = target;
        sendPulseBurst(target);
        return;
    }
}

void processIncomingPackets() {
    uint16_t packetSize = Udp.PacketParse();
    if (packetSize > 0) {
        int32_t bytesRead = Udp.PacketRead(packetBuffer, MAX_PACKET_LENGTH - 1);
        packetBuffer[bytesRead] = '\0';
        handleUdpMessage((char *)packetBuffer);
    }
}

// === Main ===
int main() {
    SetupUsbSerial();
    SetupEthernet();
    SetupPulsePin();

    ConnectorUsb.SendLine("ClearCore ready for pulse burst + UDP");

    while (true) {
        uint32_t now = Milliseconds();

        if (now - lastUdpCheck >= UDP_POLL_INTERVAL) {
            processIncomingPackets();
            lastUdpCheck = now;
        }

        if (terminalDiscovered && now - lastStatusTime >= STATUS_INTERVAL_MS) {
            char status[64];
            snprintf(status, sizeof(status), "Current Target Pos: %d", currentPosition);
            sendToPC(status);
            lastStatusTime = now;
        }
    }
}
