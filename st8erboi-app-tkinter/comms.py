import socket
import threading
import time
import datetime
import tkinter as tk
from devices import device_modules

# --- Constants ---
CLEARCORE_PORT = 8888
CLIENT_PORT = 6272
HEARTBEAT_INTERVAL = 0.5
DISCOVERY_INTERVAL = 2.0
TIMEOUT_THRESHOLD = 3.0

# --- Helper ---
def safe_float(s, default_val=0.0):
    try:
        return float(s)
    except (ValueError, TypeError):
        return default_val

# --- Global State ---
last_fw_main_state_for_gui_update = None
last_fw_feed_state_for_gui_update = None

# --- Communication State ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(('', CLIENT_PORT))
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.1)
except OSError as e:
    print(f"ERROR binding socket: {e}. Port {CLIENT_PORT} may be in use by another application.")
    exit()

# MODIFIED: Added a threading lock to prevent race conditions on the socket.
socket_lock = threading.Lock()
devices_lock = threading.Lock()

devices = {
    "fillhead": {"ip": None, "last_rx": 0, "connected": False, "last_discovery_attempt": 0},
    "gantry": {"ip": None, "last_rx": 0, "connected": False, "last_discovery_attempt": 0}
}


# --- Communication Functions ---

def log_to_terminal(msg, gui_refs):
    """Safely logs a message to the GUI terminal by placing it on the queue."""
    timestr = datetime.datetime.now().strftime("[%H:%M:%S.%f]")[:-3]
    full_msg = f"{timestr} {msg}\n"
    
    terminal_cb = gui_refs.get('terminal_cb')
    gui_queue = gui_refs.get('gui_queue')

    if terminal_cb and gui_queue:
        # The terminal_cb function itself is what needs to run in the main thread.
        gui_queue.put((terminal_cb, (full_msg,), {}))
    else:
        # Fallback for when GUI elements aren't available
        print(full_msg)


def discover_devices(gui_refs):
    """Sends a generic discovery message."""
    msg = f"DISCOVER_DEVICE PORT={CLIENT_PORT}"

    log_discovery = gui_refs.get('show_discovery_var', tk.BooleanVar(value=False)).get()
    if log_discovery:
        log_to_terminal(f"[CMD SENT to BROADCAST]: {msg}", gui_refs)

    try:
        # No lock needed for broadcast as it's less critical and frequent
        sock.sendto(msg.encode(), ('192.168.1.255', CLEARCORE_PORT))
    except Exception as e:
        log_to_terminal(f"Discovery error: {e}", gui_refs)


def discovery_loop(gui_refs):
    """Continuously sends out discovery messages."""
    while True:
        discover_devices(gui_refs)
        time.sleep(DISCOVERY_INTERVAL)


def send_to_device(device_key, msg, gui_refs):
    """Sends a message to a specific device if its IP is known."""
    with devices_lock:
        device_ip = devices[device_key].get("ip")
    if device_ip:
        # MODIFIED: Added a lock to ensure thread-safe socket access.
        with socket_lock:
            try:
                log_to_terminal(f"[CMD SENT to {device_key.upper()}]: {msg}", gui_refs)
                sock.sendto(msg.encode(), (device_ip, CLEARCORE_PORT))
            except Exception as e:
                log_to_terminal(f"Error sending to {device_key}: {e}", gui_refs)

    elif "DISCOVER" not in msg:
        log_to_terminal(f"Cannot send to {device_key}: IP unknown.", gui_refs)


def monitor_connections(gui_refs):
    """Monitors device connection status."""
    terminal_cb = gui_refs.get('terminal_cb')
    gui_queue = gui_refs.get('gui_queue')

    while True:
        now = time.time()
        with devices_lock:
            # Create a copy of items to avoid issues with dictionary size changes during iteration
            device_items = list(devices.items())

        for key, device in device_items:
            with devices_lock:
                # Re-fetch the device's current state inside the lock
                device = devices[key]
                prev_conn_status = device["connected"]

            if prev_conn_status and (now - device["last_rx"]) > TIMEOUT_THRESHOLD:
                with devices_lock:
                    devices[key]["connected"] = False
                    devices[key]["ip"] = None
                
                log_to_terminal(f"🔌 {key.capitalize()} Disconnected", gui_refs)
                
                # Queue the panel reset/hide function to run on the main thread
                if gui_queue and 'reset_and_hide_panel' in gui_refs:
                    gui_queue.put((gui_refs['reset_and_hide_panel'], (key,), {}))
                
                # Also queue the visibility update for the "searching" panel
                if gui_queue:
                    gui_queue.put((update_searching_panel_visibility, (gui_refs,), {}))

        time.sleep(HEARTBEAT_INTERVAL)


