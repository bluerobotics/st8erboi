import os
import importlib

# This dictionary will hold the dynamically imported modules for each device
device_modules = {}
discovery_logs = [] # Store logs here
ALL_COMMANDS = {} # Store all loaded commands here

def discover_devices():
    """
    Scans the 'devices' directory to find all available device modules
    and imports their respective gui, commands, and telem modules.
    """
    # Clear previous state in case of a reload
    device_modules.clear()
    discovery_logs.clear()
    ALL_COMMANDS.clear()

    devices_dir = os.path.dirname(__file__)
    for item_name in os.listdir(devices_dir):
        item_path = os.path.join(devices_dir, item_name)
        # Check if it's a directory and not a special folder like __pycache__ or a file
        if os.path.isdir(item_path) and not item_name.startswith('__'):
            device_name = item_name
            try:
                gui_module = importlib.import_module(f'.{device_name}.gui', package='devices')
                commands_module = importlib.import_module(f'.{device_name}.commands', package='devices')
                telem_module = importlib.import_module(f'.{device_name}.telem', package='devices')
                
                device_modules[device_name] = {
                    'gui': gui_module,
                    'commands': commands_module,
                    'telem': telem_module
                }
                discovery_logs.append(f"Successfully loaded device module: {device_name}")

                # Load commands if they exist
                if hasattr(commands_module, 'get_commands'):
                    ALL_COMMANDS.update(commands_module.get_commands())

            except ImportError as e:
                discovery_logs.append(f"Could not load device module '{device_name}': {e}")

# Run discovery when the package is imported
discover_devices()
