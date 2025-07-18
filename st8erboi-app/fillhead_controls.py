import tkinter as tk
from tkinter import ttk


def create_fillhead_ancillary_controls(parent, send_fillhead_cmd):
    """Creates the Jog and Homing control frames for the Fillhead."""

    # --- Jog Controls ---
    fh_jog_frame = tk.LabelFrame(parent, text="Fillhead Jog", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_jog_frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')

    fh_jog_dist_mm_var = tk.StringVar(value="10.0");
    fh_jog_vel_mms_var = tk.StringVar(value="15.0");
    fh_jog_accel_mms2_var = tk.StringVar(value="50.0");
    fh_jog_torque_var = tk.StringVar(value="20")

    jog_params_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b");
    jog_params_frame.pack(fill=tk.X)
    jog_params_frame.grid_columnconfigure((1, 3), weight=1)

    tk.Label(jog_params_frame, text="Dist (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=0, padx=(0, 5), pady=1,
                                                                                 sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_dist_mm_var, width=8).grid(row=0, column=1, sticky='ew',
                                                                               padx=(0, 2))
    tk.Label(jog_params_frame, text="Speed (mm/s):", bg="#2a2d3b", fg="white").grid(row=0, column=2, padx=(2, 5),
                                                                                    pady=1, sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_vel_mms_var, width=8).grid(row=0, column=3, sticky='ew')
    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg="#2a2d3b", fg="white").grid(row=1, column=0, padx=(0, 5),
                                                                                     pady=1, sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_accel_mms2_var, width=8).grid(row=1, column=1, sticky='ew',
                                                                                  padx=(0, 2))
    tk.Label(jog_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=1, column=2, padx=(2, 5), pady=1,
                                                                                  sticky='e');
    ttk.Entry(jog_params_frame, textvariable=fh_jog_torque_var, width=8).grid(row=1, column=3, sticky='ew')

    jog_cmd_str = lambda \
        dist: f"{dist} {fh_jog_vel_mms_var.get()} {fh_jog_accel_mms2_var.get()} {fh_jog_torque_var.get()}"

    jog_btn_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b");
    jog_btn_frame.pack(fill=tk.X, pady=5)

    ttk.Button(jog_btn_frame, text="X-",
               command=lambda: send_fillhead_cmd(f"MOVE_X -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                           expand=True,
                                                                                                           fill=tk.X)
    ttk.Button(jog_btn_frame, text="X+",
               command=lambda: send_fillhead_cmd(f"MOVE_X {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                          expand=True,
                                                                                                          fill=tk.X,
                                                                                                          padx=2)
    ttk.Button(jog_btn_frame, text="Y-",
               command=lambda: send_fillhead_cmd(f"MOVE_Y -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                           expand=True,
                                                                                                           fill=tk.X,
                                                                                                           padx=(8, 2))
    ttk.Button(jog_btn_frame, text="Y+",
               command=lambda: send_fillhead_cmd(f"MOVE_Y {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                          expand=True,
                                                                                                          fill=tk.X)
    ttk.Button(jog_btn_frame, text="Z-",
               command=lambda: send_fillhead_cmd(f"MOVE_Z -{jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                           expand=True,
                                                                                                           fill=tk.X,
                                                                                                           padx=(8, 2))
    ttk.Button(jog_btn_frame, text="Z+",
               command=lambda: send_fillhead_cmd(f"MOVE_Z {jog_cmd_str(fh_jog_dist_mm_var.get())}")).pack(side=tk.LEFT,
                                                                                                          expand=True,
                                                                                                          fill=tk.X)

    # --- Homing Controls ---
    fh_home_frame = tk.LabelFrame(parent, text="Fillhead Homing", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_home_frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')
    fh_home_torque_var = tk.StringVar(value="20");
    fh_home_distance_mm_var = tk.StringVar(value="420.0")
    home_params_frame = tk.Frame(fh_home_frame, bg="#2a2d3b");
    home_params_frame.pack(fill=tk.X)
    home_params_frame.grid_columnconfigure(1, weight=1)
    home_params_frame.grid_columnconfigure(3, weight=1)

    tk.Label(home_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="e", pady=1,
                                                                                   padx=(0, 5));
    ttk.Entry(home_params_frame, textvariable=fh_home_torque_var, width=10).grid(row=0, column=1, sticky="ew",
                                                                                 padx=(0, 2))
    tk.Label(home_params_frame, text="Max Dist (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=2, sticky="e",
                                                                                      pady=1, padx=(2, 5));
    ttk.Entry(home_params_frame, textvariable=fh_home_distance_mm_var, width=10).grid(row=0, column=3, sticky="ew")

    home_btn_frame = tk.Frame(fh_home_frame, bg="#2a2d3b");
    home_btn_frame.pack(pady=(5, 0), fill=tk.X)
    home_cmd_str = lambda axis: f"HOME_{axis} {fh_home_torque_var.get()} {fh_home_distance_mm_var.get()}"
    ttk.Button(home_btn_frame, text="Home X", command=lambda: send_fillhead_cmd(home_cmd_str("X"))).pack(side=tk.LEFT,
                                                                                                         expand=True,
                                                                                                         fill=tk.X)
    ttk.Button(home_btn_frame, text="Home Y", command=lambda: send_fillhead_cmd(home_cmd_str("Y"))).pack(side=tk.LEFT,
                                                                                                         expand=True,
                                                                                                         fill=tk.X,
                                                                                                         padx=5)
    ttk.Button(home_btn_frame, text="Home Z", command=lambda: send_fillhead_cmd(home_cmd_str("Z"))).pack(side=tk.LEFT,
                                                                                                         expand=True,
                                                                                                         fill=tk.X)

    return {}