def update_searching_panel_visibility(gui_refs):
    """Shows or hides the 'searching for devices' panel."""
    searching_frame = gui_refs.get('searching_frame')
    status_bar_container = gui_refs.get('status_bar_container')
    if not searching_frame or not status_bar_container:
        return

    any_connected = False
    with devices_lock:
        for device in devices.values():
            if device["connected"]:
                any_connected = True
                break
    
    # This function is now executed by the main thread, so it's safe to modify the GUI
    if any_connected:
        searching_frame.pack_forget()
    else:
        # Use 'before' to ensure it's always packed at the top of its parent,
        # right before the container that holds the device status panels.
        searching_frame.pack(before=status_bar_container, side=tk.TOP, fill="x", expand=False, pady=(0, 8))

def queue_ui_update(gui_refs, var_name, value):
    """Safely queues a tkinter variable update."""
    gui_queue = gui_refs.get('gui_queue')
    var = gui_refs.get(var_name)
    if gui_queue and var:
        # Convert value type if necessary for DoubleVar
        if isinstance(var, tk.DoubleVar):
            value = safe_float(value)
        gui_queue.put((var.set, (value,), {}))

def handle_connection(device_key, source_ip, gui_refs):
    """Handles the logic for a new or existing connection."""
    gui_queue = gui_refs.get('gui_queue')
    is_new_connection = False

    with devices_lock:
        device = devices[device_key]
        if not device["connected"]:
            is_new_connection = True
        device["ip"] = source_ip
        device["last_rx"] = time.time()
        device["connected"] = True

    if is_new_connection:
        status_text = f"✅ {device_key.capitalize()} Connected ({source_ip})"
        log_to_terminal(status_text, gui_refs)
        
        status_var = gui_refs.get(f'status_var_{device_key}')
        if gui_queue and status_var:
            # Queue the status variable update
            gui_queue.put((status_var.set, (status_text,), {}))
        
        # Queue showing the panel
        if gui_queue and 'show_panel' in gui_refs:
            gui_queue.put((gui_refs['show_panel'], (device_key,), {}))
        
        # Queue the visibility update for the "searching" panel
        if gui_queue:
            gui_queue.put((update_searching_panel_visibility, (gui_refs,), {}))


# --- Parser Functions ---

def parse_fillhead_telemetry(msg, gui_refs):
    """Parses the telemetry string from the Fillhead controller."""
    try:
        payload = msg.split("FILLHEAD_TELEM: ")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Main Controller States
        queue_ui_update(gui_refs, 'main_state_var', parts.get("MAIN_STATE", "---"))

        # Injector Motors
        queue_ui_update(gui_refs, 'torque0_var', parts.get('inj_t0', '0.0'))
        queue_ui_update(gui_refs, 'torque1_var', parts.get('inj_t1', '0.0'))
        queue_ui_update(gui_refs, 'enabled_state1_var', "Enabled" if parts.get("enabled0", "0") == "1" else "Disabled")
        queue_ui_update(gui_refs, 'enabled_state2_var', "Enabled" if parts.get("enabled1", "0") == "1" else "Disabled")
        queue_ui_update(gui_refs, 'homed0_var', "Homed" if parts.get("inj_h_mach", "0") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'homed1_var', "Homed" if parts.get("inj_h_cart", "0") == "1" else "Not Homed")

        # Component States
        queue_ui_update(gui_refs, 'fillhead_injector_state_var', parts.get("inj_st", "---"))
        queue_ui_update(gui_refs, 'fillhead_inj_valve_state_var', parts.get("inj_v_st", "---"))
        queue_ui_update(gui_refs, 'fillhead_vac_valve_state_var', parts.get("vac_v_st", "---"))
        queue_ui_update(gui_refs, 'fillhead_heater_state_var', parts.get("h_st_str", "---"))
        queue_ui_update(gui_refs, 'fillhead_vacuum_state_var', parts.get("vac_st_str", "---"))
        
        # Pinch Valves
        queue_ui_update(gui_refs, 'inj_valve_pos_var', parts.get("inj_valve_pos", "---"))
        queue_ui_update(gui_refs, 'inj_valve_homed_var', "Homed" if parts.get("inj_valve_homed", "0") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'torque2_var', parts.get('inj_valve_torque', '0.0'))
        queue_ui_update(gui_refs, 'vac_valve_pos_var', parts.get("vac_valve_pos", "---"))
        queue_ui_update(gui_refs, 'vac_valve_homed_var', "Homed" if parts.get("vac_valve_homed", "0") == "1" else "Not Homed")
        queue_ui_update(gui_refs, 'torque3_var', parts.get('vac_valve_torque', '0.0'))

        # Other System Status
        queue_ui_update(gui_refs, 'machine_steps_var', parts.get("inj_mach_mm", "N/A"))
        queue_ui_update(gui_refs, 'cartridge_steps_var', parts.get("inj_cart_mm", "N/A"))
        queue_ui_update(gui_refs, 'inject_cumulative_ml_var', f'{safe_float(parts.get("inj_cumulative_ml")):.2f} ml')
        queue_ui_update(gui_refs, 'inject_active_ml_var', f'{safe_float(parts.get("inj_active_ml")):.2f} ml')
        queue_ui_update(gui_refs, 'injection_target_ml_var', f'{safe_float(parts.get("inj_tgt_ml")):.2f}')
        queue_ui_update(gui_refs, 'temp_c_var', f'{safe_float(parts.get("h_pv")):.1f} °C')
        queue_ui_update(gui_refs, 'pid_setpoint_var', f'{safe_float(parts.get("h_sp")):.1f}')
        queue_ui_update(gui_refs, 'vacuum_psig_var', f"{safe_float(parts.get('vac_pv')):.2f} PSIG")

    except Exception as e:
        log_to_terminal(f"Fillhead telemetry parse error: {e}\n", gui_refs)


