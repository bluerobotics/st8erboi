import socket
import threading
import time
import datetime
from gui import build_gui  # Assumes updated gui.py is in the same directory

CLEARCORE_PORT = 8888
CLIENT_PORT = 6272  # Make sure this port is not in use
HEARTBEAT_INTERVAL = 0.3  # How often to check connection status
DISCOVERY_RETRY_INTERVAL = 2.0  # How often to send discovery if not connected
TIMEOUT_THRESHOLD = 1.5  # Seconds of no telemetry before considering disconnected
TORQUE_HISTORY_LENGTH = 200

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind(('', CLIENT_PORT))
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.1)  # Non-blocking recv
except OSError as e:
    print(f"ERROR binding socket: {e}. Port {CLIENT_PORT} may be in use by another application.")
    exit()

clearcore_ip = None
last_rx_time = 0
connected = False
last_fw_main_state_for_gui_update = None  # Tracks MAIN_STATE to update GUI panels only on change


def log_to_terminal(msg, terminal_cb_func):
    timestr = datetime.datetime.now().strftime("[%H:%M:%S.%f]")[:-3]
    if terminal_cb_func:
        # Ensure GUI updates are done in the main thread if tkinter is not thread-safe
        # For simple text append, often direct call is fine, but complex changes might need root.after
        terminal_cb_func(f"{timestr} {msg}\n")
    else:
        print(f"{timestr} {msg}")


def discover(terminal_cb_func):
    msg = f"DISCOVER_TELEM PORT={CLIENT_PORT}"  # CMD_STR_DISCOVER_TELEM
    try:
        sock.sendto(msg.encode(), ('192.168.1.255', CLEARCORE_PORT))
        log_to_terminal(f"Discovery: '{msg}' sent to broadcast", terminal_cb_func)  # Re-enabled this log
    except Exception as e:
        log_to_terminal(f"Discovery error: {e}", terminal_cb_func)


def send_udp(msg, terminal_cb_func):
    if clearcore_ip:
        try:
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
            # log_to_terminal(f"Sent to {clearcore_ip}: '{msg}'", terminal_cb_func) # Optional: for debugging all sends
        except Exception as e:
            log_to_terminal(f"UDP send error to {clearcore_ip}: {e}", terminal_cb_func)
    else:
        # Log if trying to send a command other than discovery when IP is unknown
        if not msg.startswith("DISCOVER_TELEM"):
            log_to_terminal(f"Cannot send '{msg}': No ClearCore IP. Discovery active.", terminal_cb_func)
        # If you want to log that discovery is happening even when other sends are blocked:
        # else:
        #    log_to_terminal(f"Discovery active, send of '{msg}' skipped until IP is known.", terminal_cb_func)


