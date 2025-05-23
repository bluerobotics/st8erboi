import socket
import threading
import time
import math
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
sock.bind(('', 6272))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.settimeout(0.1)

clearcore_ip = None
clearcore_connected = False
last_pong_time = 0
last_plot_update_time = time.time()

root = tk.Tk()
root.title("st8erboi app")
root.configure(bg="#21232b")

style = ttk.Style()
style.theme_use('clam')
style.configure('.', background="#21232b", foreground="#ffffff", fieldbackground="#34374b", selectbackground="#21232b")
style.configure('TButton', padding=6, font=('Segoe UI', 10, 'bold'))
style.configure('Blue.TButton', background='#0069d9', foreground='white')
style.map('Blue.TButton', background=[('active', '#3399ff')])
style.configure('Green.TButton', background='#21ba45', foreground='white')
style.map('Green.TButton', background=[('active', '#43ea5a')])
style.configure('Red.TButton', background='#db2828', foreground='white')
style.map('Red.TButton', background=[('active', '#ff4136')])
style.configure('Orange.TButton', background='#f2711c', foreground='white')
style.map('Orange.TButton', background=[('active', '#ff8000')])
style.configure('Gray.TButton', background='#767676', foreground='white')
style.map('Gray.TButton', background=[('active', '#bbbbbb')])
style.configure('Toggle.TButton', background='#2e2e2e', foreground='white')
style.map('Toggle.TButton', background=[('active', '#00bfff'), ('selected', '#00bfff')])

status_var = tk.StringVar(value="🔌 Not connected")
motor_state1 = tk.StringVar(value="Unknown")
enabled_state1 = tk.StringVar(value="Unknown")
torque_value1 = tk.StringVar(value="--- %")
motor_state2 = tk.StringVar(value="Unknown")
enabled_state2 = tk.StringVar(value="Unknown")
torque_value2 = tk.StringVar(value="--- %")
commanded_steps_var = tk.StringVar(value="0")
rev_var = tk.StringVar(value="1")
ppr_var = tk.StringVar(value="800")
torque_limit_var = tk.StringVar(value="2.0")
jog_step_var = tk.StringVar(value="128")
torque_offset_var = tk.StringVar(value="-2.4")
current_move_speed = tk.StringVar(value="FAST")

# --- Syringe/ball screw logic fields ---
ballscrew_lead_var = tk.StringVar(value="5")       # mm per rev
bore1_dia_var = tk.StringVar(value="75")           # mm
bore2_dia_var = tk.StringVar(value="33")           # mm
ml_per_rev_var = tk.StringVar(value="0.0")
move_ml_var = tk.StringVar(value="1.0")

torque_history1 = []
torque_history2 = []
torque_times = []
fig, ax = plt.subplots(figsize=(7, 3), facecolor='#21232b')
line1, = ax.plot([], [], color='#00bfff', label="Motor 1")
line2, = ax.plot([], [], color='yellow', label="Motor 2")
ax.set_facecolor('#1b1e2b')
ax.tick_params(axis='x', colors='white')
ax.tick_params(axis='y', colors='white')
ax.spines['bottom'].set_color('white')
ax.spines['left'].set_color('white')
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.set_ylabel("Torque (%)", color='white')
ax.set_ylim(-10, 110)
ax.legend(facecolor='#21232b', edgecolor='white')

def update_ml_per_rev(*args):
    try:
        lead = float(ballscrew_lead_var.get())
        dia1 = float(bore1_dia_var.get())
        dia2 = float(bore2_dia_var.get())
        area1 = math.pi * (dia1/2)**2
        area2 = math.pi * (dia2/2)**2
        ml_per_rev = ((area1 + area2) * lead) / 1000.0  # mm³ to mL
        ml_per_rev_var.set(f"{ml_per_rev:.2f}")
    except Exception:
        ml_per_rev_var.set("0.0")

ballscrew_lead_var.trace_add("write", update_ml_per_rev)
bore1_dia_var.trace_add("write", update_ml_per_rev)
bore2_dia_var.trace_add("write", update_ml_per_rev)
update_ml_per_rev()

def send_udp(msg):
    if clearcore_ip:
        try:
            sock.sendto(msg.encode(), (clearcore_ip, CLEARCORE_PORT))
        except Exception as e:
            update_status(f"❌ Send failed: {e}")

def update_status(text):
    status_var.set(text)

def update_motor_status(motor, text, color="#ffffff"):
    if motor == 1:
        motor_state1.set(text)
        motor_status_label1.config(fg=color)
    else:
        motor_state2.set(text)
        motor_status_label2.config(fg=color)

def update_enabled_status(motor, text, color="#ffffff"):
    if motor == 1:
        enabled_state1.set(text)
        enabled_status_label1.config(fg=color)
    else:
        enabled_state2.set(text)
        enabled_status_label2.config(fg=color)

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

