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
        log_to_terminal(f"Discovery: '{msg}' sent to broadcast", terminal_cb_func)
    except Exception as e:
        log_to_terminal(f"Discovery error: {e}", terminal_cb_func)


def send_to_device(device_key, msg, terminal_cb_func):
    device_ip = devices[device_key].get("ip")
    if device_ip:
        try:
            sock.sendto(msg.encode(), (device_ip, CLEARCORE_PORT))
        except Exception as e:
            log_to_terminal(f"UDP send error to {device_key} ({device_ip}): {e}", terminal_cb_func)
    else:
        if "DISCOVER" not in msg:
            log_to_terminal(f"Cannot send to {device_key}: IP unknown.", terminal_cb_func)


def monitor_connections(gui_refs):
    while True:
        now = time.time()
        for key, device in devices.items():
            prev_conn_status = device["connected"]
            current_conn_status = prev_conn_status

            if current_conn_status and (now - device["last_rx"]) > TIMEOUT_THRESHOLD:
                current_conn_status = False

            if current_conn_status != prev_conn_status:
                device["connected"] = current_conn_status
                status_text = f"✅ {key.capitalize()} Connected" if current_conn_status else f"🔌 {key.capitalize()} Disconnected"
                log_to_terminal(status_text, gui_refs.get('terminal_cb'))

                if f'status_var_{key}' in gui_refs:
                    gui_refs[f'status_var_{key}'].set(status_text)

                if not current_conn_status and key == "injector":
                    if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set("UNKNOWN")
                    if 'update_state' in gui_refs: gui_refs['update_state']("UNKNOWN")

        time.sleep(HEARTBEAT_INTERVAL)


def auto_discover_loop(terminal_cb_func):
    while True:
        discover(terminal_cb_func)
        time.sleep(DISCOVERY_RETRY_INTERVAL)


def parse_injector_telemetry(msg, gui_refs):
    try:
        parts = dict(item.split(':', 1) for item in msg.split(':', 1) if ':' in item)

        # --- Update all State variables ---
        fw_main_state = parts.get("MAIN_STATE", "---")
        if 'main_state_var' in gui_refs: gui_refs['main_state_var'].set(fw_main_state)

        if 'homing_state_var' in gui_refs: gui_refs['homing_state_var'].set(parts.get("HOMING_STATE", "---"))
        if 'homing_phase_var' in gui_refs: gui_refs['homing_phase_var'].set(parts.get("HOMING_PHASE", "---"))
        if 'feed_state_var' in gui_refs: gui_refs['feed_state_var'].set(parts.get("FEED_STATE", "IDLE"))
        if 'error_state_var' in gui_refs: gui_refs['error_state_var'].set(parts.get("ERROR_STATE", "No Error"))

        # --- Update all Motor Status variables (0, 1, 2) ---
        for i in range(3):
            gui_var_index = i + 1
            if f'torque_value{gui_var_index}_var' in gui_refs: gui_refs[f'torque_value{gui_var_index}_var'].set(
                parts.get(f'torque{i}', '0.0'))
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

        torque_floats = [float(parts.get(f"torque{i}", 0.0)) for i in range(3)]
        gui_refs.get('torque_history1', []).append(torque_floats[0])
        gui_refs.get('torque_history2', []).append(torque_floats[1])
        gui_refs.get('torque_history3', []).append(torque_floats[2])
        gui_refs.get('torque_times', []).append(time.time())

        while len(gui_refs['torque_times']) > TORQUE_HISTORY_LENGTH:
            for key in ['torque_times', 'torque_history1', 'torque_history2', 'torque_history3']:
                if gui_refs.get(key): gui_refs[key].pop(0)

        if 'update_torque_plot' in gui_refs and callable(gui_refs['update_torque_plot']):
            gui_refs['update_torque_plot']()

        if 'update_state' in gui_refs and callable(gui_refs['update_state']):
            gui_refs['update_state'](fw_main_state)

    except Exception as e:
        log_to_terminal(f"Injector telemetry parse error: {e}", gui_refs.get('terminal_cb'))


