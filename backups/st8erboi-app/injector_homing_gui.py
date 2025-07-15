import tkinter as tk
from tkinter import ttk


def create_homing_controls(parent, send_cmd_func, ui_elements, variables):
    """
    Creates and populates the Homing controls frame.
    """
    homing_controls_frame = tk.LabelFrame(parent, text="Homing Controls (Active in HOMING_MODE)",
                                          bg="#2b1e34", fg="#D8BFD8", font=("Segoe UI", 10, "bold"),
                                          bd=2, relief="ridge")
    ui_elements['homing_controls_frame'] = homing_controls_frame

    font_small = ('Segoe UI', 9)
    field_width_homing = 10

    homing_controls_frame.grid_columnconfigure(1, weight=1)
    homing_controls_frame.grid_columnconfigure(3, weight=1)

    h_row = 0
    tk.Label(homing_controls_frame, text="Stroke (mm):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(homing_controls_frame, textvariable=variables['homing_stroke_len_var'], width=field_width_homing,
              font=font_small).grid(row=h_row, column=1, sticky="ew", padx=2, pady=2)

    tk.Label(homing_controls_frame, text="Rapid Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(homing_controls_frame, textvariable=variables['homing_rapid_vel_var'], width=field_width_homing,
              font=font_small).grid(row=h_row, column=3, sticky="ew", padx=2, pady=2)

    h_row += 1
    tk.Label(homing_controls_frame, text="Touch Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(homing_controls_frame, textvariable=variables['homing_touch_vel_var'], width=field_width_homing,
              font=font_small).grid(row=h_row, column=1, sticky="ew", padx=2, pady=2)

    tk.Label(homing_controls_frame, text="Accel (mm/s²):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(homing_controls_frame, textvariable=variables['homing_acceleration_var'], width=field_width_homing,
              font=font_small).grid(row=h_row, column=3, sticky="ew", padx=2, pady=2)

    h_row += 1
    tk.Label(homing_controls_frame, text="M.Retract (mm):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(homing_controls_frame, textvariable=variables['homing_retract_dist_var'], width=field_width_homing,
              font=font_small).grid(row=h_row, column=1, sticky="ew", padx=2, pady=2)

    tk.Label(homing_controls_frame, text="Torque (%):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(homing_controls_frame, textvariable=variables['homing_torque_percent_var'], width=field_width_homing,
              font=font_small).grid(row=h_row, column=3, sticky="ew", padx=2, pady=2)

    h_row += 1
    home_btn_frame = tk.Frame(homing_controls_frame, bg=homing_controls_frame['bg'])
    home_btn_frame.grid(row=h_row, column=0, columnspan=4, pady=5, sticky="ew")

    ttk.Button(home_btn_frame, text="Execute Machine Home", style='Small.TButton', command=lambda: send_cmd_func(
        f"MACHINE_HOME_MOVE {variables['homing_stroke_len_var'].get()} {variables['homing_rapid_vel_var'].get()} {variables['homing_touch_vel_var'].get()} {variables['homing_acceleration_var'].get()} {variables['homing_retract_dist_var'].get()} {variables['homing_torque_percent_var'].get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(home_btn_frame, text="Execute Cartridge Home", style='Small.TButton', command=lambda: send_cmd_func(
        f"CARTRIDGE_HOME_MOVE {variables['homing_stroke_len_var'].get()} {variables['homing_rapid_vel_var'].get()} {variables['homing_touch_vel_var'].get()} {variables['homing_acceleration_var'].get()} 0 {variables['homing_torque_percent_var'].get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(home_btn_frame, text="Execute Pinch Home", style='Small.TButton',
               command=lambda: send_cmd_func("PINCH_HOME_MOVE")).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
