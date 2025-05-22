import socket
import threading
import time
import tkinter as tk
import tkinter.font as tkfont
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

CLEARCORE_PORT = 8888
PYTHON_LOCAL_PORT = 6272
CLEARCORE_IP = '192.168.1.162'  # <-- Set to your ClearCore's IP address

HEARTBEAT_INTERVAL = 0.3
TIMEOUT_THRESHOLD = 1.0
TORQUE_HISTORY_LENGTH = 200
PLOT_UPDATE_INTERVAL = 0.05

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', PYTHON_LOCAL_PORT))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(0.1)

clearcore_ip = None
clearcore_connected = False
last_pong_time = 0
last_plot_update_time = time.time()

root = tk.Tk()
root.title("st8erboi app")
root.configure(bg="#181818")

font_obj = tkfont.Font(family="Segoe UI", size=12)
font_bold = tkfont.Font(family="Segoe UI", size=12, weight="bold")
font_mono = tkfont.Font(family="Consolas", size=11)

style = ttk.Style()
style.theme_use('clam')
style.configure('.', background="#181818", foreground="#fff", fieldbackground="#242424", selectbackground="#242424")
style.configure('TButton', padding=8, font=('Segoe UI', 11, 'bold'), foreground="#fff", background="#181818")
style.map('TButton', foreground=[('active', '#fff')], background=[('active', '#242424')])
style.configure('TLabel', background="#181818", foreground="#fff")

status_var = tk.StringVar(value="🔌 Not connected")
motor_state1 = tk.StringVar(value="Unknown")
enabled_state1 = tk.StringVar(value="Unknown")
hlfb_state1 = tk.StringVar(value="---")
torque_value1 = tk.StringVar(value="--- %")
motor_state2 = tk.StringVar(value="Unknown")
enabled_state2 = tk.StringVar(value="Unknown")
hlfb_state2 = tk.StringVar(value="---")
torque_value2 = tk.StringVar(value="--- %")
commanded_steps_var = tk.StringVar(value="0")
rev_var = tk.StringVar(value="1")
ppr_var = tk.StringVar(value="6400")
torque_limit_var = tk.StringVar(value="10.0")
jog_step_var = tk.StringVar(value="128")
torque_offset_var = tk.StringVar(value="-2.4")
current_move_speed = tk.StringVar(value="FAST")

torque_history1 = []
torque_history2 = []
torque_times = []
fig, ax = plt.subplots(figsize=(8, 3.2), facecolor='#181818')
line1, = ax.plot([], [], color='#00bfff', label="Motor 1")
line2, = ax.plot([], [], color='yellow', label="Motor 2")
ax.set_facecolor('#111')
ax.tick_params(axis='x', colors='white')
ax.tick_params(axis='y', colors='white')
ax.spines['bottom'].set_color('white')
ax.spines['left'].set_color('white')
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.set_ylabel("Torque (%)", color='white')
ax.set_ylim(-110, 110)
ax.legend(facecolor='#181818', edgecolor='white', labelcolor='white')

def log_debug(msg):
    terminal.insert(tk.END, f"{msg}\n")
    terminal.see(tk.END)

def set_motor_status(motor, status, color):
    if motor == 1:
        motor_state1.set(status)
        motor_status_label1.config(fg=color)
    else:
        motor_state2.set(status)
        motor_status_label2.config(fg=color)

def set_enabled_status(motor, status, color):
    if motor == 1:
        enabled_state1.set(status)
        enabled_status_label1.config(fg=color)
    else:
        enabled_state2.set(status)
        enabled_status_label2.config(fg=color)

def set_hlfb_status(motor, value):
    if motor == 1:
        hlfb_state1.set(value)
    else:
        hlfb_state2.set(value)

def update_status(text):
    status_var.set(text)
    log_debug(f"{text}")

def update_torque_display_and_plot(val1, val2):
    torque_value1.set(val1)
    torque_value2.set(val2)
    if torque_history1 and torque_history2:
        ax.set_xlim(torque_times[0], torque_times[-1])
        line1.set_data(torque_times, torque_history1)
        line2.set_data(torque_times, torque_history2)
        min_y = min(min(torque_history1), min(torque_history2))
        max_y = max(max(torque_history1), max(torque_history2))
        if max_y - min_y == 0:
            ax.set_ylim(min_y - 10, min_y + 10)
        else:
            buffer = (max_y - min_y) * 0.1
            ax.set_ylim(min_y - buffer, max_y + buffer)
        fig.canvas.draw_idle()

