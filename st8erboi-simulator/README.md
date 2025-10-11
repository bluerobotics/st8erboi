# St8erBoi Device Simulator

This is a simple Python-based simulator for the Gantry and Fillhead devices of the St8erBoi project. It allows for GUI development and testing without needing the physical hardware.

## Features

- Simulates both Gantry and Fillhead devices.
- Each device can be toggled on or off independently via a simple UI.
- Sends realistic telemetry data over UDP.
- Responds to discovery broadcasts from the main GUI application.
- Handles basic commands like jogging and homing, and updates its state accordingly.

## How to Run

1.  Ensure you have Python 3 and Tkinter installed (usually included with Python).
2.  Navigate to the root directory of the `st8erboi` project.
3.  Run the simulator using the following command:

    ```sh
    python st8erboi-simulator/simulator.py
    ```

4.  A window will appear. Use the checkboxes to start or stop the Gantry and Fillhead simulators.
5.  With the simulator running, you can launch the main `st8erboi-app-tkinter` GUI, which should now connect to the simulated devices.

## Networking

- **Gantry Simulator:** Listens on port `8888`.
- **Fillhead Simulator:** Listens on port `8889`.
- **Client Application:** The simulator expects the GUI application to be listening on port `6272` for responses and telemetry.

The simulator listens on all network interfaces, and the main GUI application has been slightly modified to broadcast discovery packets to `<broadcast>` to facilitate discovery when running on the same machine.
