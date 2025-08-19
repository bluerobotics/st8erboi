import tkinter as tk
from tkinter import ttk
import math

MOTOR_STEPS_PER_REV = 800


# MODIFIED: Renamed function for consistency.
def create_fillhead_jog_controls(parent, send_cmd_func, variables, ui_elements):
    """
    Creates jog controls specifically for the fillhead's injector motors (M0 & M1).
    """
    # MODIFIED: Updated LabelFrame text.
    jog_controls_frame = tk.LabelFrame(parent, text="Fillhead Jog",
                                       bg="#2a2d3b", fg="#0f8", font=("Segoe UI", 9, "bold"),
                                       padx=5, pady=5)
    jog_controls_frame.pack(fill=tk.X, expand=False, pady=(5, 0), padx=5)

    font_small = ('Segoe UI', 9)
    field_width = 8

    jog_params_frame = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_params_frame.pack(fill=tk.X, pady=(0, 5))
    jog_params_frame.grid_columnconfigure(1, weight=1)
    jog_params_frame.grid_columnconfigure(3, weight=1)

    tk.Label(jog_params_frame, text="Dist (mm):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                               column=0,
                                                                                                               sticky="e",
                                                                                                               pady=1,
                                                                                                               padx=(
                                                                                                               0, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_dist_mm_var'], width=field_width, font=font_small).grid(
        row=0, column=1, sticky="ew", padx=(0, 5))
    tk.Label(jog_params_frame, text="Vel (mm/s):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                                column=2,
                                                                                                                sticky="e",
                                                                                                                pady=1,
                                                                                                                padx=(
                                                                                                                5, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_velocity_var'], width=field_width, font=font_small).grid(
        row=0, column=3, sticky="ew")

    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(
        row=1, column=0, sticky="e", pady=1, padx=(0, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_acceleration_var'], width=field_width,
              font=font_small).grid(row=1, column=1, sticky="ew", padx=(0, 5))
    tk.Label(jog_params_frame, text="Torque (%):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=1,
                                                                                                                column=2,
                                                                                                                sticky="e",
                                                                                                                pady=1,
                                                                                                                padx=(
                                                                                                                5, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_torque_percent_var'], width=field_width,
              font=font_small).grid(row=1, column=3, sticky="ew")

    jog_buttons_area = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_buttons_area.pack(fill=tk.X, pady=2)
    for i in range(3):
        jog_buttons_area.grid_columnconfigure(i, weight=1)

    jog_cmd_str = lambda d1, d2: f"JOG_MOVE {d1} {d2} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"

    ui_elements['jog_m0_up_btn'] = ttk.Button(jog_buttons_area, text="▲ M0", style="Jog.TButton", state=tk.DISABLED,
                                              command=lambda: send_cmd_func(
                                                  jog_cmd_str(variables['jog_dist_mm_var'].get(), 0)))
    ui_elements['jog_m0_up_btn'].grid(row=0, column=0, sticky="ew", padx=1)
    ui_elements['jog_m0_down_btn'] = ttk.Button(jog_buttons_area, text="▼ M0", style="Jog.TButton", state=tk.DISABLED,
                                                command=lambda: send_cmd_func(
                                                    jog_cmd_str(f"-{variables['jog_dist_mm_var'].get()}", 0)))
    ui_elements['jog_m0_down_btn'].grid(row=1, column=0, sticky="ew", padx=1, pady=2)
    ui_elements['jog_m1_up_btn'] = ttk.Button(jog_buttons_area, text="▲ M1", style="Jog.TButton", state=tk.DISABLED,
                                              command=lambda: send_cmd_func(
                                                  jog_cmd_str(0, variables['jog_dist_mm_var'].get())))
    ui_elements['jog_m1_up_btn'].grid(row=0, column=1, sticky="ew", padx=1)
    ui_elements['jog_m1_down_btn'] = ttk.Button(jog_buttons_area, text="▼ M1", style="Jog.TButton", state=tk.DISABLED,
                                                command=lambda: send_cmd_func(
                                                    jog_cmd_str(0, f"-{variables['jog_dist_mm_var'].get()}")))
    ui_elements['jog_m1_down_btn'].grid(row=1, column=1, sticky="ew", padx=1, pady=2)
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
    Creates jog controls for both pinch valve motors (Injection and Vacuum).
    """
    jog_controls_frame = tk.LabelFrame(parent, text="Pinch Valve Jog",
                                       bg="#2a2d3b", fg="#E0B0FF", font=("Segoe UI", 9, "bold"),
                                       padx=5, pady=5)
    jog_controls_frame.pack(fill=tk.X, expand=False, pady=(5, 0), padx=5)

    font_small = ('Segoe UI', 9)
    field_width = 8

    jog_params_frame = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_params_frame.pack(fill=tk.X, pady=(0, 5))
    jog_params_frame.grid_columnconfigure(1, weight=1)

    tk.Label(jog_params_frame, text="Dist (mm):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                              column=0,
                                                                                                              sticky="e",
                                                                                                              pady=1,
                                                                                                              padx=(
                                                                                                              0, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_pinch_dist_mm_var'], width=field_width,
              font=font_small).grid(row=0, column=1, sticky="ew")


    jog_buttons_area = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_buttons_area.pack(fill=tk.X, pady=2)
    jog_buttons_area.grid_columnconfigure(0, weight=1)
    jog_buttons_area.grid_columnconfigure(1, weight=1)

    # Injection Valve Jog
    ttk.Button(jog_buttons_area, text="▲ Inj Valve", style="Jog.TButton",
               command=lambda: send_cmd_func(f"INJECTION_VALVE_JOG {variables['jog_pinch_dist_mm_var'].get()}")).grid(row=0,
                                                                                                                column=0,
                                                                                                                sticky="ew",
                                                                                                                padx=(0, 1),
                                                                                                                pady=(0,1))
    ttk.Button(jog_buttons_area, text="▼ Inj Valve", style="Jog.TButton",
               command=lambda: send_cmd_func(f"INJECTION_VALVE_JOG -{variables['jog_pinch_dist_mm_var'].get()}")).grid(
        row=1, column=0, sticky="ew", padx=(0, 1))

    # Vacuum Valve Jog
    ttk.Button(jog_buttons_area, text="▲ Vac Valve", style="Jog.TButton",
               command=lambda: send_cmd_func(f"VACUUM_VALVE_JOG {variables['jog_pinch_dist_mm_var'].get()}")).grid(row=0,
                                                                                                                column=1,
                                                                                                                sticky="ew",
                                                                                                                padx=(1, 0),
                                                                                                                pady=(0,1))
    ttk.Button(jog_buttons_area, text="▼ Vac Valve", style="Jog.TButton",
               command=lambda: send_cmd_func(f"VACUUM_VALVE_JOG -{variables['jog_pinch_dist_mm_var'].get()}")).grid(
        row=1, column=1, sticky="ew", padx=(1, 0))


def create_homing_controls(parent, send_cmd_func, variables):
    """
    Creates and populates the Homing controls frame with separate buttons for each homing routine.
    """
    homing_controls_frame = tk.LabelFrame(parent, text="Homing Controls",
                                          bg="#2b1e34", fg="#D8BFD8", font=("Segoe UI", 10, "bold"),
                                          bd=2, relief="ridge", padx=10, pady=10)
    homing_controls_frame.pack(fill=tk.X, expand=False, padx=5, pady=5)

    font_small = ('Segoe UI', 9)
    field_width_homing = 10

    params_frame = tk.Frame(homing_controls_frame, bg=homing_controls_frame['bg'])
    params_frame.pack(fill=tk.X)
    params_frame.grid_columnconfigure(1, weight=1)
    params_frame.grid_columnconfigure(3, weight=1)

    tk.Label(params_frame, text="Stroke (mm):", bg=homing_controls_frame['bg'], fg='white', font=font_small).grid(row=0,
                                                                                                                  column=0,
                                                                                                                  sticky="e",
                                                                                                                  padx=(
                                                                                                                  0, 5),
                                                                                                                  pady=1)
    ttk.Entry(params_frame, textvariable=variables['homing_stroke_len_var'], width=field_width_homing,
              font=font_small).grid(row=0, column=1, sticky="ew", padx=(0, 2))
    tk.Label(params_frame, text="Rapid Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white', font=font_small).grid(
        row=0, column=2, sticky="e", padx=(2, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['homing_rapid_vel_var'], width=field_width_homing,
              font=font_small).grid(row=0, column=3, sticky="ew")

    tk.Label(params_frame, text="Touch Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white', font=font_small).grid(
        row=1, column=0, sticky="e", padx=(0, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['homing_touch_vel_var'], width=field_width_homing,
              font=font_small).grid(row=1, column=1, sticky="ew", padx=(0, 2))
    tk.Label(params_frame, text="Accel (mm/s²):", bg=homing_controls_frame['bg'], fg='white', font=font_small).grid(
        row=1, column=2, sticky="e", padx=(2, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['homing_acceleration_var'], width=field_width_homing,
              font=font_small).grid(row=1, column=3, sticky="ew")

    tk.Label(params_frame, text="M.Retract (mm):", bg=homing_controls_frame['bg'], fg='white', font=font_small).grid(
        row=2, column=0, sticky="e", padx=(0, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['homing_retract_dist_var'], width=field_width_homing,
              font=font_small).grid(row=2, column=1, sticky="ew", padx=(0, 2))
    tk.Label(params_frame, text="Torque (%):", bg=homing_controls_frame['bg'], fg='white', font=font_small).grid(row=2,
                                                                                                                 column=2,
                                                                                                                 sticky="e",
                                                                                                                 padx=(
                                                                                                                 2, 5),
                                                                                                                 pady=1)
    ttk.Entry(params_frame, textvariable=variables['homing_torque_percent_var'], width=field_width_homing,
              font=font_small).grid(row=2, column=3, sticky="ew")

    main_home_btn_frame = tk.Frame(homing_controls_frame, bg=homing_controls_frame['bg'])
    main_home_btn_frame.pack(fill=tk.X, pady=(8, 5))
    main_home_btn_frame.grid_columnconfigure(0, weight=1)
    main_home_btn_frame.grid_columnconfigure(1, weight=1)

    ttk.Button(main_home_btn_frame, text="Machine Home", style='Small.TButton', command=lambda: send_cmd_func(
        f"MACHINE_HOME_MOVE {variables['homing_stroke_len_var'].get()} {variables['homing_rapid_vel_var'].get()} {variables['homing_touch_vel_var'].get()} {variables['homing_acceleration_var'].get()} {variables['homing_retract_dist_var'].get()} {variables['homing_torque_percent_var'].get()}")).grid(
        row=0, column=0, sticky="ew", padx=(0, 2))
    ttk.Button(main_home_btn_frame, text="Cartridge Home", style='Small.TButton', command=lambda: send_cmd_func(
        f"CARTRIDGE_HOME_MOVE {variables['homing_stroke_len_var'].get()} {variables['homing_rapid_vel_var'].get()} {variables['homing_touch_vel_var'].get()} {variables['homing_acceleration_var'].get()} 0 {variables['homing_torque_percent_var'].get()}")).grid(
        row=0, column=1, sticky="ew", padx=2)

    pinch_home_btn_frame = tk.Frame(homing_controls_frame, bg=homing_controls_frame['bg'])
    pinch_home_btn_frame.pack(fill=tk.X)
    pinch_home_btn_frame.grid_columnconfigure(0, weight=1)
    pinch_home_btn_frame.grid_columnconfigure(1, weight=1)

    ttk.Button(pinch_home_btn_frame, text="Home Injection Valve", style='Small.TButton',
               command=lambda: send_cmd_func("INJECTION_VALVE_HOME")).grid(row=0, column=0, sticky="ew", padx=(0, 2))
    ttk.Button(pinch_home_btn_frame, text="Home Vacuum Valve", style='Small.TButton',
               command=lambda: send_cmd_func("VACUUM_VALVE_HOME")).grid(row=0, column=1, sticky="ew", padx=(2, 0))


def create_feed_controls(parent, send_cmd_func, ui_elements, variables):
    """
    Creates and populates the Feed controls frame.
    """
    feed_controls_frame = tk.LabelFrame(parent, text="Feed Controls",
                                        bg="#1e3434", fg="#AFEEEE", font=("Segoe UI", 10, "bold"),
                                        bd=2, relief="ridge", padx=10, pady=10)
    feed_controls_frame.pack(fill=tk.X, expand=False, padx=5, pady=5)

    font_small = ('Segoe UI', 9)
    field_width_feed = 8

    params_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    params_frame.pack(fill=tk.X)
    params_frame.grid_columnconfigure((1, 3), weight=1)

    tk.Label(params_frame, text="Cyl 1 Dia (mm):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=0, column=0, sticky="e", padx=(0, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['feed_cyl1_dia_var'], width=field_width_feed, font=font_small).grid(
        row=0, column=1, sticky="ew", padx=(0, 2))
    tk.Label(params_frame, text="Cyl 2 Dia (mm):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=0, column=2, sticky="e", padx=(2, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['feed_cyl2_dia_var'], width=field_width_feed, font=font_small).grid(
        row=0, column=3, sticky="ew")

    tk.Label(params_frame, text="Pitch (mm/rev):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=1, column=0, sticky="e", padx=(0, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['feed_ballscrew_pitch_var'], width=field_width_feed,
              font=font_small).grid(row=1, column=1, sticky="ew", padx=(0, 2))
    tk.Label(params_frame, text="Feed Accel (sps²):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=1, column=2, sticky="e", padx=(2, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['feed_acceleration_var'], width=field_width_feed,
              font=font_small).grid(row=1, column=3, sticky="ew")

    tk.Label(params_frame, text="Torque Limit (%):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=2, column=2, sticky="e", padx=(2, 5), pady=1)
    ttk.Entry(params_frame, textvariable=variables['feed_torque_percent_var'], width=field_width_feed,
              font=font_small).grid(row=2, column=3, sticky="ew")

    calc_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    calc_frame.pack(fill=tk.X, pady=(5, 0))
    calc_frame.grid_columnconfigure((1, 3), weight=1)
    tk.Label(calc_frame, text="Calc ml/rev:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(row=0,
                                                                                                              column=0,
                                                                                                              sticky='e')
    tk.Label(calc_frame, textvariable=variables['feed_ml_per_rev_var'], bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=0, column=1, sticky='w', padx=5)
    tk.Label(calc_frame, text="Calc Steps/ml:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(row=0,
                                                                                                                column=2,
                                                                                                                sticky='e')
    tk.Label(calc_frame, textvariable=variables['feed_steps_per_ml_var'], bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=0, column=3, sticky='w', padx=5)

    ttk.Separator(feed_controls_frame, orient='horizontal').pack(fill=tk.X, pady=8)

    positioning_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    positioning_frame.pack(fill=tk.X)
    tk.Label(positioning_frame, text="Retract Offset (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).pack(side=tk.LEFT)
    ttk.Entry(positioning_frame, textvariable=variables['cartridge_retract_offset_mm_var'], width=field_width_feed,
              font=font_small).pack(side=tk.LEFT, padx=5)

    positioning_btn_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    positioning_btn_frame.pack(fill=tk.X, pady=(5, 0))
    positioning_btn_frame.grid_columnconfigure((0, 1), weight=1)
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Home", style='Green.TButton',
               command=lambda: send_cmd_func("MOVE_TO_CARTRIDGE_HOME")).grid(row=0, column=0, sticky="ew", padx=(0, 2))
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Retract", style='Green.TButton',
               command=lambda: send_cmd_func(
                   f"MOVE_TO_CARTRIDGE_RETRACT {variables['cartridge_retract_offset_mm_var'].get()}")).grid(row=0,
                                                                                                            column=1,
                                                                                                            sticky="ew",
                                                                                                            padx=(2, 0))

    ttk.Separator(feed_controls_frame, orient='horizontal').pack(fill=tk.X, pady=8)

    inject_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    inject_frame.pack(fill=tk.X)
    inject_frame.grid_columnconfigure((1, 3), weight=1)
    tk.Label(inject_frame, text="Inject Vol (ml):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=0, column=0, sticky='e', padx=(0, 5))
    ttk.Entry(inject_frame, textvariable=variables['inject_amount_ml_var'], width=field_width_feed,
              font=font_small).grid(row=0, column=1, sticky='ew', padx=(0, 2))
    tk.Label(inject_frame, text="Inject Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=0, column=2, sticky='e', padx=(2, 5))
    ttk.Entry(inject_frame, textvariable=variables['inject_speed_ml_s_var'], width=field_width_feed,
              font=font_small).grid(row=0, column=3, sticky='ew')

    inject_status_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    inject_status_frame.pack(fill=tk.X, pady=2)
    tk.Label(inject_status_frame, text="Est. Time:", bg=feed_controls_frame['bg'], fg='white', font=font_small).pack(
        side=tk.LEFT)
    tk.Label(inject_status_frame, textvariable=variables['inject_time_var'], bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).pack(side=tk.LEFT, padx=5)
    tk.Label(inject_status_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).pack(
        side=tk.LEFT, padx=(10, 0))
    tk.Label(inject_status_frame, textvariable=variables['inject_dispensed_ml_var'], bg=feed_controls_frame['bg'],
             fg='lightgreen', font=font_small).pack(side=tk.LEFT, padx=5)

    inject_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    inject_op_frame.pack(fill=tk.X)
    inject_op_frame.grid_columnconfigure((0, 1), weight=1)
    ui_elements['start_inject_btn'] = ttk.Button(inject_op_frame, text="Start Inject", style='Start.TButton',
                                                 state='normal', command=lambda: send_cmd_func(
            f"INJECT_MOVE {variables['inject_amount_ml_var'].get()} {variables['inject_speed_ml_s_var'].get()} {variables['feed_acceleration_var'].get()} {variables['feed_steps_per_ml_var'].get()} {variables['feed_torque_percent_var'].get()}"))
    ui_elements['start_inject_btn'].grid(row=0, column=0, sticky="ew", padx=(0, 2))
    ui_elements['pause_inject_btn'] = ttk.Button(inject_op_frame, text="Pause", style='Pause.TButton', state='disabled',
                                                 command=lambda: send_cmd_func("PAUSE_INJECTION"))
    ui_elements['pause_inject_btn'].grid(row=0, column=1, sticky="ew", padx=(2, 0))
    ui_elements['resume_inject_btn'] = ttk.Button(inject_op_frame, text="Resume", style='Resume.TButton',
                                                  state='disabled', command=lambda: send_cmd_func("RESUME_INJECTION"))
    ui_elements['resume_inject_btn'].grid(row=1, column=0, sticky="ew", padx=(0, 2), pady=4)
    ui_elements['cancel_inject_btn'] = ttk.Button(inject_op_frame, text="Cancel", style='Cancel.TButton',
                                                  state='disabled', command=lambda: send_cmd_func("CANCEL_INJECTION"))
    ui_elements['cancel_inject_btn'].grid(row=1, column=1, sticky="ew", padx=(2, 0), pady=4)

    ttk.Separator(feed_controls_frame, orient='horizontal').pack(fill=tk.X, pady=8)

    purge_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    purge_frame.pack(fill=tk.X)
    purge_frame.grid_columnconfigure((1, 3), weight=1)
    tk.Label(purge_frame, text="Purge Vol (ml):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(row=0,
                                                                                                                  column=0,
                                                                                                                  sticky='e',
                                                                                                                  padx=(
                                                                                                                  0, 5))
    ttk.Entry(purge_frame, textvariable=variables['purge_amount_ml_var'], width=field_width_feed, font=font_small).grid(
        row=0, column=1, sticky='ew', padx=(0, 2))
    tk.Label(purge_frame, text="Purge Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=0, column=2, sticky='e', padx=(2, 5))
    ttk.Entry(purge_frame, textvariable=variables['purge_speed_ml_s_var'], width=field_width_feed,
              font=font_small).grid(row=0, column=3, sticky='ew')

    purge_status_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    purge_status_frame.pack(fill=tk.X, pady=2)
    tk.Label(purge_status_frame, text="Est. Time:", bg=feed_controls_frame['bg'], fg='white', font=font_small).pack(
        side=tk.LEFT)
    tk.Label(purge_status_frame, textvariable=variables['purge_time_var'], bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).pack(side=tk.LEFT, padx=5)
    tk.Label(purge_status_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).pack(
        side=tk.LEFT, padx=(10, 0))
    tk.Label(purge_status_frame, textvariable=variables['purge_dispensed_ml_var'], bg=feed_controls_frame['bg'],
             fg='lightgreen', font=font_small).pack(side=tk.LEFT, padx=5)

    purge_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    purge_op_frame.pack(fill=tk.X)
    purge_op_frame.grid_columnconfigure((0, 1), weight=1)
    ui_elements['start_purge_btn'] = ttk.Button(purge_op_frame, text="Start Purge", style='Start.TButton',
                                                state='normal', command=lambda: send_cmd_func(
            f"PURGE_MOVE {variables['purge_amount_ml_var'].get()} {variables['purge_speed_ml_s_var'].get()} {variables['feed_acceleration_var'].get()} {variables['feed_steps_per_ml_var'].get()} {variables['feed_torque_percent_var'].get()}"))
    ui_elements['start_purge_btn'].grid(row=0, column=0, sticky="ew", padx=(0, 2))
    ui_elements['pause_purge_btn'] = ttk.Button(purge_op_frame, text="Pause", style='Pause.TButton', state='disabled',
                                                command=lambda: send_cmd_func("PAUSE_INJECTION"))
    ui_elements['pause_purge_btn'].grid(row=0, column=1, sticky="ew", padx=(2, 0))
    ui_elements['resume_purge_btn'] = ttk.Button(purge_op_frame, text="Resume", style='Resume.TButton',
                                                 state='disabled', command=lambda: send_cmd_func("RESUME_INJECTION"))
    ui_elements['resume_purge_btn'].grid(row=1, column=0, sticky="ew", padx=(0, 2), pady=4)
    ui_elements['cancel_purge_btn'] = ttk.Button(purge_op_frame, text="Cancel", style='Cancel.TButton',
                                                 state='disabled', command=lambda: send_cmd_func("CANCEL_INJECTION"))
    ui_elements['cancel_purge_btn'].grid(row=1, column=1, sticky="ew", padx=(2, 0), pady=4)


def create_fillhead_ancillary_controls(parent, send_fillhead_cmd, shared_gui_refs, ui_elements):
    """Creates the Jog, Feed, and other non-motor-box controls for the Fillhead."""
    variables = shared_gui_refs

    if 'jog_individual_unlocked_var' not in variables: variables['jog_individual_unlocked_var'] = tk.BooleanVar(
        value=False)
    if 'jog_dist_mm_var' not in variables: variables['jog_dist_mm_var'] = tk.StringVar(value="1.0")
    if 'jog_velocity_var' not in variables: variables['jog_velocity_var'] = tk.StringVar(value="5.0")
    if 'jog_acceleration_var' not in variables: variables['jog_acceleration_var'] = tk.StringVar(value="25.0")
    if 'jog_torque_percent_var' not in variables: variables['jog_torque_percent_var'] = tk.StringVar(value="20.0")
    if 'jog_pinch_dist_mm_var' not in variables: variables['jog_pinch_dist_mm_var'] = tk.StringVar(value="1.0")
    if 'homing_stroke_len_var' not in variables: variables['homing_stroke_len_var'] = tk.StringVar(value='50.0')
    if 'homing_rapid_vel_var' not in variables: variables['homing_rapid_vel_var'] = tk.StringVar(value='20.0')
    if 'homing_touch_vel_var' not in variables: variables['homing_touch_vel_var'] = tk.StringVar(value='2.0')
    if 'homing_acceleration_var' not in variables: variables['homing_acceleration_var'] = tk.StringVar(value='100.0')
    if 'homing_retract_dist_var' not in variables: variables['homing_retract_dist_var'] = tk.StringVar(value='1.0')
    if 'homing_torque_percent_var' not in variables: variables['homing_torque_percent_var'] = tk.StringVar(value='25')
    if 'feed_cyl1_dia_var' not in variables: variables['feed_cyl1_dia_var'] = tk.StringVar(value='10.0')
    if 'feed_cyl2_dia_var' not in variables: variables['feed_cyl2_dia_var'] = tk.StringVar(value='10.0')
    if 'feed_ballscrew_pitch_var' not in variables: variables['feed_ballscrew_pitch_var'] = tk.StringVar(value='2.0')
    if 'feed_ml_per_rev_var' not in variables: variables['feed_ml_per_rev_var'] = tk.StringVar(value='0.0')
    if 'feed_steps_per_ml_var' not in variables: variables['feed_steps_per_ml_var'] = tk.StringVar(value='0.0')
    if 'feed_acceleration_var' not in variables: variables['feed_acceleration_var'] = tk.StringVar(value='5000')
    if 'feed_torque_percent_var' not in variables: variables['feed_torque_percent_var'] = tk.StringVar(value='50')
    if 'cartridge_retract_offset_mm_var' not in variables: variables['cartridge_retract_offset_mm_var'] = tk.StringVar(
        value='1.0')
    if 'inject_amount_ml_var' not in variables: variables['inject_amount_ml_var'] = tk.StringVar(value='0.1')
    if 'inject_speed_ml_s_var' not in variables: variables['inject_speed_ml_s_var'] = tk.StringVar(value='0.1')
    if 'inject_time_var' not in variables: variables['inject_time_var'] = tk.StringVar(value='---')
    if 'purge_amount_ml_var' not in variables: variables['purge_amount_ml_var'] = tk.StringVar(value='0.5')
    if 'purge_speed_ml_s_var' not in variables: variables['purge_speed_ml_s_var'] = tk.StringVar(value='0.5')
    if 'purge_time_var' not in variables: variables['purge_time_var'] = tk.StringVar(value='---')
    if 'purge_dispensed_ml_var' not in variables: variables['purge_dispensed_ml_var'] = tk.StringVar(value='---')

    ancillary_frame = tk.Frame(parent, bg="#2a2d3b")
    ancillary_frame.pack(fill=tk.X, expand=False)

    def toggle_individual_jog():
        new_state = tk.NORMAL if variables['jog_individual_unlocked_var'].get() else tk.DISABLED
        for btn_key in ['jog_m0_up_btn', 'jog_m0_down_btn', 'jog_m1_up_btn', 'jog_m1_down_btn']:
            if btn_key in ui_elements: ui_elements[btn_key].config(state=new_state)

    s = ttk.Style();
    s.configure('Modern.TCheckbutton', background="#2a2d3b", foreground='white');
    s.map('Modern.TCheckbutton', background=[('active', "#2a2d3b")],
          indicatorcolor=[('selected', '#4299e1'), ('!selected', '#718096')])
    interlock_frame = tk.Frame(ancillary_frame, bg="#2a2d3b");
    interlock_frame.pack(fill=tk.X, pady=(5, 0), padx=10)
    ttk.Checkbutton(interlock_frame, text="Jog Individual Motors", style='Modern.TCheckbutton',
                    variable=variables['jog_individual_unlocked_var'], command=toggle_individual_jog).pack(
        side=tk.LEFT)

    # MODIFIED: Call the renamed jog controls function.
    create_fillhead_jog_controls(ancillary_frame, send_fillhead_cmd, variables, ui_elements)
    create_pinch_jog_controls(ancillary_frame, send_fillhead_cmd, variables)
    create_homing_controls(ancillary_frame, send_fillhead_cmd, variables)
    create_feed_controls(ancillary_frame, send_fillhead_cmd, ui_elements, variables)

    return {}