def send_udp(msg):
    if clearcore_ip:
        try:
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
        except Exception as e:
            update_status(f"❌ Send failed: {e}")

def discover():
    port = sock.getsockname()[1]
    msg = f"DISCOVER_CLEARCORE PORT={PYTHON_LOCAL_PORT}"
    log_debug(f"Sending discovery to {CLEARCORE_IP}:{CLEARCORE_PORT} from local {port}")
    try:
        sock.sendto(msg.encode(), (CLEARCORE_IP, CLEARCORE_PORT))
        update_status("📡 Discovering...")
        log_debug("Discovery packet sent OK")
    except Exception as e:
        log_debug(f"Error sending discovery: {e}")
    root.after(2000, check_discovery_timeout)

def check_discovery_timeout():
    if not clearcore_connected:
        log_debug("No CLEARCORE_ACK after 2s")
        update_status("❌ Discovery timeout (no ACK)")

def send_heartbeat():
    if clearcore_ip:
        send_udp("PING")

def monitor_connection():
    global clearcore_connected
    was_connected = False  # Track previous connection state
    while True:
        send_heartbeat()
        time.sleep(HEARTBEAT_INTERVAL)
        connected_now = (time.time() - last_pong_time <= TIMEOUT_THRESHOLD)
        if not connected_now:
            if clearcore_connected:
                clearcore_connected = False
                update_status("🔌 Disconnected")
                set_motor_status(1, "Unknown", "#fff")
                set_motor_status(2, "Unknown", "#fff")
                set_enabled_status(1, "Unknown", "#fff")
                set_enabled_status(2, "Unknown", "#fff")
                set_hlfb_status(1, "---")
                set_hlfb_status(2, "---")
                commanded_steps_var.set("0")
            was_connected = False
        else:
            if not was_connected:
                update_status(f"✅ Connected to {clearcore_ip}")
            was_connected = True
            clearcore_connected = True


def recv_loop():
    global clearcore_ip, clearcore_connected, last_pong_time, last_plot_update_time
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode().strip()
            if msg == "CLEARCORE_ACK":
                clearcore_ip = addr[0]
                last_pong_time = time.time()
                clearcore_connected = True
                update_status(f"✅ Connected to {clearcore_ip} (port {addr[1]})")
                log_debug("Connected to ClearCore.")
            elif msg == "PONG":
                last_pong_time = time.time()
                clearcore_connected = True
            else:
                msg_lc = msg.lower()
                is_telemetry = (
                    "torque1:" in msg_lc and "hlfb1:" in msg_lc and "enabled1:" in msg_lc and
                    "torque2:" in msg_lc and "hlfb2:" in msg_lc and "enabled2:" in msg_lc and "custom_pos:" in msg_lc
                )
                if not is_telemetry:
                    terminal.insert(tk.END, f"[{addr[0]}]: {msg}\n")
                    terminal.see(tk.END)
                # Always parse telemetry for status/graph, but do not log it!
                if is_telemetry:
                    try:
                        parts = [p.strip() for p in msg.replace('%','').split(',')]
                        torque1 = float(parts[0].split(':')[1].strip()) if parts[0].split(':')[1].strip() != "---" else 0.0
                        hlfb1 = int(parts[1].split(':')[1].strip())
                        enabled1 = int(parts[2].split(':')[1].strip())
                        poscmd1 = int(parts[3].split(':')[1].strip())
                        torque2 = float(parts[4].split(':')[1].strip()) if parts[4].split(':')[1].strip() != "---" else 0.0
                        hlfb2 = int(parts[5].split(':')[1].strip())
                        enabled2 = int(parts[6].split(':')[1].strip())
                        poscmd2 = int(parts[7].split(':')[1].strip())
                        custom_pos = int(parts[8].split(':')[1].strip())
                        commanded_steps_var.set(str(custom_pos))
                        set_hlfb_status(1, str(hlfb1))
                        set_hlfb_status(2, str(hlfb2))

                        torque_history1.append(torque1)
                        torque_history2.append(torque2)
                        torque_times.append(time.time())
                        if len(torque_history1) > TORQUE_HISTORY_LENGTH:
                            torque_history1.pop(0)
                            torque_history2.pop(0)
                            torque_times.pop(0)

                        current_time = time.time()
                        if current_time - last_plot_update_time >= PLOT_UPDATE_INTERVAL:
                            update_torque_display_and_plot(f"{torque1:.2f} %", f"{torque2:.2f} %")
                            last_plot_update_time = current_time

                        for motor, hlfb, enabled in [(1, hlfb1, enabled1), (2, hlfb2, enabled2)]:
                            if enabled == 0:
                                set_enabled_status(motor, "Disabled", "#ff4444")
                                set_motor_status(motor, "N/A", "#888888")
                            else:
                                set_enabled_status(motor, "Enabled", "#00ff88")
                                if hlfb == 1:
                                    set_motor_status(motor, "At Position", "#00bfff")
                                else:
                                    set_motor_status(motor, "Moving / Busy", "#ffff55")
                    except Exception as e:
                        print(f"Error parsing telemetry message: {e} - Message: {msg}")
        except socket.timeout:
            continue
        except Exception as e:
            log_debug(f"Exception in recv_loop: {e}")
            terminal.insert(tk.END, f"Error processing message: {e}\n")
            terminal.see(tk.END)