def discover():
    port = sock.getsockname()[1]
    msg = f"DISCOVER_CLEARCORE PORT=6272"
    sock.sendto(msg.encode(), ('192.168.1.162', CLEARCORE_PORT))
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
                update_motor_status(1, "Unknown", "#ffffff")
                update_motor_status(2, "Unknown", "#ffffff")
                update_enabled_status(1, "Unknown", "#ffffff")
                update_enabled_status(2, "Unknown", "#ffffff")
                commanded_steps_var.set("0")
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
                if ("torque1:" in msg_lc and "hlfb1:" in msg_lc and "enabled1:" in msg_lc and
                    "torque2:" in msg_lc and "hlfb2:" in msg_lc and "enabled2:" in msg_lc and "custom_pos:" in msg_lc):
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
                                update_enabled_status(motor, "Disabled", "#ff4444")
                                update_motor_status(motor, "N/A", "#888888")
                            else:
                                update_enabled_status(motor, "Enabled", "#00ff88")
                                if hlfb == 1:
                                    update_motor_status(motor, "At Position", "#00bfff")
                                else:
                                    update_motor_status(motor, "Moving / Busy", "#ffff55")
                    except Exception as e:
                        print(f"Error parsing telemetry message: {e} - Message: {msg}")
        except socket.timeout:
            continue
        except Exception as e:
            print(f"Error in recv_loop: {e}")
            terminal.insert(tk.END, f"Error processing message: {e}\n")
            terminal.see(tk.END)

def on_start_move_command():
    send_udp(f"{current_move_speed.get().upper()} {rev_var.get()}")

def on_abort(): send_udp("ABORT")
def on_disable(): send_udp("DISABLE")
def on_enable(): send_udp("ENABLE")
def on_reset(): send_udp("RESET")
def on_ppr_change(event): send_udp(f"PPR {ppr_var.get()}")

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

def on_set_torque_offset():
    try:
        offset = float(torque_offset_var.get())
        send_udp(f"SET_TORQUE_OFFSET {offset:.2f}")
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

def on_move_by_ml():
    try:
        ml_per_rev = float(ml_per_rev_var.get())
        ml = float(move_ml_var.get())
        if ml_per_rev <= 0:
            update_status("❌ Invalid ml/rev calculation.")
            return
        revs = -ml / ml_per_rev  # Note the negative sign!
        if abs(revs) < 0.001:
            update_status("❌ 0 revolutions calculated.")
            return
        send_udp(f"SLOW {revs:.5f}")
        update_status(f"Moving {ml:.2f} mL ({revs:.4f} revs, slow, NEG DIR)")
    except Exception as e:
        update_status("❌ Invalid input for move by mL.")

def on_machine_home():
    send_udp("FAST 200")
    update_status("Machine home: moving -200 revs (fast).")

def on_cartridge_home():
    send_udp("FAST -200")
    update_status("Cartridge home: moving +200 revs (fast).")

main_frame = tk.Frame(root, bg="#21232b")
main_frame.pack(expand=True)

tk.Label(main_frame, textvariable=status_var, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12)).pack(pady=(10, 5))

# --- Syringe config
syringe_frame = tk.LabelFrame(main_frame, text="Syringe Parameters", bg="#21232b", fg="#ffaaff", font=("Segoe UI", 11, "bold"), bd=2, relief=tk.RIDGE, labelanchor="n")
syringe_frame.pack(pady=8, fill="x", padx=16)
tk.Label(syringe_frame, text="Ballscrew Lead (mm/rev):", bg="#21232b", fg="white").grid(row=0, column=0, padx=2, pady=2, sticky="e")
tk.Entry(syringe_frame, textvariable=ballscrew_lead_var, width=7, bg="#34374b", fg="#ffaa00", insertbackground="#ffffff").grid(row=0, column=1, padx=2, pady=2)
tk.Label(syringe_frame, text="Bore 1 Diameter (mm):", bg="#21232b", fg="white").grid(row=0, column=2, padx=2, pady=2, sticky="e")
tk.Entry(syringe_frame, textvariable=bore1_dia_var, width=7, bg="#34374b", fg="#00bfff", insertbackground="#ffffff").grid(row=0, column=3, padx=2, pady=2)
tk.Label(syringe_frame, text="Bore 2 Diameter (mm):", bg="#21232b", fg="white").grid(row=0, column=4, padx=2, pady=2, sticky="e")
tk.Entry(syringe_frame, textvariable=bore2_dia_var, width=7, bg="#34374b", fg="#ffff00", insertbackground="#ffffff").grid(row=0, column=5, padx=2, pady=2)
tk.Label(syringe_frame, text="Total mL / rev:", bg="#21232b", fg="white").grid(row=0, column=6, padx=2, pady=2, sticky="e")
tk.Label(syringe_frame, textvariable=ml_per_rev_var, bg="#21232b", fg="#ff88ff", width=8, font=("Segoe UI", 12, "bold")).grid(row=0, column=7, padx=2, pady=2)

