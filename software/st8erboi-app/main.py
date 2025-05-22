import socket
import threading
import time
import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

CLEARCORE_PORT = 8888
HEARTBEAT_INTERVAL = 0.3
TIMEOUT_THRESHOLD = 1.0
TORQUE_HISTORY_LENGTH = 200
PLOT_UPDATE_INTERVAL = 0.05

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 0))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(0.1)

clearcore_ip = None
clearcore_connected = False
last_pong_time = 0
last_plot_update_time = time.time()

# --- REMOVED: python_commanded_steps_offset is no longer needed ---

# --- GUI Variables ---
root = tk.Tk()
root.title("ClearCore Controller")
root.configure(bg="#2e2e2e")

style = ttk.Style()
style.theme_use('clam')

style.configure('.', background="#2e2e2e", foreground="#ffffff", fieldbackground="#3e3e3e", selectbackground="#5e5e5e")
style.configure('TButton', padding=6, font=('Segoe UI', 10, 'bold'))
style.map('TButton', background=[('active', '#4e4e4e')])

style.configure('TLabel', background="#2e2e2e", foreground="#ffffff")
style.configure('TCombobox', fieldbackground="#3e3e3e", foreground="#ffffff", selectbackground="#5e5e5e")

style.configure('Toggle.TButton', background='#5e5e5e', foreground='white', relief='flat', borderwidth=0, focuscolor='#2e2e2e')
style.map('Toggle.TButton',
          background=[('active', '#7e7e7e'), ('!disabled', '#5e7e7e')],
          foreground=[('active', 'white'), ('!disabled', 'white')]
         )

style.configure('ToggleActive.TButton', background='#00bfff', foreground='black', relief='flat', borderwidth=0, focuscolor='#2e2e2e')
style.map('ToggleActive.TButton',
          background=[('active', '#0099cc')],
          foreground=[('active', 'white'), ('!disabled', 'black')]
         )

status_var = tk.StringVar(value="🔌 Not connected")
motor_state = tk.StringVar(value="Unknown")
enabled_state = tk.StringVar(value="Unknown")
torque_value = tk.StringVar(value="--- %")
commanded_steps_var = tk.StringVar(value="0") # This will now display the custom counter
rev_var = tk.StringVar(value="1")
ppr_var = tk.StringVar(value="6400")
current_move_speed = tk.StringVar(value="FAST")
torque_limit_var = tk.StringVar(value="80.0")

# --- Plotting Variables ---
torque_history = []
torque_times = []
fig, ax = plt.subplots(figsize=(6, 2.5), facecolor='#2e2e2e')
line, = ax.plot([], [], color='#00bfff')
ax.set_facecolor('#1e1e1e')
ax.tick_params(axis='x', colors='white')
ax.tick_params(axis='y', colors='white')
ax.spines['bottom'].set_color('white')
ax.spines['left'].set_color('white')
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.set_ylabel("Torque (%)", color='white')
ax.set_ylim(-10, 110)

def update_status(text):
    status_var.set(text)

def update_motor_status(text, color="#ffffff"):
    motor_state.set(text)
    motor_status_label.config(fg=color)

def update_enabled_status(text, color="#ffffff"):
    enabled_state.set(text)
    enabled_status_label.config(fg=color)

def update_torque_display_and_plot(value_str, actual_value):
    torque_value.set(value_str)

    if torque_history:
        ax.set_xlim(torque_times[0], torque_times[-1])
        line.set_data(torque_times, torque_history)

        min_torque = min(torque_history)
        max_torque = max(torque_history)
        if max_torque - min_torque == 0:
            ax.set_ylim(min_torque - 10, min_torque + 10)
        else:
            buffer = (max_torque - min_torque) * 0.1
            ax.set_ylim(min_torque - buffer, max_torque + buffer)

        fig.canvas.draw_idle()

def send_udp(msg):
    if clearcore_ip:
        try:
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
        except Exception as e:
            update_status(f"❌ Send failed: {e}")
            print(f"Error sending UDP: {e}")

def discover():
    port = sock.getsockname()[1]
    msg = f"DISCOVER_CLEARCORE PORT={port}"
    sock.sendto(msg.encode(), ('<broadcast>', CLEARCORE_PORT))
    update_status("📡 Discovering...")