def on_start_move_command():
    send_udp(f"{current_move_speed.get().upper()} {rev_var.get()}")
    log_debug(f"Sent MOVE: {current_move_speed.get().upper()} {rev_var.get()}")

def on_abort():
    send_udp("ABORT")
    log_debug("Sent ABORT")

def on_disable():
    send_udp("DISABLE")
    log_debug("Sent DISABLE")

def on_enable():
    send_udp("ENABLE")
    log_debug("Sent ENABLE")

def on_reset():
    send_udp("RESET")
    log_debug("Sent RESET")

def on_ppr_change(event):
    send_udp(f"PPR {ppr_var.get()}")
    log_debug(f"Set PPR: {ppr_var.get()}")

def on_set_torque_limit():
    try:
        limit = float(torque_limit_var.get())
        if 0.0 <= limit <= 100.0:
            send_udp(f"SET_TORQUE_LIMIT {limit:.1f}")
            log_debug(f"Set Torque Limit: {limit:.1f}%")
            update_status(f"Setting Torque Limit to {limit:.1f}%...")
        else:
            update_status("❌ Invalid Torque Limit (0-100%)")
    except ValueError:
        update_status("❌ Invalid Torque Limit (Not a number)")

def on_set_torque_offset():
    try:
        offset = float(torque_offset_var.get())
        send_udp(f"SET_TORQUE_OFFSET {offset:.2f}")
        log_debug(f"Set Torque Offset: {offset:.2f}")
        update_status(f"Setting Torque Offset to {offset:.2f}")
    except ValueError:
        update_status("❌ Invalid Torque Offset (Not a number)")

def jog_motor(motor, direction):
    try:
        steps = int(jog_step_var.get())
        if steps < 1 or steps > 100000:
            update_status("Jog steps out of range.")
            return
    except ValueError:
        update_status("Jog steps not valid!")
        return
    send_udp(f"JOG {motor} {direction} {steps}")
    log_debug(f"Jog {motor} {direction} {steps}")

main_frame = tk.Frame(root, bg="#181818")
main_frame.pack(expand=True)

tk.Label(main_frame, textvariable=status_var, bg="#181818", fg="#fff", font=font_bold).pack(pady=(8,6))

speed_frame = tk.Frame(main_frame, bg="#181818")
speed_frame.pack(pady=6)
def toggle_speed(sel):
    current_move_speed.set(sel)
    if sel == "FAST":
        fast_btn.config(relief="sunken", bg="#00bfff", fg="#fff")
        slow_btn.config(relief="raised", bg="#222", fg="#fff")
    else:
        fast_btn.config(relief="raised", bg="#222", fg="#fff")
        slow_btn.config(relief="sunken", bg="#ffaa00", fg="#fff")
