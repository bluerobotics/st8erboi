import tkinter as tk
from tkinter import ttk


def create_fillhead_motor_boxes(parent, send_fillhead_cmd, shared_gui_refs):
    """
    Creates and packs the individual motor status and control frames for the Fillhead
    into a given parent widget.
    """
    ui_elements = {}

    # Initialize StringVars if they don't exist
    for i in range(4):
        if f'fh_pos_m{i}_var' not in shared_gui_refs: shared_gui_refs[f'fh_pos_m{i}_var'] = tk.StringVar(value="0.00")
        if f'fh_enabled_m{i}_var' not in shared_gui_refs: shared_gui_refs[f'fh_enabled_m{i}_var'] = tk.StringVar(
            value="Disabled")
        if f'fh_homed_m{i}_var' not in shared_gui_refs: shared_gui_refs[f'fh_homed_m{i}_var'] = tk.StringVar(
            value="Not Homed")

    def update_button_styles(enabled_var, enable_btn, disable_btn):
        if enabled_var.get() == "Enabled":
            enable_btn.config(style="Green.TButton")
            disable_btn.config(style="Neutral.TButton")
        else:
            enable_btn.config(style="Neutral.TButton")
            disable_btn.config(style="Red.TButton")

    def create_axis_control_frame(p, axis_name, axis_letter, motor_index, color):
        frame = tk.LabelFrame(p, text=axis_name, bg="#1b2432", fg=color, padx=5, pady=2, font=("Segoe UI", 9, "bold"))
        frame.pack(fill=tk.X, pady=2, anchor='n')
        frame.grid_columnconfigure(1, weight=1)

        enabled_var = shared_gui_refs[f'fh_enabled_m{motor_index}_var']

        tk.Label(frame, text="Position (mm):", bg=frame['bg'], fg="white").grid(row=0, column=0, sticky="w")
        tk.Label(frame, textvariable=shared_gui_refs[f'fh_pos_m{motor_index}_var'], bg=frame['bg'], fg=color).grid(
            row=0, column=1, sticky="w")

        tk.Label(frame, text="Homed:", bg=frame['bg'], fg="white").grid(row=1, column=0, sticky="w")
        tk.Label(frame, textvariable=shared_gui_refs[f'fh_homed_m{motor_index}_var'], bg=frame['bg'],
                 fg="lightgreen").grid(row=1, column=1, sticky="w")

        # Enable/Disable buttons are now inside this frame
        button_pair_frame = tk.Frame(frame, bg=frame['bg'])
        button_pair_frame.grid(row=2, column=0, columnspan=2, sticky='ew', pady=(3, 0))

        enable_btn = ttk.Button(button_pair_frame, text="Enable", width=7, style="Neutral.TButton",
                                command=lambda: send_fillhead_cmd(f"ENABLE_{axis_letter}"))
        enable_btn.pack(side=tk.LEFT, expand=True, fill=tk.X)

        disable_btn = ttk.Button(button_pair_frame, text="Disable", width=7, style="Red.TButton",
                                 command=lambda: send_fillhead_cmd(f"DISABLE_{axis_letter}"))
        disable_btn.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(2, 0))

        enabled_var.trace_add('write', lambda *args: update_button_styles(enabled_var, enable_btn, disable_btn))
        update_button_styles(enabled_var, enable_btn, disable_btn)

    motor_map = [("Motor 0 (X)", "X", 0, "cyan"), ("Motor 1 (Y1)", "Y", 1, "yellow"),
                 ("Motor 2 (Y2)", "Y", 2, "#ffed72"), ("Motor 3 (Z)", "Z", 3, "#ff8888")]
    for name, letter, index, color in motor_map:
        create_axis_control_frame(parent, name, letter, index, color)

    return ui_elements


