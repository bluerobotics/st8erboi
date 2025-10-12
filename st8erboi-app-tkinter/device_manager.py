import os
import importlib
import tkinter as tk

class DeviceManager:
    def __init__(self, shared_gui_refs):
        self.devices = {}
        self.discovery_logs = []
        self.shared_gui_refs = shared_gui_refs
        self.discover_devices()

    def discover_devices(self):
        """
        Dynamically discovers and loads device modules from the 'devices' directory.
        """
        self.log("Starting device discovery...")
        devices_dir = os.path.join(os.path.dirname(__file__), 'devices')
        if not os.path.isdir(devices_dir):
            self.log(f"Devices directory not found at '{devices_dir}'")
            return

        for device_name in os.listdir(devices_dir):
            device_path = os.path.join(devices_dir, device_name)
            if os.path.isdir(device_path) and not device_name.startswith('__'):
                try:
                    gui_module = importlib.import_module(f'devices.{device_name}.gui')
                    commands_module = importlib.import_module(f'devices.{device_name}.commands')
                    telem_module = importlib.import_module(f'devices.{device_name}.telem')
                    
                    self.devices[device_name] = {
                        'gui': gui_module,
                        'commands': commands_module,
                        'telem': telem_module,
                        'status_var': tk.StringVar(value=f'🔌 {device_name.capitalize()} Disconnected')
                    }
                    self.log(f"Successfully loaded modules for '{device_name}'")
                except ImportError as e:
                    self.log(f"Failed to load modules for '{device_name}': {e}")
                except Exception as e:
                    self.log(f"An unexpected error occurred loading '{device_name}': {e}")

    def log(self, message):
        """Adds a log message to the discovery logs."""
        print(message) # Also print to console for immediate feedback
        self.discovery_logs.append(message)

    def get_device_modules(self):
        """Returns the dictionary of loaded device modules."""
        return self.devices

    def get_discovery_logs(self):
        """Returns the list of discovery log messages."""
        return self.discovery_logs

    def create_all_gui_components(self, parent_container):
        """
        Iterates through all loaded devices and calls their GUI creation functions.
        """
        for device_name, modules in self.devices.items():
            if hasattr(modules['gui'], 'create_gui_components'):
                panel = modules['gui'].create_gui_components(parent_container, self.shared_gui_refs)
                self.shared_gui_refs[f'{device_name}_panel'] = panel
                panel.pack_forget() # Hide by default
                self.log(f"Created GUI panel for {device_name}")

    def get_all_command_functions(self):
        """
        Aggregates command functions from all discovered devices.
        """
        all_commands = {}
        for device_name, modules in self.devices.items():
            if hasattr(modules['commands'], 'get_command_functions'):
                # Pass a sender function specific to this device
                sender = self.get_device_sender(device_name)
                device_commands = modules['commands'].get_command_functions(sender, self.shared_gui_refs)
                all_commands.update(device_commands)
        
        # Add global commands
        all_commands['abort'] = self.send_global_abort
        return all_commands

    def get_device_sender(self, device_name):
        """Returns a lambda function that sends a message to a specific device."""
        import comms # Local import to avoid circular dependency
        return lambda msg: comms.send_to_device(device_name, msg, self.shared_gui_refs)

    def send_global_abort(self):
        """Sends an ABORT command to all connected devices."""
        import comms # Local import
        if 'terminal_cb' in self.shared_gui_refs:
            comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", self.shared_gui_refs.get('terminal_cb'))
        
        for device_name in self.devices.keys():
            # A more robust implementation would check if the device is actually connected
            sender = self.get_device_sender(device_name)
            sender("ABORT")

    def get_all_scripting_commands(self):
        """
        Aggregates scripting command definitions from all discovered devices.
        """
        all_commands = {}
        for device_name, modules in self.devices.items():
            if hasattr(modules['commands'], 'get_scripting_commands'):
                device_commands = modules['commands'].get_scripting_commands()
                all_commands.update(device_commands)
        return all_commands

    def get_all_device_variable_names(self):
        """
        Collects all GUI variable names from all devices.
        """
        all_vars = {}
        for device_name, modules in self.devices.items():
            if hasattr(modules['gui'], 'get_gui_variable_names'):
                all_vars[device_name] = modules['gui'].get_gui_variable_names()
        return all_vars