def parse_gantry_telemetry(msg, gui_refs):
    """Parses the telemetry string from the Gantry controller."""
    try:
        payload = msg.split("GANTRY_TELEM: ")[1]
        parts = dict(item.split(':', 1) for item in payload.split(',') if ':' in item)

        # Update Gantry State
        queue_ui_update(gui_refs, 'fh_state_var', parts.get("gantry_state", "---"))

        # Update Axis States
        axes = ['x', 'y', 'z']
        fh_map = {'x': 'm0', 'y': 'm1', 'z': 'm3'}

        for axis in axes:
            key_suffix = fh_map[axis]
            queue_ui_update(gui_refs, f'fh_pos_{key_suffix}_var', parts.get(f"{axis}_p", "---"))
            queue_ui_update(gui_refs, f'fh_torque_{key_suffix}_var', parts.get(f"{axis}_t", '0.0'))
            queue_ui_update(gui_refs, f'fh_enabled_{key_suffix}_var', "Enabled" if parts.get(f"{axis}_e", "0") == "1" else "Disabled")
            queue_ui_update(gui_refs, f'fh_homed_{key_suffix}_var', "Homed" if parts.get(f"{axis}_h", "0") == "1" else "Not Homed")
            queue_ui_update(gui_refs, f'fh_state_{axis}_var', parts.get(f"{axis}_st", "---"))

    except Exception as e:
        log_to_terminal(f"Gantry telemetry parse error: {e} -> on msg: {msg}\n", gui_refs)


def recv_loop(gui_refs):
    """Main network receive loop. Routes packets to the correct local parser."""
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()
            source_ip = addr[0]
            log_telemetry = gui_refs.get('show_telemetry_var', tk.BooleanVar(value=False)).get()

            if msg.startswith("DISCOVERY_RESPONSE:"):
                try:
                    # e.g., DISCOVERY_RESPONSE: DEVICE_ID=gantry
                    parts = msg.split(" ")[1].split("=")
                    if parts[0] == "DEVICE_ID":
                        device_key = parts[1].lower()
                        # This part is new, to dynamically add devices
                        with devices_lock:
                            if device_key not in devices:
                                devices[device_key] = {"ip": None, "last_rx": 0, "connected": False, "last_discovery_attempt": 0}
                                # log_to_terminal(f"Discovered new device: {device_key}", gui_refs) # This is redundant
                        handle_connection(device_key, source_ip, gui_refs)
                except IndexError:
                    log_to_terminal(f"Malformed discovery response: {msg}", gui_refs)
            
            # --- DYNAMIC TELEMETRY PARSING ---
            elif "_TELEM:" in msg:
                try:
                    device_key = msg.split("_TELEM:")[0].lower()
                    if device_key in device_modules:
                        handle_connection(device_key, source_ip, gui_refs)
                        if log_telemetry:
                            log_to_terminal(f"[TELEM @{source_ip}]: {msg}", gui_refs)
                        
                        # Call the dynamically loaded parser
                        parser = device_modules[device_key]['telem'].parse_telemetry
                        parser(msg, gui_refs, queue_ui_update, safe_float)
                    else:
                        log_to_terminal(f"[UNHANDLED @{source_ip}]: {msg}", gui_refs)
                except Exception as e:
                    log_to_terminal(f"Error processing telemetry for {msg}: {e}", gui_refs)

            elif msg.startswith(("INFO:", "DONE:", "ERROR:",
                                 "FILLHEAD_DONE:", "FH_DONE:",
                                 "FILLHEAD_INFO:", "FH_INFO:", "FILLHEAD_ERROR:", "FH_ERROR:",
                                 "GANTRY_DONE:", "GANTRY_INFO:", "GANTRY_ERROR:",
                                 "FILLHEAD_START:", "GANTRY_START:")):
                log_to_terminal(f"[STATUS @{source_ip}]: {msg}", gui_refs)
                for key, device in devices.items():
                    if device["ip"] == source_ip:
                        device["last_rx"] = time.time()
                        break
            else:
                log_to_terminal(f"[UNHANDLED @{source_ip}]: {msg}", gui_refs)

        except socket.timeout:
            continue
        except Exception as e:
            # Check if the socket was closed intentionally.
            if isinstance(e, socket.error) and e.errno == 10004: # WSAEINTR on Windows
                break # Exit loop if socket is closed.
            log_to_terminal(f"Recv_loop error: {e}\n", gui_refs)