# --- Move by mL
move_ml_frame = tk.Frame(main_frame, bg="#21232b")
move_ml_frame.pack(pady=5)
tk.Label(move_ml_frame, text="Move by mL:", bg="#21232b", fg="#ffaa00").grid(row=0, column=0)
tk.Entry(move_ml_frame, textvariable=move_ml_var, width=7, bg="#34374b", fg="#ffaa00", insertbackground="#ffffff").grid(row=0, column=1)
ttk.Button(move_ml_frame, text="Move by mL (SLOW)", command=on_move_by_ml, style='Orange.TButton').grid(row=0, column=2, padx=6)

# --- Fast/Slow Toggle (proper)
speed_frame = tk.Frame(main_frame, bg="#21232b")
speed_frame.pack(pady=8)
def toggle_speed(sel):
    current_move_speed.set(sel)
    if sel == "FAST":
        fast_btn.config(relief="sunken", bg="#00bfff")
        slow_btn.config(relief="raised", bg="#2e2e2e")
    else:
        fast_btn.config(relief="raised", bg="#2e2e2e")
        slow_btn.config(relief="sunken", bg="#ffaa00")
fast_btn = tk.Button(speed_frame, text="FAST", width=8, command=lambda: toggle_speed("FAST"), fg="white")
fast_btn.grid(row=0, column=0)
slow_btn = tk.Button(speed_frame, text="SLOW", width=8, command=lambda: toggle_speed("SLOW"), fg="white")
slow_btn.grid(row=0, column=1)
toggle_speed("FAST")

# --- Config fields, centered
config_frame = tk.Frame(main_frame, bg="#21232b")
config_frame.pack(pady=10)
tk.Label(config_frame, text="Jog Step Size:", bg="#21232b", fg="white").grid(row=0, column=0)
tk.Entry(config_frame, textvariable=jog_step_var, width=7, bg="#34374b", fg="#ffff55", insertbackground="#ffffff").grid(row=0, column=1)
tk.Label(config_frame, text="Torque Limit (%):", bg="#21232b", fg="white").grid(row=0, column=2)
tk.Entry(config_frame, textvariable=torque_limit_var, width=7, bg="#34374b", fg="#00ff88", insertbackground="#ffffff").grid(row=0, column=3)
tk.Label(config_frame, text="Torque Offset:", bg="#21232b", fg="white").grid(row=0, column=4)
tk.Entry(config_frame, textvariable=torque_offset_var, width=7, bg="#34374b", fg="#ffaa00", insertbackground="#ffffff").grid(row=0, column=5)
tk.Button(config_frame, text="Set Torque Offset", command=on_set_torque_offset, width=14, bg="#ffaa00", fg="white").grid(row=0, column=6, padx=(10,0))
tk.Button(config_frame, text="Set Limit", command=on_set_torque_limit, width=9, bg="#21ba45", fg="white").grid(row=0, column=7, padx=(10,0))

# --- Main Controls
button_frame = tk.Frame(main_frame, bg="#21232b")
button_frame.pack(pady=10)
ttk.Button(button_frame, text="START Move", command=on_start_move_command, style='Blue.TButton').grid(row=0, column=0, padx=5, pady=5)
ttk.Button(button_frame, text="Abort", command=on_abort, style='Red.TButton').grid(row=0, column=1, padx=5, pady=5)
ttk.Button(button_frame, text="Enable", command=on_enable, style='Green.TButton').grid(row=0, column=2, padx=5, pady=5)
ttk.Button(button_frame, text="Disable", command=on_disable, style='Red.TButton').grid(row=0, column=3, padx=5, pady=5)
ttk.Button(button_frame, text="Reset", command=on_reset, style='Orange.TButton').grid(row=0, column=4, padx=5, pady=5)
ttk.Button(button_frame, text="Rediscover", command=discover, style='Gray.TButton').grid(row=0, column=5, padx=5, pady=5)
ttk.Button(button_frame, text="Machine Home", command=on_machine_home, style='Orange.TButton').grid(row=1, column=0, padx=5, pady=5)
ttk.Button(button_frame, text="Cartridge Home", command=on_cartridge_home, style='Orange.TButton').grid(row=1, column=1, padx=5, pady=5)