def send_heartbeat():
    if clearcore_ip:
        send_udp("PING")

def monitor_connection():
    global clearcore_connected
    while True:
        send_heartbeat()
        time.sleep(HEARTBEAT_INTERVAL)
        if time.time() - last_pong_time > TIMEOUT_THRESHOLD:
            if clearcore_connected:
                clearcore_connected = False
                update_status("🔌 Disconnected")
                update_motor_status("Unknown", "#ffffff")
                update_enabled_status("Unknown", "#ffffff")
                commanded_steps_var.set("0") # Reset steps on disconnect
        elif clearcore_ip:
            update_status(f"✅ Connected to {clearcore_ip}")

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
                update_status(f"✅ Connected to {clearcore_ip}")
            elif msg == "PONG":
                last_pong_time = time.time()
                clearcore_connected = True
            else:
                terminal.insert(tk.END, f"[{addr[0]}]: {msg}\n")
                terminal.see(tk.END)

                msg_lc = msg.lower()

                # --- MODIFIED: Parse new 'custom_pos' from telemetry ---
                if "torque:" in msg_lc and "hlfb:" in msg_lc and "enabled:" in msg_lc and "custom_pos:" in msg_lc:
                    try:
                        parts = msg_lc.replace('%', '').split(',')
                        torque_str = parts[0].split(':')[1].strip()
                        hlfb_val_str = parts[1].split(':')[1].strip()
                        enabled_val_str = parts[2].split(':')[1].strip()
                        # The 4th part is pos_cmd, 5th part is custom_pos
                        custom_pos_str = parts[4].split(':')[1].strip() # Extract custom commanded position

                        commanded_steps_var.set(custom_pos_str) # Update GUI with custom counter

                        hlfb_val = int(hlfb_val_str)
                        enabled_val = int(enabled_val_str)

                        try:
                            torque_float = float(torque_str)
                        except ValueError:
                            torque_float = 0.0

                        torque_history.append(torque_float)
                        torque_times.append(time.time())
                        if len(torque_history) > TORQUE_HISTORY_LENGTH:
                            torque_history.pop(0)
                            torque_times.pop(0)

                        current_time = time.time()
                        if current_time - last_plot_update_time >= PLOT_UPDATE_INTERVAL:
                            update_torque_display_and_plot(f"{torque_str} %", torque_float)
                            last_plot_update_time = current_time

                        if enabled_val == 0:
                            update_enabled_status("Disabled", "#ff4444")
                            update_motor_status("N/A", "#888888")
                        else:
                            update_enabled_status("Enabled", "#00ff88")
                            if hlfb_val == 1:
                                update_motor_status("At Position", "#00bfff")
                            else:
                                update_motor_status("Moving / Busy", "#ffff55")

                    except Exception as e:
                        print(f"Error parsing telemetry message: {e} - Message: {msg}")
                        pass
                # --- END MODIFIED ---

                elif "motor disabled via abort" in msg_lc:
                    update_motor_status("Aborted (Motor Disabled)", "#ff0000")
                elif "motor disabled" in msg_lc:
                    update_motor_status("Disabled", "#ff4444")
                elif "motor enabled" in msg_lc:
                    update_motor_status("Enabled", "#00ff88")
                elif "motor reset complete" in msg_lc:
                    update_motor_status("Reset Complete", "#00ff88")
                    # No need to manually reset commanded_steps_var here, C++ sends 0
                elif "move blocked" in msg_lc:
                    update_motor_status("Move Blocked", "#ff8c00")
                elif "move aborted" in msg_lc:
                    update_motor_status("Move Aborted", "#ff0000")
                elif "ppr updated" in msg_lc:
                    update_motor_status("PPR Updated", "#00bfff")
                elif "invalid ppr value" in msg_lc:
                    update_motor_status("Invalid PPR Value", "#ff4444")
                elif "rev move" in msg_lc or "fast move" in msg_lc or "slow move" in msg_lc:
                    update_motor_status("Move Command Done", "#00bfff")
                elif "torque limit set" in msg_lc:
                    limit_val = msg_lc.split(" ")[-1]
                    update_status(f"✅ Torque Limit Set: {limit_val}%")
                elif "torque limit exceeded" in msg_lc:
                    update_motor_status("TORQUE LIMIT ABORT!", "#ff0000")
                    update_status("❌ Aborted: Torque Limit Exceeded!")
                    # No need to manually reset commanded_steps_var here, C++ sends 0
                elif "motor re-enabled after torque abort" in msg_lc:
                    update_status("✅ Auto-Re-enabled after Torque Abort")
                elif "motor re-enabled after manual abort" in msg_lc:
                    update_status("✅ Auto-Re-enabled after Manual Abort")
                    # No need to manually reset commanded_steps_var here, C++ sends 0

        except socket.timeout:
            continue
        except Exception as e:
            print(f"Error in recv_loop: {e}")
            terminal.insert(tk.END, f"Error processing message: {e}\n")
            terminal.see(tk.END)

