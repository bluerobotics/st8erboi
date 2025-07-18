import tkinter as tk
from tkinter import ttk


def create_injector_jog_controls(parent, send_cmd_func, variables, ui_elements):
    """
    Creates jog controls specifically for the injector motors (M0 & M1)
    and places them into the provided parent frame.
    """
    jog_controls_frame = tk.LabelFrame(parent, text="Jog Controls",
                                       bg=parent['bg'], fg="#0f8", font=("Segoe UI", 9, "bold"),
                                       padx=5, pady=5)
    jog_controls_frame.pack(fill=tk.X, expand=True, pady=(5, 0))

    font_small = ('Segoe UI', 9)

    # --- Jog Parameters ---
    jog_params_frame = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_params_frame.pack(fill=tk.X, pady=(0, 5))
    jog_params_frame.grid_columnconfigure((1, 3), weight=1)

    tk.Label(jog_params_frame, text="Dist (mm):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                             column=0,
                                                                                                             sticky="e",
                                                                                                             pady=1,
                                                                                                             padx=(0,
                                                                                                                   5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_dist_mm_var'], width=8, font=font_small).grid(row=0,
                                                                                                          column=1,
                                                                                                          sticky="w")

    tk.Label(jog_params_frame, text="Vel (mm/s):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                              column=2,
                                                                                                              sticky="e",
                                                                                                              pady=1,
                                                                                                              padx=(5,
                                                                                                                    5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_velocity_var'], width=8, font=font_small).grid(row=0,
                                                                                                           column=3,
                                                                                                           sticky="w")

    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(
        row=1, column=0, sticky="e", pady=1, padx=(0, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_acceleration_var'], width=8, font=font_small).grid(row=1,
                                                                                                                column=1,
                                                                                                                sticky="w")

    tk.Label(jog_params_frame, text="Torque (%):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=1,
                                                                                                              column=2,
                                                                                                              sticky="e",
                                                                                                              pady=1,
                                                                                                              padx=(5,
                                                                                                                    5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_torque_percent_var'], width=8, font=font_small).grid(row=1,
                                                                                                                  column=3,
                                                                                                                  sticky="w")

    # --- Jog Buttons ---
    jog_buttons_area = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_buttons_area.pack(fill=tk.X, pady=2)
    for i in range(3):  # Now 3 columns for buttons
        jog_buttons_area.grid_columnconfigure(i, weight=1)

    jog_cmd_str = lambda d1, d2: f"JOG_MOVE {d1} {d2} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"

    # --- Motor 0 Buttons (Initially disabled) ---
    ui_elements['jog_m0_up_btn'] = ttk.Button(jog_buttons_area, text="▲ M0", style="Jog.TButton", state=tk.DISABLED,
                                              command=lambda: send_cmd_func(jog_cmd_str(variables['jog_dist_mm_var'].get(), 0)))
    ui_elements['jog_m0_up_btn'].grid(row=0, column=0, sticky="ew", padx=1)
    ui_elements['jog_m0_down_btn'] = ttk.Button(jog_buttons_area, text="▼ M0", style="Jog.TButton", state=tk.DISABLED,
                                                command=lambda: send_cmd_func(jog_cmd_str(f"-{variables['jog_dist_mm_var'].get()}", 0)))
    ui_elements['jog_m0_down_btn'].grid(row=1, column=0, sticky="ew", padx=1, pady=2)

    # --- Motor 1 Buttons (Initially disabled) ---
    ui_elements['jog_m1_up_btn'] = ttk.Button(jog_buttons_area, text="▲ M1", style="Jog.TButton", state=tk.DISABLED,
                                              command=lambda: send_cmd_func(jog_cmd_str(0, variables['jog_dist_mm_var'].get())))
    ui_elements['jog_m1_up_btn'].grid(row=0, column=1, sticky="ew", padx=1)
    ui_elements['jog_m1_down_btn'] = ttk.Button(jog_buttons_area, text="▼ M1", style="Jog.TButton", state=tk.DISABLED,
                                                command=lambda: send_cmd_func(jog_cmd_str(0, f"-{variables['jog_dist_mm_var'].get()}")))
    ui_elements['jog_m1_down_btn'].grid(row=1, column=1, sticky="ew", padx=1, pady=2)

    # --- Both Motors Buttons (Always enabled) ---
    ttk.Button(jog_buttons_area, text="▲ Both", style="Jog.TButton", command=lambda: send_cmd_func(
        jog_cmd_str(variables['jog_dist_mm_var'].get(), variables['jog_dist_mm_var'].get()))).grid(row=0, column=2,
                                                                                                  sticky="ew", padx=1)
    ttk.Button(jog_buttons_area, text="▼ Both", style="Jog.TButton", command=lambda: send_cmd_func(
        jog_cmd_str(f"-{variables['jog_dist_mm_var'].get()}", f"-{variables['jog_dist_mm_var'].get()}"))).grid(row=1,
                                                                                                              column=2,
                                                                                                              sticky="ew",
                                                                                                              padx=1,
                                                                                                              pady=2)


def create_pinch_jog_controls(parent, send_cmd_func, variables):
    """
    Creates jog controls specifically for the pinch motor (M2)
    and places them into the provided parent frame.
    """
    jog_controls_frame = tk.LabelFrame(parent, text="Jog Controls",
                                       bg=parent['bg'], fg="#0f8", font=("Segoe UI", 9, "bold"),
                                       padx=5, pady=5)
    jog_controls_frame.pack(fill=tk.X, expand=True, pady=(5, 0))

    font_small = ('Segoe UI', 9)

    # --- Jog Parameters ---
    jog_params_frame = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_params_frame.pack(fill=tk.X, pady=(0, 5))
    jog_params_frame.grid_columnconfigure((1, 3), weight=1)

    tk.Label(jog_params_frame, text="Dist (°):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                            column=0,
                                                                                                            sticky="e",
                                                                                                            pady=1,
                                                                                                            padx=(0,
                                                                                                                  5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_pinch_degrees_var'], width=8, font=font_small).grid(row=0,
                                                                                                                 column=1,
                                                                                                                 sticky="w")

    tk.Label(jog_params_frame, text="Vel (sps):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                             column=2,
                                                                                                             sticky="e",
                                                                                                             pady=1,
                                                                                                             padx=(5,
                                                                                                                   5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_pinch_velocity_sps_var'], width=8,
              font=font_small).grid(row=0, column=3, sticky="w")

    tk.Label(jog_params_frame, text="Accel (sps²):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(
        row=1, column=0, sticky="e", pady=1, padx=(0, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_pinch_accel_sps2_var'], width=8, font=font_small).grid(
        row=1, column=1, sticky="w")

    tk.Label(jog_params_frame, text="Torque (%):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=1,
                                                                                                              column=2,
                                                                                                              sticky="e",
                                                                                                              pady=1,
                                                                                                              padx=(5,
                                                                                                                    5))
    ttk.Entry(jog_params_frame, textvariable=variables['pinch_jog_torque_percent_var'], width=8,
              font=font_small).grid(row=1, column=3, sticky="w")

    # --- Jog Buttons ---
    jog_buttons_area = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_buttons_area.pack(fill=tk.X, pady=2)
    jog_buttons_area.grid_columnconfigure(0, weight=1)

    pinch_jog_cmd_str = lambda d: f"PINCH_JOG_MOVE {d} {variables['pinch_jog_torque_percent_var'].get()} {variables['jog_pinch_velocity_sps_var'].get()} {variables['jog_pinch_accel_sps2_var'].get()}"

    ttk.Button(jog_buttons_area, text="▲ M2 (Pinch)", style="Jog.TButton",
               command=lambda: send_cmd_func(pinch_jog_cmd_str(variables['jog_pinch_degrees_var'].get()))).grid(row=0,
                                                                                                                column=0,
                                                                                                                sticky="ew",
                                                                                                                padx=1)
    ttk.Button(jog_buttons_area, text="▼ M2 (Pinch)", style="Jog.TButton",
               command=lambda: send_cmd_func(pinch_jog_cmd_str(f"-{variables['jog_pinch_degrees_var'].get()}"))).grid(
        row=1, column=0, sticky="ew", padx=1, pady=2)
