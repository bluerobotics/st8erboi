import socket
import threading
import time
import datetime
import tkinter as tk
from tkinter import ttk

# Import the GUI builder functions from the other files
from shared_gui import create_shared_widgets
from injector_gui import create_injector_tab
from fillhead_gui import create_fillhead_tab

# --- Constants ---
CLEARCORE_PORT = 8888
CLIENT_PORT = 6272
HEARTBEAT_INTERVAL = 0.3
DISCOVERY_RETRY_INTERVAL = 2.0
TIMEOUT_THRESHOLD = 1.5
TORQUE_HISTORY_LENGTH = 200
STEPS_PER_REV = 800

# --- Global State ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(('', CLIENT_PORT))
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.1)
except OSError as e:
    print(f"ERROR binding socket: {e}. Port {CLIENT_PORT} may be in use by another application.")
    exit()

devices = {
    "injector": {"ip": None, "last_rx": 0, "connected": False},
    "fillhead": {"ip": None, "last_rx": 0, "connected": False}
}
last_fw_main_state_for_gui_update = None
last_fw_feed_state_for_gui_update = None


def log_to_terminal(msg, terminal_cb_func):
    timestr = datetime.datetime.now().strftime("[%H:%M:%S.%f]")[:-3]
    if terminal_cb_func:
        terminal_cb_func(f"{timestr} {msg}\n")
    else:
        print(f"{timestr} {msg}")


def discover(terminal_cb_func):
    msg = f"DISCOVER_TELEM PORT={CLIENT_PORT}"
    try:
        sock.sendto(msg.encode(), ('192.168.1.255', CLEARCORE_PORT))
    except Exception as e:
        log_to_terminal(f"Discovery error: {e}", terminal_cb_func)


def send_to_device(device_key, msg, terminal_cb_func):
    device_ip = devices[device_key].get("ip")
    if device_ip:
        try:
            if "SET_PEER_IP" not in msg and "CLEAR_PEER_IP" not in msg:
                log_to_terminal(f"[CMD SENT to {device_key.upper()}]: {msg}", terminal_cb_func)
            sock.sendto(msg.encode(), (device_ip, CLEARCORE_PORT))
        except Exception as e:
            log_to_terminal(f"UDP send error to {device_key} ({device_ip}): {e}", terminal_cb_func)
    else:
        if "ABORT" not in msg and "DISCOVER" not in msg:
            log_to_terminal(f"Cannot send to {device_key}: IP unknown.", terminal_cb_func)


def monitor_connections(gui_refs):
    global last_fw_main_state_for_gui_update, last_fw_feed_state_for_gui_update
    while True:
        now = time.time()
        for key, device in devices.items():
            prev_conn_status = device["connected"]
            current_conn_status = prev_conn_status
            time_since_last_rx = now - device["last_rx"] if device["last_rx"] > 0 else float('inf')

            if prev_conn_status and time_since_last_rx > TIMEOUT_THRESHOLD:
                current_conn_status = False
            elif not prev_conn_status and time_since_last_rx < TIMEOUT_THRESHOLD:
                current_conn_status = True

            if current_conn_status != prev_conn_status:
                device["connected"] = current_conn_status

                other_key = "fillhead" if key == "injector" else "injector"

                if current_conn_status:  # A device just connected
                    ip_address = device.get("ip", "?.?.?.?")
                    status_text = f"✅ {key.capitalize()} Connected ({ip_address})"

                    if devices[other_key]["connected"]:
                        log_to_terminal(f"INFO: Both devices connected. Brokering IPs.", gui_refs.get('terminal_cb'))
                        send_to_device(key, f"SET_PEER_IP {devices[other_key]['ip']}", gui_refs.get('terminal_cb'))
                        send_to_device(other_key, f"SET_PEER_IP {device['ip']}", gui_refs.get('terminal_cb'))

                else:  # A device just disconnected
                    status_text = f"🔌 {key.capitalize()} Disconnected"

                    if devices[other_key]["connected"]:
                        log_to_terminal(f"INFO: {key.capitalize()} disconnected. Informing peer.",
                                        gui_refs.get('terminal_cb'))
                        send_to_device(other_key, "CLEAR_PEER_IP", gui_refs.get('terminal_cb'))

                log_to_terminal(status_text, gui_refs.get('terminal_cb'))

                if f'status_var_{key}' in gui_refs:
                    gui_refs[f'status_var_{key}'].set(status_text)

                if not current_conn_status and key == "injector":
                    if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set("UNKNOWN")
                    if 'update_state' in gui_refs and callable(gui_refs['update_state']):
                        gui_refs['update_state']("UNKNOWN")
                    if 'update_feed_buttons' in gui_refs and callable(gui_refs['update_feed_buttons']):
                        gui_refs['update_feed_buttons']("IDLE")
                    last_fw_main_state_for_gui_update = "UNKNOWN"
                    last_fw_feed_state_for_gui_update = "IDLE"

        time.sleep(HEARTBEAT_INTERVAL)


