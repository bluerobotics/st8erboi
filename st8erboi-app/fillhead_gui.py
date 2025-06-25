# fillhead_gui.py
import tkinter as tk
from tkinter import ttk


def create_fillhead_tab(notebook, send_fillhead_cmd, shared_gui_refs):
    fillhead_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(fillhead_tab, text='  Fillhead  ')

    ui_elements = {}

    # --- Parent Frames ---
    fh_controls_frame = tk.Frame(fillhead_tab, bg="#21232b")
    fh_controls_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
    fh_status_frame = tk.Frame(fillhead_tab, bg="#21232b")
    fh_status_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=5, pady=5)

    # --- Fillhead Controls ---
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

    fh_home_frame = tk.LabelFrame(fh_controls_frame, text="Fillhead Homing", bg="#2a2d3b", fg="white", padx=5, pady=5)
    fh_home_frame.pack(fill=tk.X, pady=5, anchor='n')
    ttk.Button(fh_home_frame, text="Home X", command=lambda: send_fillhead_cmd("HOME_X")).pack(side=tk.LEFT,
                                                                                               expand=True, fill=tk.X)
    ttk.Button(fh_home_frame, text="Home Y", command=lambda: send_fillhead_cmd("HOME_Y")).pack(side=tk.LEFT,
                                                                                               expand=True, fill=tk.X)
    ttk.Button(fh_home_frame, text="Home Z", command=lambda: send_fillhead_cmd("HOME_Z")).pack(side=tk.LEFT,
                                                                                               expand=True, fill=tk.X)

    # --- Fillhead Status Display ---
    for i, axis in enumerate(['x', 'y', 'z']):
        frame = tk.LabelFrame(fh_status_frame, text=f"Axis {axis.upper()}", bg="#2a2d3b", fg="white", padx=5, pady=2)
        frame.pack(fill=tk.X, pady=2, anchor='n')

        pos_var = tk.StringVar(value="0");
        ui_elements[f'fh_pos_{axis}_var'] = pos_var
        torque_var = tk.StringVar(value="0.0 %");
        ui_elements[f'fh_torque_{axis}_var'] = torque_var
        enabled_var = tk.StringVar(value="Disabled");
        ui_elements[f'fh_enabled_{axis}_var'] = enabled_var
        homed_var = tk.StringVar(value="Not Homed");
        ui_elements[f'fh_homed_{axis}_var'] = homed_var

        frame.grid_columnconfigure(1, weight=1)
        tk.Label(frame, text="Position:", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="w")
        tk.Label(frame, textvariable=pos_var, bg="#2a2d3b", fg="cyan").grid(row=0, column=1, sticky="w")
        tk.Label(frame, text="Torque:", bg="#2a2d3b", fg="white").grid(row=1, column=0, sticky="w")
        tk.Label(frame, textvariable=torque_var, bg="#2a2d3b", fg="cyan").grid(row=1, column=1, sticky="w")
        tk.Label(frame, text="Enabled:", bg="#2a2d3b", fg="white").grid(row=2, column=0, sticky="w")
        tk.Label(frame, textvariable=enabled_var, bg="#2a2d3b", fg="lightgreen").grid(row=2, column=1, sticky="w")
        tk.Label(frame, text="Homed:", bg="#2a2d3b", fg="white").grid(row=3, column=0, sticky="w")
        tk.Label(frame, textvariable=homed_var, bg="#2a2d3b", fg="lightgreen").grid(row=3, column=1, sticky="w")

    return ui_elements