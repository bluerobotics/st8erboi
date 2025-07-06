#include "fillhead.h"

// Global instance of our Fillhead controller class
Fillhead fillhead;

/*
    Main application entry point.
*/
int main(void) {
    // Perform one-time setup
    fillhead.setup();

    // Main non-blocking application loop
    while (true) {
        // This single call now processes communications and updates all axis state machines.
        fillhead.update();
    }
}