def on_start_move_command():
    send_udp(f"{current_move_speed.get().upper()} {rev_var.get()}")

def on_toggle_speed_button(speed_type):
    current_move_speed.set(speed_type)
    if speed_type == "FAST":
        fast_btn.config(style='ToggleActive.TButton')
        slow_btn.config(style='Toggle.TButton')
    else:
        slow_btn.config(style='ToggleActive.TButton')
        fast_btn.config(style='Toggle.TButton')

def on_set_torque_limit():
    try:
        limit = float(torque_limit_var.get())
        if 0.0 <= limit <= 100.0:
            send_udp(f"SET_TORQUE_LIMIT {limit:.1f}")
            update_status(f"Setting Torque Limit to {limit:.1f}%...")
        else:
            update_status("❌ Invalid Torque Limit (0-100%)")
    except ValueError:
        update_status("❌ Invalid Torque Limit (Not a number)")

def on_abort(): send_udp("ABORT")
def on_disable(): send_udp("DISABLE")
def on_enable(): send_udp("ENABLE")
def on_reset(): send_udp("RESET")
def on_ppr_change(event): send_udp(f"PPR {ppr_var.get()}")

# === GUI Layout ===

tk.Label(root, textvariable=status_var, bg="#2e2e2e", fg="#ffffff", font=("Segoe UI", 12)).pack(pady=(10, 5))

input_frame = tk.Frame(root, bg="#2e2e2e")
input_frame.pack(pady=10, padx=10, fill="x")

ttk.Label(input_frame, text="Revolutions:").grid(row=0, column=0, sticky="w", padx=(0, 5), pady=(0, 5))
tk.Entry(input_frame, textvariable=rev_var, width=8, bg="#3e3e3e", fg="#ffffff", insertbackground="#ffffff",
         highlightbackground="#5e5e5e", highlightthickness=1, bd=0, font=("Segoe UI", 10)).grid(row=0, column=1, sticky="w", padx=(0, 20), pady=(0, 5))

ttk.Label(input_frame, text="Speed:").grid(row=0, column=2, sticky="w", padx=(0, 5), pady=(0, 5))
speed_toggle_frame = ttk.Frame(input_frame, style='TFrame')
speed_toggle_frame.grid(row=0, column=3, columnspan=2, sticky="w", pady=(0, 5))

fast_btn = ttk.Button(speed_toggle_frame, text="FAST", command=lambda: on_toggle_speed_button("FAST"), style='ToggleActive.TButton', width=6)
fast_btn.pack(side="left", ipadx=5, ipady=2)
slow_btn = ttk.Button(speed_toggle_frame, text="SLOW", command=lambda: on_toggle_speed_button("SLOW"), style='Toggle.TButton', width=6)
slow_btn.pack(side="left", ipadx=5, ipady=2)

on_toggle_speed_button(current_move_speed.get())

ttk.Label(input_frame, text="PPR:").grid(row=1, column=0, sticky="w", padx=(0, 5), pady=(5,0))
ppr_menu = ttk.Combobox(input_frame, values=["6400", "800"], textvariable=ppr_var, state="readonly", width=8)
ppr_menu.grid(row=1, column=1, sticky="w", padx=(0, 20), pady=(5,0))
ppr_menu.bind("<<ComboboxSelected>>", on_ppr_change)