def auto_discover_loop(terminal_cb_func):
    while True:
        discover(terminal_cb_func)
        time.sleep(DISCOVERY_RETRY_INTERVAL)


def parse_injector_telemetry(msg, gui_refs):
    global last_fw_main_state_for_gui_update, last_fw_feed_state_for_gui_update
    try:
        parts = dict(item.split(':', 1) for item in msg.split(',')[1:] if ':' in item)

        fw_main_state = parts.get("MAIN_STATE", "---")
        fw_feed_state = parts.get("FEED_STATE", "IDLE")

        if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set(fw_main_state)
        if 'feed_state_var' in gui_refs: gui_refs['feed_state_var'].set(fw_feed_state)

        if 'homing_state_var' in gui_refs: gui_refs['homing_state_var'].set(parts.get("HOMING_STATE", "---"))
        if 'homing_phase_var' in gui_refs: gui_refs['homing_phase_var'].set(parts.get("HOMING_PHASE", "---"))
        if 'error_state_var' in gui_refs: gui_refs['error_state_var'].set(parts.get("ERROR_STATE", "No Error"))

        for i in range(3):
            gui_var_index = i + 1
            torque_str = parts.get(f'torque{i}', '0.0')
            if f'torque_value{gui_var_index}_var' in gui_refs:
                gui_refs[f'torque_value{gui_var_index}_var'].set(torque_str)

            if f'position_cmd{gui_var_index}_var' in gui_refs: gui_refs[f'position_cmd{gui_var_index}_var'].set(
                parts.get(f'pos_cmd{i}', '0'))
            if f'motor_state{gui_var_index}_var' in gui_refs: gui_refs[f'motor_state{gui_var_index}_var'].set(
                "Ready" if parts.get(f"hlfb{i}", "0") == "1" else "Busy/Fault")
            if f'enabled_state{gui_var_index}_var' in gui_refs: gui_refs[f'enabled_state{gui_var_index}_var'].set(
                "Enabled" if parts.get(f"enabled{i}", "0") == "1" else "Disabled")

        if 'pinch_homed_var' in gui_refs: gui_refs['pinch_homed_var'].set(
            "Homed" if parts.get("homed2", "0") == "1" else "Not Homed")

        if 'machine_steps_var' in gui_refs: gui_refs['machine_steps_var'].set(parts.get("machine_steps", "N/A"))
        if 'cartridge_steps_var' in gui_refs: gui_refs['cartridge_steps_var'].set(parts.get("cartridge_steps", "N/A"))
        if 'inject_dispensed_ml_var' in gui_refs: gui_refs['inject_dispensed_ml_var'].set(
            f'{float(parts.get("dispensed_ml", 0.0)):.2f} ml')

        injector_motors_enabled = parts.get("enabled0", "0") == "1"
        pinch_motor_enabled = parts.get("enabled2", "0") == "1"
        if 'enable_disable_var' in gui_refs: gui_refs['enable_disable_var'].set(
            "Enabled" if injector_motors_enabled else "Disabled")
        if 'enable_disable_pinch_var' in gui_refs: gui_refs['enable_disable_pinch_var'].set(
            "Enabled" if pinch_motor_enabled else "Disabled")

        torque_floats = []
        for i in range(3):
            torque_str = parts.get(f"torque{i}", '0.0')
            try:
                torque_val = float(torque_str)
            except ValueError:
                torque_val = 0.0
            torque_floats.append(torque_val)

        gui_refs.get('injector_torque_history1', []).append(torque_floats[0])
        gui_refs.get('injector_torque_history2', []).append(torque_floats[1])
        gui_refs.get('injector_torque_history3', []).append(torque_floats[2])
        gui_refs.get('injector_torque_times', []).append(time.time())

        while len(gui_refs['injector_torque_times']) > TORQUE_HISTORY_LENGTH:
            gui_refs['injector_torque_times'].pop(0)
            gui_refs['injector_torque_history1'].pop(0)
            gui_refs['injector_torque_history2'].pop(0)
            gui_refs['injector_torque_history3'].pop(0)

        if 'update_injector_torque_plot' in gui_refs and callable(gui_refs['update_injector_torque_plot']):
            gui_refs['update_injector_torque_plot']()

        if fw_main_state != last_fw_main_state_for_gui_update:
            if 'update_state' in gui_refs and callable(gui_refs['update_state']):
                gui_refs['update_state'](fw_main_state)
            last_fw_main_state_for_gui_update = fw_main_state

        if fw_feed_state != last_fw_feed_state_for_gui_update:
            if 'update_feed_buttons' in gui_refs and callable(gui_refs['update_feed_buttons']):
                gui_refs['update_feed_buttons'](fw_feed_state)
            last_fw_feed_state_for_gui_update = fw_feed_state

    except Exception as e:
        log_to_terminal(f"Injector telemetry parse error: {e}", gui_refs.get('terminal_cb'))


