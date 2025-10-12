import subprocess
import sys
import os
import tkinter as tk
from tkinter import messagebox
import comms
import time

# --- Global State ---
simulator_process = None

def get_simulator_path():
    """Gets the absolute path to the simulator script."""
    # Assumes the script is located at <project_root>/st8erboi-simulator/simulator.py
    # This is a bit fragile but works for the current structure.
    current_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(current_dir, '..'))
    return os.path.join(project_root, 'st8erboi-simulator', 'simulator.py')

def run_simulator(parent):
    """Launches the device simulator in a new process."""
    global simulator_process
    simulator_path = get_simulator_path()

    if not os.path.exists(simulator_path):
        messagebox.showerror("Error", f"Simulator script not found at:\n{simulator_path}", parent=parent)
        return

    if simulator_process and simulator_process.poll() is None:
        # messagebox.showinfo("Info", "Simulator is already running.", parent=parent) # Removed as requested
        return

    try:
        # Use the same Python executable that's running this script
        python_executable = sys.executable
        simulator_process = subprocess.Popen([python_executable, simulator_path])
        # messagebox.showinfo("Success", "Device simulator started in a new window.", parent=parent) # Removed as requested
    except Exception as e:
        messagebox.showerror("Error", f"Failed to start simulator:\n{e}", parent=parent)

def scan_for_devices(gui_refs):
    """Triggers a manual device discovery broadcast."""
    comms.discover_devices(gui_refs)
    if 'terminal_cb' in gui_refs:
        # Use the comms logger to ensure thread safety
        comms.log_to_terminal("[GUI] Manual device scan triggered.", gui_refs)

def show_connected_devices(parent, gui_refs):
    """Displays a list of currently known devices and their status."""
    
    # We need to access the devices dictionary from the comms module
    # It's protected by a lock, so we should acquire it.
    with comms.devices_lock:
        if not comms.devices:
            message = "No devices are currently being tracked."
        else:
            message = "Known Devices:\n\n"
            for name, info in comms.devices.items():
                status = "Connected" if info['connected'] else "Disconnected"
                ip = info['ip'] if info['ip'] else "N/A"
                last_seen = f"{time.time() - info['last_rx']:.1f}s ago" if info['last_rx'] > 0 else "Never"
                message += f"- {name.capitalize()}\n"
                message += f"  Status: {status}\n"
                message += f"  IP: {ip}\n"
                message += f"  Last Seen: {last_seen}\n\n"

    messagebox.showinfo("Connected Devices", message, parent=parent)

def create_device_commands(parent, gui_refs):
    """
    Creates a dictionary of device-related commands.
    This helps keep the menu creation code clean by preparing the lambdas.
    """
    return {
        'run_simulator': lambda: run_simulator(parent),
        'scan_for_devices': lambda: scan_for_devices(gui_refs),
        'show_connected_devices': lambda: show_connected_devices(parent, gui_refs)
    }