def monitor_connection(gui_refs):
    global connected, last_rx_time, clearcore_ip, last_fw_main_state_for_gui_update
    prev_conn_status = False
    while True:
        now = time.time()
        current_conn_status = connected

        if current_conn_status and (now - last_rx_time) > TIMEOUT_THRESHOLD:
            log_to_terminal("Connection timeout.", gui_refs['terminal_cb'])
            connected = False
            current_conn_status = False  # Update for this iteration
            # No need to nullify clearcore_ip here, auto_discover will try to find it or a new one

        if current_conn_status != prev_conn_status:
            if current_conn_status:
                log_to_terminal(f"Connected to ClearCore at {clearcore_ip}", gui_refs['terminal_cb'])
                gui_refs['update_status'](f"✅ Connected to {clearcore_ip}")
            else:
                log_to_terminal("Disconnected from ClearCore.", gui_refs['terminal_cb'])
                gui_refs['update_status']("🔌 Disconnected (Timeout)")
                gui_refs['prominent_firmware_state_var'].set("DISCONNECTED")
                if gui_refs['prominent_state_display_frame'].winfo_exists():
                    gui_refs['prominent_state_display_frame'].config(bg="#500000")  # Dark Red
                if gui_refs['prominent_state_label'].winfo_exists():
                    gui_refs['prominent_state_label'].config(bg="#500000", fg="white")

                # Reset GUI fields to reflect disconnection
                gui_refs['main_state_var'].set("UNKNOWN")
                gui_refs['homing_state_var'].set("---")
                gui_refs['feed_state_var'].set("---")
                gui_refs['error_state_var'].set("---")
                gui_refs['machine_steps_var'].set("0")
                gui_refs['cartridge_steps_var'].set("0")
                gui_refs['enable_disable_var'].set("Disabled")  # Reflects inability to be enabled

                for i in [1, 2]:  # Motor 0 and Motor 1
                    gui_refs[f'motor_state{i}_var'].set("N/A")
                    gui_refs[f'enabled_state{i}_var'].set("N/A")  # Reflect actual state from telem if available
                    gui_refs[f'torque_value{i}_var'].set("--- %")
                    gui_refs[f'position_cmd{i}_var'].set("0")

                gui_refs['update_state']("UNKNOWN")  # Trigger GUI panel updates
                last_fw_main_state_for_gui_update = "UNKNOWN"

            prev_conn_status = current_conn_status
        time.sleep(HEARTBEAT_INTERVAL)


def auto_discover_loop(terminal_cb_func):
    while True:
        if not connected:
            discover(terminal_cb_func)
        time.sleep(DISCOVERY_RETRY_INTERVAL)


def parse_telemetry(msg, gui_refs):
    global last_fw_main_state_for_gui_update
    try:
        parts = {}
        for item in msg.split(','):
            if ':' in item:
                key, value = item.split(':', 1)
                parts[key.strip()] = value.strip()

        # Update GUI vars directly
        gui_refs['main_state_var'].set(parts.get("MAIN_STATE", "---"))
        gui_refs['homing_state_var'].set(parts.get("HOMING_STATE", "---"))
        gui_refs['homing_phase_var'].set(parts.get("HOMING_PHASE", "---"))
        gui_refs['feed_state_var'].set(parts.get("FEED_STATE", "---"))
        gui_refs['error_state_var'].set(parts.get("ERROR_STATE", "No Error"))

        # Motor values
        gui_refs['torque_value1_var'].set(parts.get("torque1", "--- %"))
        gui_refs['position_cmd1_var'].set(parts.get("pos_cmd1", "0"))
        gui_refs['torque_value2_var'].set(parts.get("torque2", "--- %"))
        gui_refs['position_cmd2_var'].set(parts.get("pos_cmd2", "0"))

        gui_refs['machine_steps_var'].set(parts.get("machine_steps", "N/A"))
        gui_refs['cartridge_steps_var'].set(parts.get("cartridge_steps", "N/A"))

        motors_enabled = parts.get("enabled1", "0") == "1"
        gui_refs['enable_disable_var'].set("Enabled" if motors_enabled else "Disabled")

        gui_refs['motor_state1_var'].set("Ready" if parts.get("hlfb1", "0") == "1" else "Busy")
        gui_refs['motor_state2_var'].set("Ready" if parts.get("hlfb2", "0") == "1" else "Busy")

        # Update torque plot
        current_time = time.time()
        torque1 = float(parts.get("torque1", "0.0").replace("---", "0"))
        torque2 = float(parts.get("torque2", "0.0").replace("---", "0"))
        gui_refs['torque_history1'].append(torque1)
        gui_refs['torque_history2'].append(torque2)
        gui_refs['torque_times'].append(current_time)
        gui_refs['update_torque_plot'](torque1, torque2, gui_refs['torque_times'],
                                        gui_refs['torque_history1'], gui_refs['torque_history2'])

        # Inject/Purge dispensed amount:
        dispensed_ml = parts.get("dispensed_ml", "0.00")
        current_feed_state = parts.get("FEED_STATE", "---")

        if "INJECT" in current_feed_state:
            gui_refs['inject_dispensed_ml_var'].set(f"{float(dispensed_ml):.2f} ml")
        elif "PURGE" in current_feed_state:
            gui_refs['purge_dispensed_ml_var'].set(f"{float(dispensed_ml):.2f} ml")

        # Update button states dynamically:
        injecting = "INJECT_ACTIVE" in current_feed_state or "INJECT_RESUMING" in current_feed_state
        purging = "PURGE_ACTIVE" in current_feed_state or "PURGE_RESUMING" in current_feed_state
        paused = "PAUSED" in current_feed_state

        # Inject buttons:
        gui_refs['start_inject_btn'].config(state='disabled' if injecting or paused else 'normal')
        gui_refs['pause_inject_btn'].config(state='normal' if injecting else 'disabled')
        gui_refs['resume_inject_btn'].config(state='normal' if paused and 'INJECT' in current_feed_state else 'disabled')
        gui_refs['cancel_inject_btn'].config(state='normal' if injecting or (paused and 'INJECT' in current_feed_state) else 'disabled')

        # Purge buttons:
        gui_refs['start_purge_btn'].config(state='disabled' if purging or paused else 'normal')
        gui_refs['pause_purge_btn'].config(state='normal' if purging else 'disabled')
        gui_refs['resume_purge_btn'].config(state='normal' if paused and 'PURGE' in current_feed_state else 'disabled')
        gui_refs['cancel_purge_btn'].config(state='normal' if purging or (paused and 'PURGE' in current_feed_state) else 'disabled')

        # Trigger GUI update if state changed:
        fw_main_state = parts.get("MAIN_STATE", "---")
        if fw_main_state != last_fw_main_state_for_gui_update:
            gui_refs['update_state'](fw_main_state)
            last_fw_main_state_for_gui_update = fw_main_state

    except Exception as e:
        gui_refs['terminal_cb'](f"Telemetry parse error: {e}\n")