# --- Jog Controls
jog_frame = tk.Frame(main_frame, bg="#21232b")
jog_frame.pack(pady=(10,0))
tk.Label(jog_frame, text="Jog Motor 1:", bg="#21232b", fg="#00bfff", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, padx=(0,5))
jog_m0_minus = ttk.Button(jog_frame, text="◀", style="TButton", width=3)
jog_m0_plus = ttk.Button(jog_frame, text="▶", style="TButton", width=3)
jog_m0_minus.grid(row=0, column=1, padx=2)
jog_m0_plus.grid(row=0, column=2, padx=2)
tk.Label(jog_frame, text="Jog Motor 2:", bg="#21232b", fg="yellow", font=("Segoe UI", 11, "bold")).grid(row=0, column=3, padx=(20,5))
jog_m1_minus = ttk.Button(jog_frame, text="◀", style="TButton", width=3)
jog_m1_plus = ttk.Button(jog_frame, text="▶", style="TButton", width=3)
jog_m1_minus.grid(row=0, column=4, padx=2)
jog_m1_plus.grid(row=0, column=5, padx=2)
jog_m0_minus.bind('<Button-1>', lambda e: jog_motor("M0", "-"))
jog_m0_plus.bind('<Button-1>', lambda e: jog_motor("M0", "+"))
jog_m1_minus.bind('<Button-1>', lambda e: jog_motor("M1", "-"))
jog_m1_plus.bind('<Button-1>', lambda e: jog_motor("M1", "+"))

# --- Main Status
status_frame = tk.Frame(main_frame, bg="#21232b")
status_frame.pack(pady=10)
tk.Label(status_frame, text="Revolutions:", bg="#21232b", font=("Segoe UI", 10), fg="white").grid(row=0, column=0)
tk.Entry(status_frame, textvariable=rev_var, width=8, bg="#34374b", fg="#ffffff", insertbackground="#ffffff").grid(row=0, column=1, padx=8)
tk.Label(status_frame, text="PPR:", bg="#21232b", font=("Segoe UI", 10), fg="white").grid(row=0, column=2)
ppr_menu = ttk.Combobox(status_frame, values=["6400", "800"], textvariable=ppr_var, state="readonly", width=8)
ppr_menu.grid(row=0, column=3)
ppr_menu.bind("<<ComboboxSelected>>", on_ppr_change)

tk.Label(status_frame, text="Motor 1 Status:", bg="#21232b", fg="white").grid(row=1, column=0)
motor_status_label1 = tk.Label(status_frame, textvariable=motor_state1, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
motor_status_label1.grid(row=1, column=1)
tk.Label(status_frame, text="Enabled:", bg="#21232b", fg="white").grid(row=1, column=2)
enabled_status_label1 = tk.Label(status_frame, textvariable=enabled_state1, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
enabled_status_label1.grid(row=1, column=3)
tk.Label(status_frame, text="Torque:", bg="#21232b", fg="white").grid(row=1, column=4)
tk.Label(status_frame, textvariable=torque_value1, bg="#21232b", fg="#00bfff", font=("Segoe UI", 12, "bold")).grid(row=1, column=5)

tk.Label(status_frame, text="Motor 2 Status:", bg="#21232b", fg="white").grid(row=2, column=0)
motor_status_label2 = tk.Label(status_frame, textvariable=motor_state2, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
motor_status_label2.grid(row=2, column=1)
tk.Label(status_frame, text="Enabled:", bg="#21232b", fg="white").grid(row=2, column=2)
enabled_status_label2 = tk.Label(status_frame, textvariable=enabled_state2, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
enabled_status_label2.grid(row=2, column=3)
tk.Label(status_frame, text="Torque:", bg="#21232b", fg="white").grid(row=2, column=4)
tk.Label(status_frame, textvariable=torque_value2, bg="#21232b", fg="yellow", font=("Segoe UI", 12, "bold")).grid(row=2, column=5)
tk.Label(status_frame, text="Steps Cmd:", bg="#21232b", fg="white").grid(row=3, column=0)
tk.Label(status_frame, textvariable=commanded_steps_var, bg="#21232b", fg="#00bfff", font=("Segoe UI", 12, "bold")).grid(row=3, column=1)

chart_frame = tk.Frame(main_frame, bg="#21232b")
chart_frame.pack(pady=10, fill="both", expand=True)
canvas = FigureCanvasTkAgg(fig, master=chart_frame)
canvas_widget = canvas.get_tk_widget()
canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

terminal = tk.Text(main_frame, height=8, bg="#1b1e2b", fg="#00ff88", insertbackground="#ffffff", wrap="word",
                    highlightbackground="#34374b", highlightthickness=1, bd=0, font=("Consolas", 10))
terminal.pack(fill="both", expand=True, padx=10, pady=(10, 10))

threading.Thread(target=recv_loop, daemon=True).start()
threading.Thread(target=monitor_connection, daemon=True).start()
discover()
root.mainloop()
