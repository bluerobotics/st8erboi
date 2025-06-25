#include "fillhead.h"

Fillhead fillhead;

int main(void) {
	fillhead.setup();

	while (true) {
		// Check for and process any incoming UDP commands
		fillhead.processUdp();

		// Send periodic telemetry packets
		fillhead.sendInternalTelemetry();
		fillhead.sendGuiTelemetry();
		
		Delay_ms(10); // Small delay to prevent busy-waiting
	}
}