def parse_fillhead_telemetry(msg, gui_refs):
    try:
        parts = dict(item.split(':', 1) for item in msg.split(',')[1:] if ':' in item)
        for axis in ['x', 'y', 'z']:
            if f'fh_pos_{axis}_var' in gui_refs: gui_refs[f'fh_pos_{axis}_var'].set(parts.get(f'pos_{axis}', '0'))
            if f'fh_torque_{axis}_var' in gui_refs: gui_refs[f'fh_torque_{axis}_var'].set(
                f"{parts.get(f'torque_{axis}', '0.0')} %")
            if f'fh_enabled_{axis}_var' in gui_refs: gui_refs[f'fh_enabled_{axis}_var'].set(
                "Enabled" if parts.get(f'enabled_{axis}', '0') == "1" else "Disabled")
            if f'fh_homed_{axis}_var' in gui_refs: gui_refs[f'fh_homed_{axis}_var'].set(
                "Homed" if parts.get(f'homed_{axis}', '0') == "1" else "Not Homed")
    except Exception as e:
        log_to_terminal(f"Fillhead telemetry parse error: {e}", gui_refs.get('terminal_cb'))


def recv_loop(gui_refs):
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode('utf-8', errors='replace').strip()

            source_ip = addr[0]
            device_key = None

            if msg.startswith("INJ_TELEM_GUI:"):
                device_key = "injector"
            elif msg.startswith("FH_TELEM_GUI:"):
                device_key = "fillhead"
            elif msg.startswith("DISCOVERED:"):
                device_key = "injector"

            if device_key:
                device = devices[device_key]
                device["ip"] = source_ip
                device["last_rx"] = time.time()

                log_to_terminal(f"[{device_key.upper()}@{source_ip}]: {msg}", gui_refs.get('terminal_cb'))

                if msg.startswith("INJ_TELEM_GUI:"):
                    parse_injector_telemetry(msg, gui_refs)
                elif msg.startswith("FH_TELEM_GUI:"):
                    parse_fillhead_telemetry(msg, gui_refs)
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
        "torque_history1": [], "torque_history2": [], "torque_history3": [], "torque_times": []
    }

    def get_terminal_cb():
        return shared_gui_refs.get('terminal_cb')

    send_injector_cmd = lambda msg: send_to_device("injector", msg, get_terminal_cb())
    send_fillhead_cmd = lambda msg: send_to_device("fillhead", msg, get_terminal_cb())

    shared_widgets = create_shared_widgets(root, send_injector_cmd)
    shared_gui_refs.update(shared_widgets)

    def update_torque_plot():
        ax = shared_gui_refs['torque_plot_ax']
        canvas = shared_gui_refs['torque_plot_canvas']
        lines = shared_gui_refs['torque_plot_lines']
        ts = shared_gui_refs['torque_times']
        histories = [shared_gui_refs['torque_history1'], shared_gui_refs['torque_history2'],
                     shared_gui_refs['torque_history3']]

        if not ts: return
        ax.set_xlim(ts[0], ts[-1])
        for i in range(3): lines[i].set_data(ts, histories[i])

        all_data = [item for sublist in histories for item in sublist]
        min_y_data = min(all_data) if all_data else -5
        max_y_data = max(all_data) if all_data else 105

        nmin = max(min_y_data - 10, -10)
        nmax = min(max_y_data + 10, 110)
        ax.set_ylim(nmin, nmax)
        canvas.draw_idle()

    shared_gui_refs['update_torque_plot'] = update_torque_plot

    notebook = ttk.Notebook(root)

    injector_widgets = create_injector_tab(notebook, send_injector_cmd, shared_gui_refs)
    shared_gui_refs.update(injector_widgets)

    fillhead_widgets = create_fillhead_tab(notebook, send_fillhead_cmd)
    shared_gui_refs.update(fillhead_widgets)

    notebook.pack(expand=True, fill="both", padx=10, pady=5)
    shared_gui_refs['shared_bottom_frame'].pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

    threading.Thread(target=recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=monitor_connections, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=auto_discover_loop, args=(get_terminal_cb(),), daemon=True).start()

    root.mainloop()


if __name__ == "__main__":
    main()