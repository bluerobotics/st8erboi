import os
import importlib
import tkinter as tk
import json

class DeviceManager:
    def __init__(self, shared_gui_refs):
        self.devices = {}
        self.device_state = {} # New dictionary for connection state
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
                    
                    # The parser module is now optional.
                    parser_module = None
                    try:
                        parser_module = importlib.import_module(f'devices.{device_name}.parser')
                    except ImportError:
                        # This is now the standard behavior, so no log message is needed.
                        pass

                    # Load scripting commands from JSON
                    scripting_commands = {}
                    json_path = os.path.join(device_path, 'commands.json')
                    if os.path.exists(json_path):
                        with open(json_path, 'r') as f:
                            scripting_commands = json.load(f)

                    # Load telemetry schema and create GUI variables
                    telem_schema = {}
                    schema_path = os.path.join(device_path, 'telem_schema.json')
                    if os.path.exists(schema_path):
                        with open(schema_path, 'r') as f:
                            telem_schema = json.load(f)
                        # Dynamically create the tk variables
                        for key, details in telem_schema.items():
                            if 'gui_var' in details:
                                self.shared_gui_refs[details['gui_var']] = tk.StringVar(value="---")

                    # Load optional device-specific configuration
                    device_config = {}
                    config_path = os.path.join(device_path, 'device_config.json')
                    if os.path.exists(config_path):
                        with open(config_path, 'r') as f:
                            device_config = json.load(f)

                    self.devices[device_name] = {
                        'gui': gui_module,
                        'parser': parser_module,
                        'telem_schema': telem_schema, # Store the schema
                        'scripting_commands': scripting_commands, # Store loaded JSON data
                        'config': device_config, # Store the optional device config
                        'status_var': tk.StringVar(value=f'🔌 {device_name.capitalize()} Disconnected')
                    }
                    # Initialize the state for this device
                    self.device_state[device_name] = {
                        "ip": None, 
                        "last_rx": 0, 
                        "connected": False, 
                        "last_discovery_attempt": 0
                    }
                    self.log(f"Successfully loaded device: {device_name}")

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
        for device_name, device_data in self.devices.items():
            sender = self.get_device_sender(device_name)
            
            # Add the generic sender for the script processor
            all_commands[f"send_{device_name}"] = sender

            # Dynamically create functions for GUI actions defined in JSON
            for command_name, details in device_data.get('scripting_commands', {}).items():
                if 'gui_action' in details:
                    action_name = details['gui_action']
                    # Use a closure to capture the command_name correctly
                    all_commands[action_name] = lambda cmd=command_name: sender(cmd)

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

    def get_device_state(self, device_name):
        """Returns the connection state for a specific device."""
        return self.device_state.get(device_name)

    def get_all_device_states(self):
        """Returns the dictionary of all device connection states."""
        return self.device_state

    def update_device_state(self, device_name, new_state):
        """Updates the connection state for a specific device."""
        if device_name in self.device_state:
            self.device_state[device_name].update(new_state)

    def get_all_scripting_commands(self):
        """
        Aggregates scripting command definitions from all discovered devices.
        """
        all_commands = {}
        for device_name, modules in self.devices.items():
            # Check for commands loaded from JSON
            if modules.get('scripting_commands'):
                all_commands.update(modules['scripting_commands'])
        return all_commands

    def get_all_device_variable_names(self):
        """
        Collects all GUI variable names from all devices and maps them back to their schema keys.
        Returns a dictionary like: {'gantry': {'gantry_x_pos_var': 'x_p', ...}, ...}
        """
        all_vars = {}
        for device_name, modules in self.devices.items():
            device_map = {}
            # Reconstruct the mapping from the stored telem_schema
            telem_schema = modules.get('telem_schema', {})
            for schema_key, details in telem_schema.items():
                if 'gui_var' in details:
                    device_map[details['gui_var']] = schema_key
            all_vars[device_name] = device_map
        return all_vars