def parse_fillhead_telemetry(msg, gui_refs):
    try:
        parts = dict(item.split(':', 1) for item in msg.split(',')[1:] if ':' in item)

        if 'fh_state_var' in gui_refs:
            gui_refs['fh_state_var'].set(parts.get("state", "UNKNOWN"))

        torque_floats = []
        for i in range(4):
            pos = parts.get(f'pos_m{i}', '0')
            enabled = "Enabled" if parts.get(f'enabled_m{i}', '0') == "1" else "Disabled"
            homed = "Homed" if parts.get(f'homed_m{i}', '0') == "1" else "Not Homed"
            torque_str = parts.get(f"torque_m{i}", '0.0')

            try:
                torque_val = float(torque_str)
            except ValueError:
                torque_val = 0.0
            torque_floats.append(torque_val)

            if f'fh_pos_m{i}_var' in gui_refs: gui_refs[f'fh_pos_m{i}_var'].set(pos)
            if f'fh_torque_m{i}_var' in gui_refs: gui_refs[f'fh_torque_m{i}_var'].set(f"{torque_val:.1f} %")
            if f'fh_enabled_m{i}_var' in gui_refs: gui_refs[f'fh_enabled_m{i}_var'].set(enabled)
            if f'fh_homed_m{i}_var' in gui_refs: gui_refs[f'fh_homed_m{i}_var'].set(homed)

        for i in range(4):
            gui_refs.get(f'fillhead_torque_history{i}', []).append(torque_floats[i])
        gui_refs.get('fillhead_torque_times', []).append(time.time())

        while len(gui_refs['fillhead_torque_times']) > TORQUE_HISTORY_LENGTH:
            gui_refs['fillhead_torque_times'].pop(0)
            for i in range(4):
                if gui_refs.get(f'fillhead_torque_history{i}'):
                    gui_refs[f'fillhead_torque_history{i}'].pop(0)

        if 'update_fillhead_torque_plot' in gui_refs and callable(gui_refs['update_fillhead_torque_plot']):
            gui_refs['update_fillhead_torque_plot']()

        if 'update_fillhead_viz' in gui_refs and callable(gui_refs['update_fillhead_viz']):
            gui_refs['update_fillhead_viz']()

    except Exception as e:
        log_to_terminal(f"Fillhead telemetry parse error: {e}", gui_refs.get('terminal_cb'))