def create_fillhead_ancillary_controls(parent, send_fillhead_cmd):
    """Creates the Jog and Homing control frames for the Fillhead."""

    # --- Jog Controls ---
    fh_jog_frame = tk.LabelFrame(parent, text="Fillhead Jog", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_jog_frame.pack(fill=tk.X, pady=5, anchor='n')

    fh_jog_dist_mm_var = tk.StringVar(value="10.0");
    fh_jog_vel_mms_var = tk.StringVar(value="15.0");
    fh_jog_accel_mms2_var = tk.StringVar(value="50.0");
    fh_jog_torque_var = tk.StringVar(value="20")
    jog_params_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b");
    jog_params_frame.grid(row=0, column=0, columnspan=4, sticky='ew')
    tk.Label(jog_params_frame, text="Distance (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=0, padx=5, pady=2,
                                                                                     sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_dist_mm_var, width=8).grid(row=0, column=1)
    tk.Label(jog_params_frame, text="Speed (mm/s):", bg="#2a2d3b", fg="white").grid(row=1, column=0, padx=5, pady=2,
                                                                                    sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_vel_mms_var, width=8).grid(row=1, column=1)
    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg="#2a2d3b", fg="white").grid(row=0, column=2, padx=5, pady=2,
                                                                                     sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_accel_mms2_var, width=8).grid(row=0, column=3)
    tk.Label(jog_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=1, column=2, padx=5, pady=2,
                                                                                  sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_torque_var, width=8).grid(row=1, column=3)
    jog_cmd_str = lambda \
        dist: f"{dist} {fh_jog_vel_mms_var.get()} {fh_jog_accel_mms2_var.get()} {fh_jog_torque_var.get()}"
    jog_btn_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b");
    jog_btn_frame.grid(row=2, column=0, columnspan=4, pady=5)
    ttk.Button(jog_btn_frame, text="X-",
               command=lambda: send_fillhead_cmd(f"MOVE_X -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(
        side=tk.LEFT);
    ttk.Button(jog_btn_frame, text="X+",
               command=lambda: send_fillhead_cmd(f"MOVE_X {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Y-",
               command=lambda: send_fillhead_cmd(f"MOVE_Y -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                           padx=10);
    ttk.Button(jog_btn_frame, text="Y+",
               command=lambda: send_fillhead_cmd(f"MOVE_Y {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT)
    ttk.Button(jog_btn_frame, text="Z-",
               command=lambda: send_fillhead_cmd(f"MOVE_Z -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                           padx=10);
    ttk.Button(jog_btn_frame, text="Z+",
               command=lambda: send_fillhead_cmd(f"MOVE_Z {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT)

    # --- Homing Controls ---
    fh_home_frame = tk.LabelFrame(parent, text="Fillhead Homing", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_home_frame.pack(fill=tk.X, pady=5, anchor='n')
    fh_home_torque_var = tk.StringVar(value="20");
    fh_home_distance_mm_var = tk.StringVar(value="420.0")
    home_params_frame = tk.Frame(fh_home_frame, bg="#2a2d3b");
    home_params_frame.pack()
    tk.Label(home_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="e", pady=1,
                                                                                   padx=5);
    ttk.Entry(home_params_frame, textvariable=fh_home_torque_var, width=10).grid(row=0, column=1, sticky="w", pady=1)
    tk.Label(home_params_frame, text="Max Distance (mm):", bg="#2a2d3b", fg="white").grid(row=1, column=0, sticky="e",
                                                                                          pady=1, padx=5);
    ttk.Entry(home_params_frame, textvariable=fh_home_distance_mm_var, width=10).grid(row=1, column=1, sticky="w",
                                                                                      pady=1)
    home_btn_frame = tk.Frame(fh_home_frame, bg="#2a2d3b");
    home_btn_frame.pack(pady=(5, 0))
    home_cmd_str = lambda axis: f"HOME_{axis} {fh_home_torque_var.get()} {fh_home_distance_mm_var.get()}"
    ttk.Button(home_btn_frame, text="Home X", command=lambda: send_fillhead_cmd(home_cmd_str("X"))).pack(side=tk.LEFT,
                                                                                                         expand=True,
                                                                                                         fill=tk.X);
    ttk.Button(home_btn_frame, text="Home Y", command=lambda: send_fillhead_cmd(home_cmd_str("Y"))).pack(side=tk.LEFT,
                                                                                                         expand=True,
                                                                                                         fill=tk.X,
                                                                                                         padx=5);
    ttk.Button(home_btn_frame, text="Home Z", command=lambda: send_fillhead_cmd(home_cmd_str("Z"))).pack(side=tk.LEFT,
                                                                                                         expand=True,
                                                                                                         fill=tk.X)

    return {}
