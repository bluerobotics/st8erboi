import tkinter as tk
from tkinter import ttk


def create_jog_controls(parent, send_cmd_func, ui_elements, variables):
    """
    Creates and populates the Jog controls frame.
    """
    jog_controls_frame = tk.LabelFrame(parent, text="Jog Controls (Active in JOG_MODE)",
                                       bg="#21232b", fg="#0f8", font=("Segoe UI", 10, "bold"))
    ui_elements['jog_controls_frame'] = jog_controls_frame

    font_small = ('Segoe UI', 9)

    # --- Jog Parameters ---
    jog_params_frame = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_params_frame.pack(fill=tk.X, pady=5, padx=5)
    jog_params_frame.grid_columnconfigure(1, weight=1)
    jog_params_frame.grid_columnconfigure(3, weight=1)

    tk.Label(jog_params_frame, text="Dist (mm):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                               column=0,
                                                                                                               sticky="w",
                                                                                                               pady=1,
                                                                                                               padx=2)
    ttk.Entry(jog_params_frame, textvariable=variables['jog_dist_mm_var'], width=8, font=font_small).grid(row=0,
                                                                                                          column=1,
                                                                                                          sticky="ew",
                                                                                                          pady=1,
                                                                                                          padx=(0, 10))

    tk.Label(jog_params_frame, text="Vel (mm/s):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                                column=2,
                                                                                                                sticky="w",
                                                                                                                pady=1,
                                                                                                                padx=2)
    ttk.Entry(jog_params_frame, textvariable=variables['jog_velocity_var'], width=8, font=font_small).grid(row=0,
                                                                                                           column=3,
                                                                                                           sticky="ew",
                                                                                                           pady=1,
                                                                                                           padx=(0, 10))

    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(
        row=1, column=0, sticky="w", pady=1, padx=2)
    ttk.Entry(jog_params_frame, textvariable=variables['jog_acceleration_var'], width=8, font=font_small).grid(row=1,
                                                                                                               column=1,
                                                                                                               sticky="ew",
                                                                                                               pady=1,
                                                                                                               padx=(
                                                                                                               0, 10))

    tk.Label(jog_params_frame, text="Torque (%):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=1,
                                                                                                                column=2,
                                                                                                                sticky="w",
                                                                                                                pady=1,
                                                                                                                padx=2)
    ttk.Entry(jog_params_frame, textvariable=variables['jog_torque_percent_var'], width=8, font=font_small).grid(row=1,
                                                                                                                 column=3,
                                                                                                                 sticky="ew",
                                                                                                                 pady=1,
                                                                                                                 padx=(
                                                                                                                 0, 10))

    # --- Jog Buttons ---
    jog_buttons_area = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_buttons_area.pack(fill=tk.X, pady=5)

    m0_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg'])
    m0_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
    ui_elements['jog_m0_plus'] = ttk.Button(m0_jog_frame, text="▲ M0", style="Jog.TButton", state=tk.DISABLED,
                                            command=lambda: send_cmd_func(
                                                f"JOG_MOVE {variables['jog_dist_mm_var'].get()} 0 {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"))
    ui_elements['jog_m0_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2))
    ui_elements['jog_m0_minus'] = ttk.Button(m0_jog_frame, text="▼ M0", style="Jog.TButton", state=tk.DISABLED,
                                             command=lambda: send_cmd_func(
                                                 f"JOG_MOVE -{variables['jog_dist_mm_var'].get()} 0 {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"))
    ui_elements['jog_m0_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)

    m1_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg'])
    m1_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
    ui_elements['jog_m1_plus'] = ttk.Button(m1_jog_frame, text="▲ M1", style="Jog.TButton", state=tk.DISABLED,
                                            command=lambda: send_cmd_func(
                                                f"JOG_MOVE 0 {variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"))
    ui_elements['jog_m1_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2))
    ui_elements['jog_m1_minus'] = ttk.Button(m1_jog_frame, text="▼ M1", style="Jog.TButton", state=tk.DISABLED,
                                             command=lambda: send_cmd_func(
                                                 f"JOG_MOVE 0 -{variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"))
    ui_elements['jog_m1_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)

    both_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg'])
    both_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
    ui_elements['jog_both_plus'] = ttk.Button(both_jog_frame, text="▲ Both", style="Jog.TButton", state=tk.DISABLED,
                                              command=lambda: send_cmd_func(
                                                  f"JOG_MOVE {variables['jog_dist_mm_var'].get()} {variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"))
    ui_elements['jog_both_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2))
    ui_elements['jog_both_minus'] = ttk.Button(both_jog_frame, text="▼ Both", style="Jog.TButton", state=tk.DISABLED,
                                               command=lambda: send_cmd_func(
                                                   f"JOG_MOVE -{variables['jog_dist_mm_var'].get()} -{variables['jog_dist_mm_var'].get()} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"))
    ui_elements['jog_both_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)

    m2_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg'])
    m2_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2)
    ui_elements['jog_m2_pinch_plus'] = ttk.Button(m2_jog_frame, text="▲ M2 (Pinch)", style="Jog.TButton",
                                                  state=tk.DISABLED,
                                                  command=lambda: send_cmd_func(
                                                      f"PINCH_JOG_MOVE {variables['jog_pinch_degrees_var'].get()} {variables['jog_torque_percent_var'].get()} {variables['jog_pinch_velocity_sps_var'].get()} {variables['jog_pinch_accel_sps2_var'].get()}"))
    ui_elements['jog_m2_pinch_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2))
    ui_elements['jog_m2_pinch_minus'] = ttk.Button(m2_jog_frame, text="▼ M2 (Pinch)", style="Jog.TButton",
                                                   state=tk.DISABLED,
                                                   command=lambda: send_cmd_func(
                                                       f"PINCH_JOG_MOVE -{variables['jog_pinch_degrees_var'].get()} {variables['jog_torque_percent_var'].get()} {variables['jog_pinch_velocity_sps_var'].get()} {variables['jog_pinch_accel_sps2_var'].get()}"))
    ui_elements['jog_m2_pinch_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)