def recv_loop(gui_refs):
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()
            source_ip = addr[0]

            log_telemetry = gui_refs.get('show_telemetry_var', tk.BooleanVar(value=False)).get()
            log_discovery = gui_refs.get('show_discovery_var', tk.BooleanVar(value=False)).get()

            if msg.startswith("INJ_TELEM_GUI:"):
                if log_telemetry:
                    log_to_terminal(f"[TELEM @{source_ip}]: {msg}", gui_refs.get('terminal_cb'))
                devices["injector"]["ip"] = source_ip
                devices["injector"]["last_rx"] = time.time()
                parse_injector_telemetry(msg, gui_refs)

            elif msg.startswith("FH_TELEM_GUI:"):
                if log_telemetry:
                    log_to_terminal(f"[TELEM @{source_ip}]: {msg}", gui_refs.get('terminal_cb'))
                devices["fillhead"]["ip"] = source_ip
                devices["fillhead"]["last_rx"] = time.time()
                parse_fillhead_telemetry(msg, gui_refs)

            elif msg.startswith("DISCOVERED:"):
                if log_discovery:
                    log_to_terminal(f"[DISCOVERY @{source_ip}]: {msg}", gui_refs.get('terminal_cb'))
                # Assume DISCOVERED is from Injector for now
                devices["injector"]["ip"] = source_ip
                devices["injector"]["last_rx"] = time.time()

            elif msg.startswith(("INFO:", "DONE:", "ERROR:")):
                # Always log important status messages from devices
                log_to_terminal(f"[STATUS @{source_ip}]: {msg}", gui_refs.get('terminal_cb'))

            elif msg.startswith("FH_TELEM_INT:"):
                # Only log internal telemetry if the box is checked
                if log_telemetry:
                    log_to_terminal(f"[INTERNAL @{source_ip}]: {msg}", gui_refs.get('terminal_cb'))

            else:
                log_to_terminal(f"[Unknown@{source_ip}]: {msg}", gui_refs.get('terminal_cb'))

        except socket.timeout:
            continue
        except Exception as e:
            log_to_terminal(f"Recv_loop error: {e}", gui_refs.get('terminal_cb'))


