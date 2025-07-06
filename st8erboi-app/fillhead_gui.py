import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import numpy as np


def create_fillhead_tab(notebook, send_fillhead_cmd, shared_gui_refs):
    fillhead_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(fillhead_tab, text='  Fillhead  ')

    ui_elements = {}

    for i in range(4):
        ui_elements[f'fh_pos_m{i}_var'] = tk.StringVar(value="0.00")
        ui_elements[f'fh_torque_m{i}_var'] = tk.StringVar(value="0.0 %")
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

    # --- UPDATED: Enable/Disable Buttons Section ---
    enable_frame = tk.LabelFrame(fh_controls_frame, text="Axis Power", bg="#2a2d3b", fg="white", padx=5, pady=5)
    enable_frame.pack(fill=tk.X, pady=5, anchor='n')

    def update_button_styles(enabled_var, enable_btn, disable_btn):
        """Changes button styles based on the 'Enabled'/'Disabled' string."""
        if enabled_var.get() == "Enabled":
            enable_btn.config(style="Green.TButton")
            disable_btn.config(style="Neutral.TButton")
        else:
            enable_btn.config(style="Neutral.TButton")
            disable_btn.config(style="Red.TButton")

    def create_enable_buttons(parent, axis_letter, enabled_var):
        """Creates a styled 'slider' button group for enabling/disabling an axis."""
        # This container holds one row: a label and the button pair
        row_container = tk.Frame(parent, bg=parent['bg'])

        tk.Label(row_container, text=f"{axis_letter} Axis:", width=7, anchor='e', bg=parent['bg'], fg='white').pack(
            side=tk.LEFT, padx=(0, 10))

        # This frame makes the two buttons stick together seamlessly
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

        # Use a lambda in trace to pass arguments to the update function
        enabled_var.trace_add('write', lambda *args: update_button_styles(enabled_var, enable_btn, disable_btn))

        # Call once to set the initial style correctly
        update_button_styles(enabled_var, enable_btn, disable_btn)

        return row_container

    # Create the button groups for each axis
    create_enable_buttons(enable_frame, "X", ui_elements['fh_enabled_m0_var']).pack(fill=tk.X, pady=(0, 3))
    create_enable_buttons(enable_frame, "Y", ui_elements['fh_enabled_m1_var']).pack(fill=tk.X, pady=(0, 3))
    create_enable_buttons(enable_frame, "Z", ui_elements['fh_enabled_m3_var']).pack(fill=tk.X)
    # --- END UPDATED SECTION ---

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
    fh_home_distance_mm_var = tk.StringVar(value="120.0")
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
    home_cmd_str = lambda axis: f"{axis} {fh_home_torque_var.get()} {fh_home_distance_mm_var.get()}"
    ttk.Button(home_btn_frame, text="Home X", command=lambda: send_fillhead_cmd(home_cmd_str("HOME_X"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X)
    ttk.Button(home_btn_frame, text="Home Y", command=lambda: send_fillhead_cmd(home_cmd_str("HOME_Y"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X, padx=5)
    ttk.Button(home_btn_frame, text="Home Z", command=lambda: send_fillhead_cmd(home_cmd_str("HOME_Z"))).pack(
        side=tk.LEFT, expand=True, fill=tk.X)

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

    fig = plt.figure(figsize=(5, 3), facecolor='#2a2d3b')
    ax_xy = fig.add_subplot(1, 2, 1)
    ax_z = fig.add_subplot(1, 2, 2)
    fig.tight_layout(pad=3.0)
    xy_pos_marker, = ax_xy.plot([0], [0], 'co', markersize=10)
    ax_xy.set_title("X-Y Position", color='white')
    ax_xy.set_facecolor('#1b1e2b')
    ax_xy.set_xlabel("X (mm)", color='white')
    ax_xy.set_ylabel("Y (mm)", color='white')
    ax_xy.set_xlim(-100, 100);
    ax_xy.set_ylim(-100, 100)
    ax_xy.tick_params(colors='white');
    ax_xy.spines['bottom'].set_color('white')
    ax_xy.spines['left'].set_color('white');
    ax_xy.spines['top'].set_visible(False)
    ax_xy.spines['right'].set_visible(False);
    ax_xy.grid(True, linestyle='--', alpha=0.3)
    z_bar = ax_z.bar(['Z'], [0], color='#ff8888')[0]
    ax_z.set_title("Z Position", color='white')
    ax_z.set_facecolor('#1b1e2b')
    ax_z.set_ylabel("Z (mm)", color='white')
    ax_z.set_ylim(-100, 5)
    ax_z.tick_params(axis='x', colors='#2a2d3b');
    ax_z.tick_params(axis='y', colors='white')
    ax_z.spines['bottom'].set_color('white');
    ax_z.spines['left'].set_color('white')
    ax_z.spines['top'].set_visible(False);
    ax_z.spines['right'].set_visible(False)
    canvas = FigureCanvasTkAgg(fig, master=vis_frame)
    canvas_widget = canvas.get_tk_widget()
    canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
    ui_elements.update({'fh_pos_viz_canvas': canvas, 'fh_xy_marker': xy_pos_marker, 'fh_z_bar': z_bar})

    fh_status_frame = tk.Frame(middle_frame, bg="#21232b")
    fh_status_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, padx=5, pady=5, anchor='n')
    plot_frame = tk.Frame(middle_frame, bg="#21232b")
    plot_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, pady=5)

    motor_map = ["Motor 0 (X)", "Motor 1 (Y1)", "Motor 2 (Y2)", "Motor 3 (Z)"]
    for i, name in enumerate(motor_map):
        frame = tk.LabelFrame(fh_status_frame, text=name, bg="#2a2d3b", fg="white", padx=5, pady=2)
        frame.pack(fill=tk.X, pady=2, anchor='n')
        frame.grid_columnconfigure(1, weight=1)
        tk.Label(frame, text="Position (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_pos_m{i}_var'], bg="#2a2d3b", fg="cyan").grid(row=0, column=1,
                                                                                                    sticky="w")
        tk.Label(frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=1, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_torque_m{i}_var'], bg="#2a2d3b", fg="cyan").grid(row=1, column=1,
                                                                                                       sticky="w")
        tk.Label(frame, text="Enabled:", bg="#2a2d3b", fg="white").grid(row=2, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_enabled_m{i}_var'], bg="#2a2d3b", fg="lightgreen").grid(row=2,
                                                                                                              column=1,
                                                                                                              sticky="w")
        tk.Label(frame, text="Homed:", bg="#2a2d3b", fg="white").grid(row=3, column=0, sticky="w")
        tk.Label(frame, textvariable=ui_elements[f'fh_homed_m{i}_var'], bg="#2a2d3b", fg="lightgreen").grid(row=3,
                                                                                                            column=1,
                                                                                                            sticky="w")

    fig_torque, ax_torque = plt.subplots(figsize=(7, 2.5), facecolor='#21232b')
    colors = ['#00bfff', 'yellow', '#ffed72', '#ff8888']
    lines = [ax_torque.plot([], [], color=c, label=f"M{i}")[0] for i, c in enumerate(colors)]
    ax_torque.set_title("Fillhead Torque", color='white');
    ax_torque.set_facecolor('#1b1e2b')
    ax_torque.tick_params(colors='white');
    ax_torque.spines['bottom'].set_color('white')
    ax_torque.spines['left'].set_color('white');
    ax_torque.spines['top'].set_visible(False)
    ax_torque.spines['right'].set_visible(False);
    ax_torque.set_ylabel("Torque (%)", color='white')
    ax_torque.set_ylim(-10, 110);
    ax_torque.legend(facecolor='#21232b', edgecolor='white', labelcolor='white')
    canvas_torque = FigureCanvasTkAgg(fig_torque, master=plot_frame)
    canvas_widget_torque = canvas_torque.get_tk_widget();
    canvas_widget_torque.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
    ui_elements['fillhead_torque_plot_canvas'] = canvas_torque
    ui_elements['fillhead_torque_plot_ax'] = ax_torque
    ui_elements['fillhead_torque_plot_lines'] = lines

    return ui_elements