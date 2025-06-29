import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg


def create_fillhead_tab(notebook, send_fillhead_cmd):
    fillhead_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(fillhead_tab, text='  Fillhead  ')

    ui_elements = {}

    top_level_frame = tk.Frame(fillhead_tab, bg="#21232b")
    top_level_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    fh_controls_frame = tk.Frame(top_level_frame, bg="#21232b")
    fh_controls_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, padx=5, pady=5)

    fh_status_frame = tk.Frame(top_level_frame, bg="#21232b")
    fh_status_frame.pack(side=tk.LEFT, fill=tk.Y, expand=True, padx=5, pady=5)

    # --- JOG CONTROLS (Y-axis commands control both Y motors via firmware) ---
    fh_jog_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead Jog", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_jog_frame.pack(fill=tk.X, pady=5, anchor='n')

    fh_jog_steps_var = tk.StringVar(value="1000")
    fh_jog_vel_var = tk.StringVar(value="8000")
    tk.Label(fh_jog_frame, text="Steps:", bg="#2a2d3b", fg="white").grid(row=0, column=0, padx=5)
    ttk.Entry(fh_jog_frame, textvariable=fh_jog_steps_var, width=8).grid(row=0, column=1)
    tk.Label(fh_jog_frame, text="Speed (sps):", bg="#2a2d3b", fg="white").grid(row=0, column=2, padx=5)
    ttk.Entry(fh_jog_frame, textvariable=fh_jog_vel_var, width=8).grid(row=0, column=3)

    jog_btn_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b")
    jog_btn_frame.grid(row=1, column=0, columnspan=4, pady=5)
    ttk.Button(jog_btn_frame, text="X-",
               command=lambda: send_fillhead_cmd(f"MOVE_X -{fh_jog_steps_var.get()} {fh_jog_vel_var.get()}")).pack(
        side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="X+",
               command=lambda: send_fillhead_cmd(f"MOVE_X {fh_jog_steps_var.get()} {fh_jog_vel_var.get()}")).pack(
        side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Y-",
               command=lambda: send_fillhead_cmd(f"MOVE_Y -{fh_jog_steps_var.get()} {fh_jog_vel_var.get()}")).pack(
        side=tk.LEFT, padx=10)
    ttk.Button(jog_btn_frame, text="Y+",
               command=lambda: send_fillhead_cmd(f"MOVE_Y {fh_jog_steps_var.get()} {fh_jog_vel_var.get()}")).pack(
        side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Z-",
               command=lambda: send_fillhead_cmd(f"MOVE_Z -{fh_jog_steps_var.get()} {fh_jog_vel_var.get()}")).pack(
        side=tk.LEFT, padx=10)
    ttk.Button(jog_btn_frame, text="Z+",
               command=lambda: send_fillhead_cmd(f"MOVE_Z {fh_jog_steps_var.get()} {fh_jog_vel_var.get()}")).pack(
        side=tk.LEFT)

    # --- HOMING CONTROLS (Y-axis command controls both Y motors via firmware) ---
    fh_home_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead Homing", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_home_frame.pack(fill=tk.X, pady=5, anchor='n')
    ttk.Button(fh_home_frame, text="Home X", command=lambda: send_fillhead_cmd("HOME_X")).pack(side=tk.LEFT,
                                                                                               expand=True, fill=tk.X)
    ttk.Button(fh_home_frame, text="Home Y", command=lambda: send_fillhead_cmd("HOME_Y")).pack(side=tk.LEFT,
                                                                                               expand=True, fill=tk.X)
    ttk.Button(fh_home_frame, text="Home Z", command=lambda: send_fillhead_cmd("HOME_Z")).pack(side=tk.LEFT,
                                                                                               expand=True, fill=tk.X)

    # --- NEW MOTOR-CENTRIC STATUS DISPLAY ---
    motor_map = ["Motor 0 (X)", "Motor 1 (Y1)", "Motor 2 (Y2)", "Motor 3 (Z)"]
    for i, name in enumerate(motor_map):
        frame = tk.LabelFrame(fh_status_frame, text=name, bg="#2a2d3b", fg="white", padx=5, pady=2)
        frame.pack(fill=tk.X, pady=2, anchor='n')

        pos_var = tk.StringVar(value="0")
        ui_elements[f'fh_pos_m{i}_var'] = pos_var
        torque_var = tk.StringVar(value="0.0 %")
        ui_elements[f'fh_torque_m{i}_var'] = torque_var
        enabled_var = tk.StringVar(value="Disabled")
        ui_elements[f'fh_enabled_m{i}_var'] = enabled_var
        homed_var = tk.StringVar(value="Not Homed")
        ui_elements[f'fh_homed_m{i}_var'] = homed_var

        frame.grid_columnconfigure(1, weight=1)
        tk.Label(frame, text="Position:", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="w")
        tk.Label(frame, textvariable=pos_var, bg="#2a2d3b", fg="cyan").grid(row=0, column=1, sticky="w")
        tk.Label(frame, text="Torque:", bg="#2a2d3b", fg="white").grid(row=1, column=0, sticky="w")
        tk.Label(frame, textvariable=torque_var, bg="#2a2d3b", fg="cyan").grid(row=1, column=1, sticky="w")
        tk.Label(frame, text="Enabled:", bg="#2a2d3b", fg="white").grid(row=2, column=0, sticky="w")
        tk.Label(frame, textvariable=enabled_var, bg="#2a2d3b", fg="lightgreen").grid(row=2, column=1, sticky="w")
        tk.Label(frame, text="Homed:", bg="#2a2d3b", fg="white").grid(row=3, column=0, sticky="w")
        tk.Label(frame, textvariable=homed_var, bg="#2a2d3b", fg="lightgreen").grid(row=3, column=1, sticky="w")

    # --- UPDATED 4-MOTOR TORQUE PLOT ---
    plot_frame = tk.Frame(fillhead_tab, bg="#21232b")
    plot_frame.pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=(10, 0))

    fig, ax = plt.subplots(figsize=(7, 2.5), facecolor='#21232b')

    colors = ['#00bfff', 'yellow', '#ffed72', '#ff8888']  # M0, M1, M2, M3
    lines = []
    for i in range(4):
        line, = ax.plot([], [], color=colors[i], label=f"M{i}")
        lines.append(line)

    ax.set_title("Fillhead Torque", color='white')
    ax.set_facecolor('#1b1e2b')
    ax.tick_params(axis='x', colors='white')
    ax.tick_params(axis='y', colors='white')
    ax.spines['bottom'].set_color('white')
    ax.spines['left'].set_color('white')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.set_ylabel("Torque (%)", color='white')
    ax.set_ylim(-10, 110)
    ax.legend(facecolor='#21232b', edgecolor='white', labelcolor='white')
    canvas = FigureCanvasTkAgg(fig, master=plot_frame)
    canvas_widget = canvas.get_tk_widget()
    canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    ui_elements['fillhead_torque_plot_canvas'] = canvas
    ui_elements['fillhead_torque_plot_ax'] = ax
    ui_elements['fillhead_torque_plot_lines'] = lines

    return ui_elements