def main():
    root = tk.Tk()
    root.title("St8erboi Multi-Device Controller")
    root.configure(bg="#21232b")

    shared_gui_refs = {
        "injector_torque_times": [],
        "injector_torque_history1": [], "injector_torque_history2": [], "injector_torque_history3": [],
        "fillhead_torque_times": [],
        "fillhead_torque_history0": [], "fillhead_torque_history1": [], "fillhead_torque_history2": [],
        "fillhead_torque_history3": [],
    }

    def get_terminal_cb():
        return shared_gui_refs.get('terminal_cb')

    send_injector_cmd = lambda msg: send_to_device("injector", msg, get_terminal_cb())
    send_fillhead_cmd = lambda msg: send_to_device("fillhead", msg, get_terminal_cb())

    def send_global_abort():
        log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", get_terminal_cb())
        send_injector_cmd("ABORT")
        send_fillhead_cmd("ABORT")

    command_funcs = {
        "abort": send_global_abort,
        "clear_errors": lambda: send_injector_cmd("CLEAR_ERRORS")
    }
    shared_widgets = create_shared_widgets(root, command_funcs)
    shared_gui_refs.update(shared_widgets)

    def update_injector_torque_plot():
        if 'injector_torque_plot_ax' not in shared_gui_refs: return
        ax = shared_gui_refs['injector_torque_plot_ax']
        canvas = shared_gui_refs['injector_torque_plot_canvas']
        lines = shared_gui_refs['injector_torque_plot_lines']
        ts = shared_gui_refs['injector_torque_times']
        histories = [shared_gui_refs['injector_torque_history1'], shared_gui_refs['injector_torque_history2'],
                     shared_gui_refs['injector_torque_history3']]
        if not ts: return
        ax.set_xlim(ts[0], ts[-1])
        for i in range(3):
            if len(ts) == len(histories[i]): lines[i].set_data(ts, histories[i])
        all_data = [item for sublist in histories if sublist for item in sublist]
        if not all_data: return
        min_y_data, max_y_data = min(all_data), max(all_data)
        nmin, nmax = max(min_y_data - 10, -10), min(max_y_data + 10, 110)
        ax.set_ylim(nmin, nmax);
        canvas.draw_idle()

    def update_fillhead_torque_plot():
        if 'fillhead_torque_plot_ax' not in shared_gui_refs: return
        ax = shared_gui_refs['fillhead_torque_plot_ax']
        canvas = shared_gui_refs['fillhead_torque_plot_canvas']
        lines = shared_gui_refs['fillhead_torque_plot_lines']
        ts = shared_gui_refs['fillhead_torque_times']
        histories = [shared_gui_refs[f'fillhead_torque_history{i}'] for i in range(4)]
        if not ts: return
        ax.set_xlim(ts[0], ts[-1])
        for i in range(4):
            if len(ts) == len(histories[i]): lines[i].set_data(ts, histories[i])
        all_data = [item for sublist in histories if sublist for item in sublist]
        if not all_data: return
        min_y_data, max_y_data = min(all_data), max(all_data)
        nmin, nmax = max(min_y_data - 10, -10), min(max_y_data + 10, 110)
        ax.set_ylim(nmin, nmax);
        canvas.draw_idle()

    def update_fillhead_visualization():
        try:
            pitch_mm = float(shared_gui_refs['fh_pitch_var'].get())
            if pitch_mm <= 0: return
        except (ValueError, KeyError):
            return

        steps_per_mm = STEPS_PER_REV / pitch_mm

        try:
            pos_x_steps = int(shared_gui_refs.get('fh_pos_m0_var').get())
            pos_y1_steps = int(shared_gui_refs.get('fh_pos_m1_var').get())
            pos_y2_steps = int(shared_gui_refs.get('fh_pos_m2_var').get())
            pos_z_steps = int(shared_gui_refs.get('fh_pos_m3_var').get())
        except (ValueError, KeyError, AttributeError):
            return

        pos_x_mm = pos_x_steps / steps_per_mm
        pos_y_mm = ((pos_y1_steps + pos_y2_steps) / 2) / steps_per_mm
        pos_z_mm = pos_z_steps / steps_per_mm

        xy_marker = shared_gui_refs.get('fh_xy_marker')
        z_bar = shared_gui_refs.get('fh_z_bar')
        ax_z = shared_gui_refs.get('fh_ax_z')
        canvas = shared_gui_refs.get('fh_pos_viz_canvas')

        if not all([xy_marker, z_bar, ax_z, canvas]): return

        xy_marker.set_data([pos_x_mm], [pos_y_mm])

        z_bar.set_height(pos_z_mm)

        min_z, max_z = ax_z.get_ylim()
        if pos_z_mm < min_z:
            ax_z.set_ylim(pos_z_mm - 10, max_z)

        canvas.draw_idle()

    shared_gui_refs['update_injector_torque_plot'] = update_injector_torque_plot
    shared_gui_refs['update_fillhead_torque_plot'] = update_fillhead_torque_plot
    shared_gui_refs['update_fillhead_viz'] = update_fillhead_visualization

    notebook = ttk.Notebook(root)

    injector_widgets = create_injector_tab(notebook, send_injector_cmd, shared_gui_refs)
    shared_gui_refs.update(injector_widgets)

    fillhead_widgets = create_fillhead_tab(notebook, send_fillhead_cmd)
    shared_gui_refs.update(fillhead_widgets)

    notebook.pack(expand=True, fill="both", padx=10, pady=5)
    shared_gui_refs['shared_bottom_frame'].pack(fill=tk.X, expand=False, padx=10, pady=(0, 10))

    threading.Thread(target=recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=monitor_connections, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=auto_discover_loop, args=(get_terminal_cb,), daemon=True).start()

    root.after(100, lambda: (
        shared_gui_refs.get('update_state', lambda x: None)(
            shared_gui_refs.get('main_state_var', tk.StringVar(value="UNKNOWN")).get()),
        shared_gui_refs.get('update_feed_buttons', lambda x: None)(
            shared_gui_refs.get('feed_state_var', tk.StringVar(value="IDLE")).get())
    ))
    root.mainloop()


if __name__ == "__main__":
    main()