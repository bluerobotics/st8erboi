import tkinter as tk
from tkinter import ttk
# Removed matplotlib imports
import numpy as np


def create_torque_bar(parent, label_text, bar_color):
    """Helper function to create a single torque bar indicator."""
    frame = tk.Frame(parent, bg=parent['bg'])

    label = tk.Label(frame, text=label_text, bg=parent['bg'], fg='white', font=('Segoe UI', 8))
    label.pack(pady=(0, 2))

    canvas = tk.Canvas(frame, width=40, height=120, bg='#1b1e2b', highlightthickness=0)
    canvas.pack()

    canvas.create_rectangle(10, 10, 30, 110, fill='#333', outline='')
    bar_item = canvas.create_rectangle(10, 110, 30, 110, fill=bar_color, outline='')
    text_item = canvas.create_text(20, 60, text="0%", fill='white', font=('Segoe UI', 9, 'bold'))

    return frame, canvas, bar_item, text_item


def create_fillhead_tab(notebook, send_fillhead_cmd, shared_gui_refs):
    fillhead_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(fillhead_tab, text='  Fillhead  ')

    ui_elements = {}

    for i in range(4):
        ui_elements[f'fh_pos_m{i}_var'] = tk.StringVar(value="0.00")
        ui_elements[f'fh_torque_m{i}_var'] = tk.StringVar(value="0.0")  # Store raw float
        ui_elements[f'fh_enabled_m{i}_var'] = tk.StringVar(value="Disabled")
        ui_elements[f'fh_homed_m{i}_var'] = tk.StringVar(value="Not Homed")

    ui_elements['fh_state_x_var'] = tk.StringVar(value="UNKNOWN")
    ui_elements['fh_state_y_var'] = tk.StringVar(value="UNKNOWN")
    ui_elements['fh_state_z_var'] = tk.StringVar(value="UNKNOWN")

    top_frame = tk.Frame(fillhead_tab, bg="#21232b")
    top_frame.pack(side=tk.TOP, fill=tk.X, expand=False, padx=5, pady=5)

    middle_frame = tk.Frame(fillhead_tab, bg="#21232b")
    middle_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    fh_controls_frame = tk.Frame(top_frame, bg="#21232b")
    fh_controls_frame.pack(side=tk.LEFT, fill=tk.Y, expand=True, padx=5, pady=5)

    state_frame = tk.LabelFrame(fh_controls_frame, text="Axis States", bg="#2a2d3b", fg="white", padx=5, pady=5)
    state_frame.pack(fill=tk.X, pady=(0, 5))
    state_frame.grid_columnconfigure(1, weight=1)

    tk.Label(state_frame, text="X-Axis:", bg="#2a2d3b", fg="white", font=("Segoe UI", 10, "bold")).grid(row=0, column=0,
                                                                                                        sticky='e',
                                                                                                        padx=5)
    tk.Label(state_frame, textvariable=ui_elements['fh_state_x_var'], bg="#2a2d3b", fg="cyan",
             font=("Segoe UI", 10, "bold")).grid(row=0, column=1, sticky='w')
    tk.Label(state_frame, text="Y-Axis:", bg="#2a2d3b", fg="white", font=("Segoe UI", 10, "bold")).grid(row=1, column=0,
                                                                                                        sticky='e',
                                                                                                        padx=5)
    tk.Label(state_frame, textvariable=ui_elements['fh_state_y_var'], bg="#2a2d3b", fg="yellow",
             font=("Segoe UI", 10, "bold")).grid(row=1, column=1, sticky='w')
    tk.Label(state_frame, text="Z-Axis:", bg="#2a2d3b", fg="white", font=("Segoe UI", 10, "bold")).grid(row=2, column=0,
                                                                                                        sticky='e',
                                                                                                        padx=5)
    tk.Label(state_frame, textvariable=ui_elements['fh_state_z_var'], bg="#2a2d3b", fg="#ff8888",
             font=("Segoe UI", 10, "bold")).grid(row=2, column=1, sticky='w')

    enable_frame = tk.LabelFrame(fh_controls_frame, text="Axis Power", bg="#2a2d3b", fg="white", padx=5, pady=5)
    enable_frame.pack(fill=tk.X, pady=5, anchor='n')

    def update_button_styles(enabled_var, enable_btn, disable_btn):
        if enabled_var.get() == "Enabled":
            enable_btn.config(style="Green.TButton")
            disable_btn.config(style="Neutral.TButton")
        else:
            enable_btn.config(style="Neutral.TButton")
            disable_btn.config(style="Red.TButton")

    def create_enable_buttons(parent, axis_letter, enabled_var):
        row_container = tk.Frame(parent, bg=parent['bg'])
        tk.Label(row_container, text=f"{axis_letter} Axis:", width=7, anchor='e', bg=parent['bg'], fg='white').pack(
            side=tk.LEFT, padx=(0, 10))
        button_pair_frame = tk.Frame(row_container)
        button_pair_frame.pack(side=tk.LEFT, expand=True, fill=tk.X)
        enable_btn = ttk.Button(button_pair_frame, text="Enable", width=7,
                                style="Neutral.TButton",
                                command=lambda: send_fillhead_cmd(f"ENABLE_{axis_letter}"))
        enable_btn.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=0, pady=0)
        disable_btn = ttk.Button(button_pair_frame, text="Disable", width=7,
                                 style="Red.TButton",
                                 command=lambda: send_fillhead_cmd(f"DISABLE_{axis_letter}"))
        disable_btn.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=0, pady=0)
        enabled_var.trace_add('write', lambda *args: update_button_styles(enabled_var, enable_btn, disable_btn))
        update_button_styles(enabled_var, enable_btn, disable_btn)
        return row_container

    create_enable_buttons(enable_frame, "X", ui_elements['fh_enabled_m0_var']).pack(fill=tk.X, pady=(0, 3))
    create_enable_buttons(enable_frame, "Y", ui_elements['fh_enabled_m1_var']).pack(fill=tk.X, pady=(0, 3))
    create_enable_buttons(enable_frame, "Z", ui_elements['fh_enabled_m3_var']).pack(fill=tk.X)

    fh_jog_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead Jog", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_jog_frame.pack(fill=tk.X, pady=5, anchor='n')

    fh_jog_dist_mm_var = tk.StringVar(value="10.0")
    fh_jog_vel_mms_var = tk.StringVar(value="15.0")
    fh_jog_accel_mms2_var = tk.StringVar(value="50.0")
    fh_jog_torque_var = tk.StringVar(value="20")

    jog_params_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b")
    jog_params_frame.grid(row=0, column=0, columnspan=4, sticky='ew')
    tk.Label(jog_params_frame, text="Distance (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=0, padx=5, pady=2,
                                                                                     sticky='e')
    ttk.Entry(jog_params_frame, textvariable=fh_jog_dist_mm_var, width=8).grid(row=0, column=1)
    tk.Label(jog_params_frame, text="Speed (mm/s):", bg="#2a2d3b", fg="white").grid(row=1, column=0, padx=5, pady=2,
                                                                                    sticky='e')
    ttk.Entry(jog_params_frame, textvariable=fh_jog_vel_mms_var, width=8).grid(row=1, column=1)
    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg="#2a2d3b", fg="white").grid(row=0, column=2, padx=5, pady=2,
                                                                                     sticky='e')
    ttk.Entry(jog_params_frame, textvariable=fh_jog_accel_mms2_var, width=8).grid(row=0, column=3)
    tk.Label(jog_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=1, column=2, padx=5, pady=2,
                                                                                  sticky='e')
    ttk.Entry(jog_params_frame, textvariable=fh_jog_torque_var, width=8).grid(row=1, column=3)

    jog_cmd_str = lambda \
            dist: f"{dist} {fh_jog_vel_mms_var.get()} {fh_jog_accel_mms2_var.get()} {fh_jog_torque_var.get()}"
    jog_btn_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b")
    jog_btn_frame.grid(row=2, column=0, columnspan=4, pady=5)
    ttk.Button(jog_btn_frame, text="X-",
               command=lambda: send_fillhead_cmd(f"MOVE_X -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="X+",
               command=lambda: send_fillhead_cmd(f"MOVE_X {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Y-",
               command=lambda: send_fillhead_cmd(f"MOVE_Y -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                           padx=10)
    ttk.Button(jog_btn_frame, text="Y+",
               command=lambda: send_fillhead_cmd(f"MOVE_Y {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Z-",
               command=lambda: send_fillhead_cmd(f"MOVE_Z -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                           padx=10)
    ttk.Button(jog_btn_frame, text="Z+",
               command=lambda: send_fillhead_cmd(f"MOVE_Z {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT)

    fh_home_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead Homing", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_home_frame.pack(fill=tk.X, pady=5, anchor='n')
    fh_home_torque_var = tk.StringVar(value="20")
    fh_home_distance_mm_var = tk.StringVar(value="420.0")
    home_params_frame = tk.Frame(fh_home_frame, bg="#2a2d3b")
    home_params_frame.pack()
    tk.Label(home_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="e", pady=1,
                                                                                   padx=5)
    ttk.Entry(home_params_frame, textvariable=fh_home_torque_var, width=10).grid(row=0, column=1, sticky="w", pady=1)
    tk.Label(home_params_frame, text="Max Distance (mm):", bg="#2a2d3b", fg="white").grid(row=1, column=0, sticky="e",
                                                                                          pady=1, padx=5)
    ttk.Entry(home_params_frame, textvariable=fh_home_distance_mm_var, width=10).grid(row=1, column=1, sticky="w",
                                                                                      pady=1)
    home_btn_frame = tk.Frame(fh_home_frame, bg="#2a2d3b")
    home_btn_frame.pack(pady=(5, 0))
    home_cmd_str = lambda axis: f"HOME_{axis} {fh_home_torque_var.get()} {fh_home_distance_mm_var.get()}"
    ttk.Button(home_btn_frame, text="Home X", command=lambda: send_fillhead_cmd(home_cmd_str("X"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X)
    ttk.Button(home_btn_frame, text="Home Y", command=lambda: send_fillhead_cmd(home_cmd_str("Y"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X, padx=5)
    ttk.Button(home_btn_frame, text="Home Z", command=lambda: send_fillhead_cmd(home_cmd_str("Z"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X)

    fh_demo_frame = tk.LabelFrame(fh_controls_frame, text="Demo Routines", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_demo_frame.pack(fill=tk.X, pady=5, anchor='n')

    demo_btn_frame = tk.Frame(fh_demo_frame, bg="#2a2d3b")
    demo_btn_frame.pack(pady=5)

    ttk.Button(demo_btn_frame, text="Start Circle Demo", style="Green.TButton",
               command=lambda: send_fillhead_cmd("START_DEMO")).pack(
        side=tk.LEFT, expand=True, fill=tk.X, padx=5)
    ttk.Button(demo_btn_frame, text="Stop Demo", style="Red.TButton", command=lambda: send_fillhead_cmd("ABORT")).pack(
        side=tk.LEFT, expand=True, fill=tk.X, padx=5)

    vis_frame = tk.LabelFrame(top_frame, text="Position Visualization", bg="#2a2d3b", fg="white", padx=5, pady=5)
    vis_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)
    readout_frame = tk.Frame(vis_frame, bg="#2a2d3b")
    readout_frame.pack(side=tk.TOP, fill=tk.X, pady=(0, 5))
    font_readout_label = ("Segoe UI", 10, "bold")
    font_readout_value = ("Consolas", 24, "bold")
    x_frame = tk.Frame(readout_frame, bg=readout_frame['bg'])
    x_frame.pack(side=tk.LEFT, expand=True, fill=tk.X)
    tk.Label(x_frame, text="X (mm)", bg=readout_frame['bg'], fg="cyan", font=font_readout_label).pack()
    tk.Label(x_frame, textvariable=ui_elements['fh_pos_m0_var'], bg=readout_frame['bg'], fg="cyan",
             font=font_readout_value).pack()
    y_frame = tk.Frame(readout_frame, bg=readout_frame['bg'])
    y_frame.pack(side=tk.LEFT, expand=True, fill=tk.X)
    tk.Label(y_frame, text="Y (mm)", bg=readout_frame['bg'], fg="yellow", font=font_readout_label).pack()
    tk.Label(y_frame, textvariable=ui_elements['fh_pos_m1_var'], bg=readout_frame['bg'], fg="yellow",
             font=font_readout_value).pack()
    z_frame = tk.Frame(readout_frame, bg=readout_frame['bg'])
    z_frame.pack(side=tk.LEFT, expand=True, fill=tk.X)
    tk.Label(z_frame, text="Z (mm)", bg=readout_frame['bg'], fg="#ff8888", font=font_readout_label).pack()
    tk.Label(z_frame, textvariable=ui_elements['fh_pos_m3_var'], bg=readout_frame['bg'], fg="#ff8888",
             font=font_readout_value).pack()

    # Position plot is now gone, this space can be reused or left empty

    fh_status_frame = tk.Frame(middle_frame, bg="#21232b")
    fh_status_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, padx=5, pady=5, anchor='n')

    # Torque indicators will go in this frame
    torque_bar_area = tk.LabelFrame(middle_frame, text="Fillhead Torque", bg="#21232b", fg="white")
    torque_bar_area.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, pady=5)

    motor_map = ["Motor 0 (X)", "Motor 1 (Y1)", "Motor 2 (Y2)", "Motor 3 (Z)"]
    for i, name in enumerate(motor_map):
        frame = tk.LabelFrame(fh_status_frame, text=name, bg="#2a2d3b", fg="white", padx=5, pady=2)
        frame.pack(fill=tk.X, pady=2, anchor='n')
        frame.grid_columnconfigure(1, weight=1)
        tk.Label(frame, text="Position (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_pos_m{i}_var'], bg="#2a2d3b", fg="cyan").grid(row=0, column=1,
                                                                                                    sticky="w")
        tk.Label(frame, text="Enabled:", bg="#2a2d3b", fg="white").grid(row=2, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_enabled_m{i}_var'], bg="#2a2d3b", fg="lightgreen").grid(row=2,
                                                                                                              column=1,
                                                                                                              sticky="w")
        tk.Label(frame, text="Homed:", bg="#2a2d3b", fg="white").grid(row=3, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_homed_m{i}_var'], bg="#2a2d3b", fg="lightgreen").grid(row=3,
                                                                                                            column=1,
                                                                                                            sticky="w")

    # --- Create Torque Bars ---
    bar_labels = ["Motor 0 (X)", "Motor 1 (Y1)", "Motor 2 (Y2)", "Motor 3 (Z)"]
    bar_colors = ['#00bfff', 'yellow', '#ffed72', '#ff8888']

    for i in range(4):
        bar_frame, canvas, bar_item, text_item = create_torque_bar(torque_bar_area, bar_labels[i], bar_colors[i])
        bar_frame.pack(side=tk.LEFT, expand=True, padx=5, pady=5)
        ui_elements[f'fh_torque_canvas{i}'] = canvas
        ui_elements[f'fh_torque_bar{i}'] = bar_item
        ui_elements[f'fh_torque_text{i}'] = text_item

    return ui_elements