ttk.Label(input_frame, text="Torque Limit (%):").grid(row=1, column=2, sticky="w", padx=(0, 5), pady=(5,0))
tk.Entry(input_frame, textvariable=torque_limit_var, width=8, bg="#3e3e3e", fg="#ffffff", insertbackground="#ffffff",
         highlightbackground="#5e5e5e", highlightthickness=1, bd=0, font=("Segoe UI", 10)).grid(row=1, column=3, sticky="w", padx=(0, 5), pady=(5,0))
ttk.Button(input_frame, text="Set Limit", command=on_set_torque_limit, style='TButton').grid(row=1, column=4, sticky="w", padx=(0, 5), pady=(5,0))


status_frame = tk.Frame(root, bg="#2e2e2e")
status_frame.pack(pady=10, padx=10, fill="x")

ttk.Label(status_frame, text="Motor Status:", font=("Segoe UI", 10)).grid(row=0, column=0, sticky="w", padx=(0, 5))
motor_status_label = tk.Label(status_frame, textvariable=motor_state, bg="#2e2e2e", fg="#ffffff", font=("Segoe UI", 12, "bold"))
motor_status_label.grid(row=0, column=1, sticky="w", padx=(0, 20))

ttk.Label(status_frame, text="Enabled:", font=("Segoe UI", 10)).grid(row=0, column=2, sticky="w", padx=(0, 5))
enabled_status_label = tk.Label(status_frame, textvariable=enabled_state, bg="#2e2e2e", fg="#ffffff", font=("Segoe UI", 12, "bold"))
enabled_status_label.grid(row=0, column=3, sticky="w", padx=(0, 20))

ttk.Label(status_frame, text="Torque:", font=("Segoe UI", 10)).grid(row=0, column=4, sticky="w", padx=(0, 5))
tk.Label(status_frame, textvariable=torque_value, bg="#2e2e2e", fg="#00ff88", font=("Segoe UI", 12, "bold")).grid(row=0, column=5, sticky="w")

# Commanded Steps Display
ttk.Label(status_frame, text="Steps Cmd:", font=("Segoe UI", 10)).grid(row=1, column=0, sticky="w", padx=(0, 5), pady=(5,0))
tk.Label(status_frame, textvariable=commanded_steps_var, bg="#2e2e2e", fg="#00bfff", font=("Segoe UI", 12, "bold")).grid(row=1, column=1, sticky="w", padx=(0, 20), pady=(5,0))


chart_frame = tk.Frame(root, bg="#2e2e2e")
chart_frame.pack(pady=10, padx=10, fill="both", expand=True)

canvas = FigureCanvasTkAgg(fig, master=chart_frame)
canvas_widget = canvas.get_tk_widget()
canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

button_frame = tk.Frame(root, bg="#2e2e2e")
button_frame.pack(pady=10, padx=10)

ttk.Button(button_frame, text="START Move", command=on_start_move_command, style='TButton').grid(row=0, column=0, padx=5, pady=5, sticky="ew")
ttk.Button(button_frame, text="Abort", command=on_abort, style='TButton').grid(row=0, column=1, padx=5, pady=5, sticky="ew")

ttk.Button(button_frame, text="Enable", command=on_enable, style='TButton').grid(row=1, column=0, padx=5, pady=5, sticky="ew")
ttk.Button(button_frame, text="Disable", command=on_disable, style='TButton').grid(row=1, column=1, padx=5, pady=5, sticky="ew")

ttk.Button(button_frame, text="Reset", command=on_reset, style='TButton').grid(row=2, column=0, padx=5, pady=5, sticky="ew")
ttk.Button(button_frame, text="Rediscover", command=discover, style='TButton').grid(row=2, column=1, padx=5, pady=5, sticky="ew")

terminal = tk.Text(root, height=8, bg="#1e1e1e", fg="#00ff88", insertbackground="#ffffff", wrap="word",
                    highlightbackground="#5e5e5e", highlightthickness=1, bd=0, font=("Consolas", 10))
terminal.pack(fill="both", expand=True, padx=10, pady=(10, 10))

threading.Thread(target=recv_loop, daemon=True).start()
threading.Thread(target=monitor_connection, daemon=True).start()

discover()
root.mainloop()