fast_btn = tk.Button(speed_frame, text="FAST", width=8, command=lambda: toggle_speed("FAST"), font=font_bold, fg="#fff")
fast_btn.pack(side="left", padx=2)
slow_btn = tk.Button(speed_frame, text="SLOW", width=8, command=lambda: toggle_speed("SLOW"), font=font_bold, fg="#fff")
slow_btn.pack(side="left", padx=2)
toggle_speed("FAST")

config_frame = tk.Frame(main_frame, bg="#181818")
config_frame.pack(pady=2)
tk.Label(config_frame, text="Jog Step Size:", bg="#181818", fg="#fff", font=font_obj).pack(side="left", padx=2)
tk.Entry(config_frame, textvariable=jog_step_var, width=7, bg="#222", fg="#fff", insertbackground="#fff", font=font_obj).pack(side="left", padx=2)
tk.Label(config_frame, text="Torque Limit (%):", bg="#181818", fg="#fff", font=font_obj).pack(side="left", padx=2)
tk.Entry(config_frame, textvariable=torque_limit_var, width=7, bg="#222", fg="#fff", insertbackground="#fff", font=font_obj).pack(side="left", padx=2)
tk.Label(config_frame, text="Torque Offset:", bg="#181818", fg="#fff", font=font_obj).pack(side="left", padx=2)
tk.Entry(config_frame, textvariable=torque_offset_var, width=7, bg="#222", fg="#fff", insertbackground="#fff", font=font_obj).pack(side="left", padx=2)
tk.Button(config_frame, text="Set Torque Offset", command=on_set_torque_offset, font=font_bold, bg="#ffaa00", fg="#fff").pack(side="left", padx=(12,2))
tk.Button(config_frame, text="Set Limit", command=on_set_torque_limit, font=font_bold, bg="#21ba45", fg="#fff").pack(side="left", padx=2)

button_frame = tk.Frame(main_frame, bg="#181818")
button_frame.pack(pady=(2,6))
tk.Button(button_frame, text="START Move", command=on_start_move_command, font=font_bold, bg="#0069d9", fg="#fff").pack(side="left", padx=5, pady=5)
tk.Button(button_frame, text="Abort", command=on_abort, font=font_bold, bg="#db2828", fg="#fff").pack(side="left", padx=5, pady=5)
tk.Button(button_frame, text="Enable", command=on_enable, font=font_bold, bg="#21ba45", fg="#fff").pack(side="left", padx=5, pady=5)
tk.Button(button_frame, text="Disable", command=on_disable, font=font_bold, bg="#db2828", fg="#fff").pack(side="left", padx=5, pady=5)
tk.Button(button_frame, text="Reset", command=on_reset, font=font_bold, bg="#f2711c", fg="#fff").pack(side="left", padx=5, pady=5)
tk.Button(button_frame, text="Rediscover", command=discover, font=font_bold, bg="#767676", fg="#fff").pack(side="left", padx=5, pady=5)

jog_frame = tk.Frame(main_frame, bg="#181818")
jog_frame.pack(pady=(2,2))
tk.Label(jog_frame, text="Jog Motor 1:", bg="#181818", fg="#fff", font=font_bold).pack(side="left", padx=(0,6))
jog_m0_minus = tk.Button(jog_frame, text="◀", width=3, font=font_bold, fg="#fff", bg="#181818", command=lambda: jog_motor("M0", "-"))
jog_m0_plus = tk.Button(jog_frame, text="▶", width=3, font=font_bold, fg="#fff", bg="#181818", command=lambda: jog_motor("M0", "+"))
jog_m0_minus.pack(side="left", padx=2)
jog_m0_plus.pack(side="left", padx=2)
tk.Label(jog_frame, text="Jog Motor 2:", bg="#181818", fg="#fff", font=font_bold).pack(side="left", padx=(20,6))
jog_m1_minus = tk.Button(jog_frame, text="◀", width=3, font=font_bold, fg="#fff", bg="#181818", command=lambda: jog_motor("M1", "-"))
jog_m1_plus = tk.Button(jog_frame, text="▶", width=3, font=font_bold, fg="#fff", bg="#181818", command=lambda: jog_motor("M1", "+"))
jog_m1_minus.pack(side="left", padx=2)
jog_m1_plus.pack(side="left", padx=2)

