import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

def build_gui(send_udp, discover):
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

    # Tkinter shared variables
    status_var = tk.StringVar(value="🔌 Not connected")
    main_state_var = tk.StringVar(value="State: Unknown")
    motor_state1 = tk.StringVar(value="Unknown")
    enabled_state1 = tk.StringVar(value="Unknown")
    torque_value1 = tk.StringVar(value="--- %")
    motor_state2 = tk.StringVar(value="Unknown")
    enabled_state2 = tk.StringVar(value="Unknown")
    torque_value2 = tk.StringVar(value="--- %")
    commanded_steps_var = tk.StringVar(value="0")
    jog_step_var = tk.StringVar(value="128")
    torque_history1, torque_history2, torque_times = [], [], []

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

    def update_torque_plot(val1, val2, times, hist1, hist2):
        torque_value1.set(f"{val1:.2f} %")
        torque_value2.set(f"{val2:.2f} %")
        if hist1 and hist2:
            ax.set_xlim(times[0], times[-1])
            line1.set_data(times, hist1)
            line2.set_data(times, hist2)
            min_y = min(min(hist1), min(hist2))
            max_y = max(max(hist1), max(hist2))
            if max_y - min_y == 0:
                ax.set_ylim(min_y - 10, min_y + 10)
            else:
                buffer = (max_y - min_y) * 0.1
                ax.set_ylim(min_y - buffer, max_y + buffer)
            fig.canvas.draw_idle()

    def terminal_cb(msg):
        terminal.insert(tk.END, msg)
        terminal.see(tk.END)

    main_frame = tk.Frame(root, bg="#21232b")
    main_frame.pack(expand=True)

    tk.Label(main_frame, textvariable=status_var, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12)).pack(pady=(10, 5))
    # --- State indicator ---
    tk.Label(main_frame, textvariable=main_state_var, bg="#21232b", fg="#b6b6ff", font=("Consolas", 11, "bold")).pack(pady=(0,5))

    button_frame = tk.Frame(main_frame, bg="#21232b")
    button_frame.pack(pady=10)
    ttk.Button(button_frame, text="Enable", command=lambda: send_udp("ENABLE"), style='Green.TButton').grid(row=0, column=0, padx=5, pady=5)
    ttk.Button(button_frame, text="Disable", command=lambda: send_udp("DISABLE"), style='Red.TButton').grid(row=0, column=1, padx=5, pady=5)
    ttk.Button(button_frame, text="Abort", command=lambda: send_udp("ABORT"), style='Red.TButton').grid(row=0, column=2, padx=5, pady=5)
    ttk.Button(button_frame, text="Standby", command=lambda: send_udp("STANDBY"), style='Blue.TButton').grid(row=0, column=3, padx=5, pady=5)
    ttk.Button(button_frame, text="Rediscover", command=discover, style='Gray.TButton').grid(row=0, column=4, padx=5, pady=5)

    jog_frame = tk.Frame(main_frame, bg="#21232b")
    jog_frame.pack(pady=(10,0))
    tk.Label(jog_frame, text="Jog Motor 1:", bg="#21232b", fg="#00bfff", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, padx=(0,5))
    jog_m0_minus = ttk.Button(jog_frame, text="◀", style="TButton", width=3, command=lambda: send_udp(f"JOG M0 - {jog_step_var.get()}"))
    jog_m0_plus = ttk.Button(jog_frame, text="▶", style="TButton", width=3, command=lambda: send_udp(f"JOG M0 + {jog_step_var.get()}"))
    jog_m0_minus.grid(row=0, column=1, padx=2)
    jog_m0_plus.grid(row=0, column=2, padx=2)
    tk.Label(jog_frame, text="Jog Motor 2:", bg="#21232b", fg="yellow", font=("Segoe UI", 11, "bold")).grid(row=0, column=3, padx=(20,5))
    jog_m1_minus = ttk.Button(jog_frame, text="◀", style="TButton", width=3, command=lambda: send_udp(f"JOG M1 - {jog_step_var.get()}"))
    jog_m1_plus = ttk.Button(jog_frame, text="▶", style="TButton", width=3, command=lambda: send_udp(f"JOG M1 + {jog_step_var.get()}"))
    jog_m1_minus.grid(row=0, column=4, padx=2)
    jog_m1_plus.grid(row=0, column=5, padx=2)

    status_frame = tk.Frame(main_frame, bg="#21232b")
    status_frame.pack(pady=10)
    tk.Label(status_frame, text="Motor 1 Status:", bg="#21232b", fg="white").grid(row=0, column=0)
    motor_status_label1 = tk.Label(status_frame, textvariable=motor_state1, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
    motor_status_label1.grid(row=0, column=1)
    tk.Label(status_frame, text="Enabled:", bg="#21232b", fg="white").grid(row=0, column=2)
    enabled_status_label1 = tk.Label(status_frame, textvariable=enabled_state1, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
    enabled_status_label1.grid(row=0, column=3)
    tk.Label(status_frame, text="Torque:", bg="#21232b", fg="white").grid(row=0, column=4)
    tk.Label(status_frame, textvariable=torque_value1, bg="#21232b", fg="#00bfff", font=("Segoe UI", 12, "bold")).grid(row=0, column=5)

    tk.Label(status_frame, text="Motor 2 Status:", bg="#21232b", fg="white").grid(row=1, column=0)
    motor_status_label2 = tk.Label(status_frame, textvariable=motor_state2, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
    motor_status_label2.grid(row=1, column=1)
    tk.Label(status_frame, text="Enabled:", bg="#21232b", fg="white").grid(row=1, column=2)
    enabled_status_label2 = tk.Label(status_frame, textvariable=enabled_state2, bg="#21232b", fg="#ffffff", font=("Segoe UI", 12, "bold"))
    enabled_status_label2.grid(row=1, column=3)
    tk.Label(status_frame, text="Torque:", bg="#21232b", fg="white").grid(row=1, column=4)
    tk.Label(status_frame, textvariable=torque_value2, bg="#21232b", fg="yellow", font=("Segoe UI", 12, "bold")).grid(row=1, column=5)
    tk.Label(status_frame, text="Machine Steps:", bg="#21232b", fg="white").grid(row=2, column=0)
    tk.Label(status_frame, textvariable=commanded_steps_var, bg="#21232b", fg="#00bfff", font=("Segoe UI", 12, "bold")).grid(row=2, column=1)

    chart_frame = tk.Frame(main_frame, bg="#21232b")
    chart_frame.pack(pady=10, fill="both", expand=True)
    canvas = FigureCanvasTkAgg(fig, master=chart_frame)
    canvas_widget = canvas.get_tk_widget()
    canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    terminal = tk.Text(main_frame, height=8, bg="#1b1e2b", fg="#00ff88", insertbackground="#ffffff", wrap="word",
                        highlightbackground="#34374b", highlightthickness=1, bd=0, font=("Consolas", 10))
    terminal.pack(fill="both", expand=True, padx=10, pady=(10, 10))

    return {
        'root': root,
        'update_status': update_status,
        'update_motor_status': update_motor_status,
        'update_enabled_status': update_enabled_status,
        'commanded_steps_var': commanded_steps_var,
        'torque_history1': torque_history1,
        'torque_history2': torque_history2,
        'torque_times': torque_times,
        'update_torque_plot': update_torque_plot,
        'terminal_cb': terminal_cb,
        'main_state_var': main_state_var,
    }