def recv_loop(gui_refs):
    global clearcore_ip, connected, last_rx_time
    while True:
        try:
            data, addr = sock.recvfrom(1024)  # Buffer size
            msg = data.decode('utf-8', errors='replace').strip()

            # Check for a key telemetry field to identify valid packets
            if "MAIN_STATE:" in msg and "torque1:" in msg:  # A more robust check for telemetry
                if not connected or clearcore_ip != addr[0]:
                    clearcore_ip = addr[0]  # Learn or update ClearCore IP
                    # No need to log "New ClearCore IP" here as monitor_connection handles "Connected" log
                connected = True
                last_rx_time = time.time()
                parse_telemetry(msg, gui_refs)
            else:
                # Log other messages from the known ClearCore IP
                if clearcore_ip and addr[0] == clearcore_ip:
                    log_to_terminal(f"[CC@{addr[0]}]: {msg}", gui_refs['terminal_cb'])
                # else: # Optional: Log messages from unknown IPs
                # log_to_terminal(f"[Unknown@{addr[0]}]: {msg}", gui_refs['terminal_cb'])

        except socket.timeout:
            continue  # Normal for non-blocking socket
        except Exception as e:
            log_to_terminal(f"Recv_loop error: {e}", gui_refs['terminal_cb'])
            time.sleep(0.1)  # Brief pause on other errors


def main():
    shared_gui_refs = {}

    gui = build_gui(
        lambda msg_to_send: send_udp(msg_to_send, shared_gui_refs.get('terminal_cb')),
        lambda: discover(shared_gui_refs.get('terminal_cb'))
    )
    shared_gui_refs.update(gui)

    threading.Thread(target=recv_loop, args=(gui,), daemon=True).start()
    threading.Thread(target=monitor_connection, args=(gui,), daemon=True).start()
    threading.Thread(target=auto_discover_loop, args=(gui.get('terminal_cb'),), daemon=True).start()

    gui['root'].mainloop()


if __name__ == "__main__":
    main()