status_frame = tk.Frame(main_frame, bg="#181818")
status_frame.pack(pady=(8,0))

tk.Label(status_frame, text="Revolutions:", bg="#181818", fg="#fff", font=font_obj).grid(row=0, column=0)
tk.Entry(status_frame, textvariable=rev_var, width=8, bg="#222", fg="#fff", insertbackground="#fff", font=font_obj).grid(row=0, column=1, padx=8)
tk.Label(status_frame, text="PPR:", bg="#181818", fg="#fff", font=font_obj).grid(row=0, column=2)
ppr_menu = ttk.Combobox(status_frame, values=["6400", "800"], textvariable=ppr_var, state="readonly", width=8)
ppr_menu.grid(row=0, column=3)
ppr_menu.bind("<<ComboboxSelected>>", on_ppr_change)

tk.Label(status_frame, text="Motor 1 Status:", bg="#181818", fg="#fff", font=font_obj).grid(row=1, column=0)
motor_status_label1 = tk.Label(status_frame, textvariable=motor_state1, bg="#181818", fg="#00bfff", font=font_bold)
motor_status_label1.grid(row=1, column=1)
tk.Label(status_frame, text="Enabled:", bg="#181818", fg="#fff", font=font_obj).grid(row=1, column=2)
enabled_status_label1 = tk.Label(status_frame, textvariable=enabled_state1, bg="#181818", fg="#00ff88", font=font_bold)
enabled_status_label1.grid(row=1, column=3)
tk.Label(status_frame, text="HLFB:", bg="#181818", fg="#fff", font=font_obj).grid(row=1, column=4)
tk.Label(status_frame, textvariable=hlfb_state1, bg="#181818", fg="#fff", font=font_bold).grid(row=1, column=5)
tk.Label(status_frame, text="Torque:", bg="#181818", fg="#fff", font=font_obj).grid(row=1, column=6)
tk.Label(status_frame, textvariable=torque_value1, bg="#181818", fg="#00bfff", font=font_bold).grid(row=1, column=7)

tk.Label(status_frame, text="Motor 2 Status:", bg="#181818", fg="#fff", font=font_obj).grid(row=2, column=0)
motor_status_label2 = tk.Label(status_frame, textvariable=motor_state2, bg="#181818", fg="yellow", font=font_bold)
motor_status_label2.grid(row=2, column=1)
tk.Label(status_frame, text="Enabled:", bg="#181818", fg="#fff", font=font_obj).grid(row=2, column=2)
enabled_status_label2 = tk.Label(status_frame, textvariable=enabled_state2, bg="#181818", fg="#00ff88", font=font_bold)
enabled_status_label2.grid(row=2, column=3)
tk.Label(status_frame, text="HLFB:", bg="#181818", fg="#fff", font=font_obj).grid(row=2, column=4)
tk.Label(status_frame, textvariable=hlfb_state2, bg="#181818", fg="#fff", font=font_bold).grid(row=2, column=5)
tk.Label(status_frame, text="Torque:", bg="#181818", fg="#fff", font=font_obj).grid(row=2, column=6)
tk.Label(status_frame, textvariable=torque_value2, bg="#181818", fg="yellow", font=font_bold).grid(row=2, column=7)
tk.Label(status_frame, text="Steps Cmd:", bg="#181818", fg="#fff", font=font_obj).grid(row=3, column=0)
tk.Label(status_frame, textvariable=commanded_steps_var, bg="#181818", fg="#00bfff", font=font_bold).grid(row=3, column=1)

chart_frame = tk.Frame(main_frame, bg="#181818")
chart_frame.pack(pady=6, fill="both", expand=True)
canvas = FigureCanvasTkAgg(fig, master=chart_frame)
canvas_widget = canvas.get_tk_widget()
canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

terminal = tk.Text(main_frame, height=8, bg="#1b1e2b", fg="#00ff88", insertbackground="#fff", wrap="word",
                    highlightbackground="#34374b", highlightthickness=1, bd=0, font=font_mono)
terminal.pack(fill="both", expand=True, padx=10, pady=(10, 10))

threading.Thread(target=recv_loop, daemon=True).start()
threading.Thread(target=monitor_connection, daemon=True).start()
discover()
root.mainloop()
