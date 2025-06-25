#include "fillhead.h"

// --- MODIFIED: Correctly initialize IpAddress member ---
Fillhead::Fillhead() : injectorIp(INJECTOR_IP) {
	guiDiscovered = false;
	guiPort = 0;
	
	homedX = false;
	homedY = false;
	homedZ = false;
}

void Fillhead::setup() {
	setupUsbSerial(); // Good for debugging
	setupEthernet();
	setupMotors();
}