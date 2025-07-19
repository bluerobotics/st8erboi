import tkinter as tk
from tkinter import ttk
import math


# This file now consolidates all logic for creating the manual control panels
# for both the Injector and the Fillhead devices.

# --- Motor Power Controls ---
def create_motor_power_controls(parent, command_funcs, shared_gui_refs):
    """Creates the central motor power enable/disable controls with dynamic color feedback."""
    power_frame = tk.LabelFrame(parent, text="Motor Power", bg="#2a2d3b", fg="white", padx=5, pady=5)
    power_frame.pack(fill=tk.X, pady=5, padx=5, anchor='n')

    power_frame.grid_columnconfigure((0, 1), weight=1)

    send_injector_cmd = command_funcs['send_injector']
    send_fillhead_cmd = command_funcs['send_fillhead']

    def make_style_tracer(button_map):
        def tracer(*args):
            for btn, var, en_style, dis_style in button_map:
                is_enabled = var.get() == "Enabled"
                btn.config(style=en_style if is_enabled else dis_style)

        return tracer

    injector_frame = tk.Frame(power_frame, bg=power_frame['bg'])
    injector_frame.grid(row=0, column=0, sticky='ew', padx=(0, 2))
    injector_frame.grid_columnconfigure((0, 1), weight=1)

    btn_enable_injector = ttk.Button(injector_frame, text="Enable Injector", style='Small.TButton',
                                     command=lambda: send_injector_cmd("ENABLE"))
    btn_enable_injector.grid(row=0, column=0, sticky='ew', padx=(0, 1))
    btn_disable_injector = ttk.Button(injector_frame, text="Disable Injector", style='Small.TButton',
                                      command=lambda: send_injector_cmd("DISABLE"))
    btn_disable_injector.grid(row=0, column=1, sticky='ew', padx=(1, 0))

    def injector_state_tracer(*args):
        m0_enabled = shared_gui_refs['enabled_state1_var'].get() == "Enabled"
        m1_enabled = shared_gui_refs['enabled_state2_var'].get() == "Enabled"
        if m0_enabled and m1_enabled:
            btn_enable_injector.config(style="Green.TButton")
            btn_disable_injector.config(style="Small.TButton")
        elif not m0_enabled and not m1_enabled:
            btn_enable_injector.config(style="Small.TButton")
            btn_disable_injector.config(style="Red.TButton")
        else:
            btn_enable_injector.config(style="Yellow.TButton")
            btn_disable_injector.config(style="Yellow.TButton")

    shared_gui_refs['enabled_state1_var'].trace_add('write', injector_state_tracer)
    shared_gui_refs['enabled_state2_var'].trace_add('write', injector_state_tracer)
    injector_state_tracer()

    pinch_frame = tk.Frame(power_frame, bg=power_frame['bg'])
    pinch_frame.grid(row=0, column=1, sticky='ew', padx=(2, 0))
    pinch_frame.grid_columnconfigure((0, 1), weight=1)

    btn_enable_pinch = ttk.Button(pinch_frame, text="Enable Pinch", style='Small.TButton',
                                  command=lambda: send_injector_cmd("ENABLE_PINCH"))
    btn_enable_pinch.grid(row=0, column=0, sticky='ew', padx=(0, 1))
    btn_disable_pinch = ttk.Button(pinch_frame, text="Disable Pinch", style='Small.TButton',
                                   command=lambda: send_injector_cmd("DISABLE_PINCH"))
    btn_disable_pinch.grid(row=0, column=1, sticky='ew', padx=(1, 0))

    pinch_tracer = make_style_tracer(
        [(btn_enable_pinch, shared_gui_refs['enabled_state3_var'], "Green.TButton", "Small.TButton"),
         (btn_disable_pinch, shared_gui_refs['enabled_state3_var'], "Small.TButton", "Red.TButton")])
    shared_gui_refs['enabled_state3_var'].trace_add('write', pinch_tracer)
    pinch_tracer()

    ttk.Separator(power_frame, orient='horizontal').grid(row=1, column=0, columnspan=2, sticky='ew', pady=5)

    fh_frame = tk.Frame(power_frame, bg=power_frame['bg'])
    fh_frame.grid(row=2, column=0, columnspan=2, sticky='ew')
    fh_frame.grid_columnconfigure((0, 1, 2), weight=1)

    btn_enable_x = ttk.Button(fh_frame, text="Enable X", style='Small.TButton',
                              command=lambda: send_fillhead_cmd("ENABLE_X"))
    btn_enable_x.grid(row=0, column=0, sticky='ew', padx=(0, 2))
    btn_disable_x = ttk.Button(fh_frame, text="Disable X", style='Small.TButton',
                               command=lambda: send_fillhead_cmd("DISABLE_X"))
    btn_disable_x.grid(row=1, column=0, sticky='ew', padx=(0, 2), pady=2)
    x_tracer = make_style_tracer(
        [(btn_enable_x, shared_gui_refs['fh_enabled_m0_var'], 'Green.TButton', 'Small.TButton'),
         (btn_disable_x, shared_gui_refs['fh_enabled_m0_var'], 'Small.TButton', 'Red.TButton')])
    shared_gui_refs['fh_enabled_m0_var'].trace_add('write', x_tracer)
    x_tracer()

    btn_enable_y = ttk.Button(fh_frame, text="Enable Y", style='Small.TButton',
                              command=lambda: send_fillhead_cmd("ENABLE_Y"))
    btn_enable_y.grid(row=0, column=1, sticky='ew', padx=2)
    btn_disable_y = ttk.Button(fh_frame, text="Disable Y", style='Small.TButton',
                               command=lambda: send_fillhead_cmd("DISABLE_Y"))
    btn_disable_y.grid(row=1, column=1, sticky='ew', padx=2, pady=2)
    y_tracer = make_style_tracer(
        [(btn_enable_y, shared_gui_refs['fh_enabled_m1_var'], 'Green.TButton', 'Small.TButton'),
         (btn_disable_y, shared_gui_refs['fh_enabled_m1_var'], 'Small.TButton', 'Red.TButton')])
    shared_gui_refs['fh_enabled_m1_var'].trace_add('write', y_tracer)
    y_tracer()

    btn_enable_z = ttk.Button(fh_frame, text="Enable Z", style='Small.TButton',
                              command=lambda: send_fillhead_cmd("ENABLE_Z"))
    btn_enable_z.grid(row=0, column=2, sticky='ew', padx=(2, 0))
    btn_disable_z = ttk.Button(fh_frame, text="Disable Z", style='Small.TButton',
                               command=lambda: send_fillhead_cmd("DISABLE_Z"))
    btn_disable_z.grid(row=1, column=2, sticky='ew', padx=(2, 0), pady=2)
    z_tracer = make_style_tracer(
        [(btn_enable_z, shared_gui_refs['fh_enabled_m3_var'], 'Green.TButton', 'Small.TButton'),
         (btn_disable_z, shared_gui_refs['fh_enabled_m3_var'], 'Small.TButton', 'Red.TButton')])
    shared_gui_refs['fh_enabled_m3_var'].trace_add('write', z_tracer)
    z_tracer()


# --- Consolidated Jog Controls ---
def create_jog_controls(parent, command_funcs, shared_gui_refs, ui_elements):
    """Creates a unified jog control panel for all devices."""

    send_injector_cmd = command_funcs['send_injector']
    send_fillhead_cmd = command_funcs['send_fillhead']
    variables = shared_gui_refs

    # Main container for all jog controls
    jog_container_frame = tk.LabelFrame(parent, text="Jog Controls", bg="#2a2d3b", fg="white", padx=5, pady=5)
    jog_container_frame.pack(fill=tk.X, expand=False, pady=(5, 0), padx=5)

    # --- Injector Jog ---
    create_injector_jog_section(jog_container_frame, send_injector_cmd, variables, ui_elements)

    # --- Pinch Jog (Updated) ---
    create_pinch_jog_section(jog_container_frame, send_injector_cmd, variables)

    # --- Fillhead Jog (Integrated) ---
    create_fillhead_jog_section(jog_container_frame, send_fillhead_cmd, variables)


def create_injector_jog_section(parent, send_cmd_func, variables, ui_elements):
    jog_controls_frame = tk.LabelFrame(parent, text="Injector", bg="#2a2d3b", fg="#0f8", font=("Segoe UI", 9, "bold"),
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
    jog_cmd_str = lambda d1,d2: f"JOG_MOVE {d1} {d2} {variables['jog_velocity_var'].get()} {variables['jog_acceleration_var'].get()} {variables['jog_torque_percent_var'].get()}"
    ui_elements['jog_m0_up_btn'] = ttk.Button(jog_buttons_area, text="▲ Up (M0)", style="Jog.TButton",
                                              state=tk.DISABLED, command=lambda: send_cmd_func(
            jog_cmd_str(variables['jog_dist_mm_var'].get(), 0)))
    ui_elements['jog_m0_up_btn'].grid(row=0, column=0, sticky="ew", padx=1)
    ui_elements['jog_m0_down_btn'] = ttk.Button(jog_buttons_area, text="▼ Down (M0)", style="Jog.TButton",
                                                state=tk.DISABLED, command=lambda: send_cmd_func(
            jog_cmd_str(f"-{variables['jog_dist_mm_var'].get()}", 0)))
    ui_elements['jog_m0_down_btn'].grid(row=1, column=0, sticky="ew", padx=1, pady=2)
    ui_elements['jog_m1_up_btn'] = ttk.Button(jog_buttons_area, text="▲ Up (M1)", style="Jog.TButton",
                                              state=tk.DISABLED, command=lambda: send_cmd_func(
            jog_cmd_str(0, variables['jog_dist_mm_var'].get())))
    ui_elements['jog_m1_up_btn'].grid(row=0, column=1, sticky="ew", padx=1)
    ui_elements['jog_m1_down_btn'] = ttk.Button(jog_buttons_area, text="▼ Down (M1)", style="Jog.TButton",
                                                state=tk.DISABLED, command=lambda: send_cmd_func(
            jog_cmd_str(0, f"-{variables['jog_dist_mm_var'].get()}")))
    ui_elements['jog_m1_down_btn'].grid(row=1, column=1, sticky="ew", padx=1, pady=2)
    ttk.Button(jog_buttons_area, text="▲ Up (Both)", style="Jog.TButton", command=lambda: send_cmd_func(
        jog_cmd_str(variables['jog_dist_mm_var'].get(), variables['jog_dist_mm_var'].get()))).grid(row=0, column=2,
                                                                                                   sticky="ew", padx=1)
    ttk.Button(jog_buttons_area, text="▼ Down (Both)", style="Jog.TButton", command=lambda: send_cmd_func(
        jog_cmd_str(f"-{variables['jog_dist_mm_var'].get()}", f"-{variables['jog_dist_mm_var'].get()}"))).grid(row=1,
                                                                                                               column=2,
                                                                                                               sticky="ew",
                                                                                                               padx=1,
                                                                                                               pady=2)


def create_pinch_jog_section(parent, send_cmd_func, variables):
    jog_controls_frame = tk.LabelFrame(parent, text="Pinch", bg="#2a2d3b", fg="#E0B0FF", font=("Segoe UI", 9, "bold"),
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
    ttk.Entry(jog_params_frame, textvariable=variables['jog_pinch_dist_var'], width=field_width, font=font_small).grid(
        row=0, column=1, sticky="ew", padx=(0, 5))
    tk.Label(jog_params_frame, text="Vel (mm/s):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                                column=2,
                                                                                                                sticky="e",
                                                                                                                pady=1,
                                                                                                                padx=(
                                                                                                                5, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_pinch_velocity_var'], width=field_width,
              font=font_small).grid(row=0, column=3, sticky="ew")
    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(
        row=1, column=0, sticky="e", pady=1, padx=(0, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['jog_pinch_accel_var'], width=field_width, font=font_small).grid(
        row=1, column=1, sticky="ew", padx=(0, 5))
    tk.Label(jog_params_frame, text="Torque (%):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=1,
                                                                                                                column=2,
                                                                                                                sticky="e",
                                                                                                                pady=1,
                                                                                                                padx=(
                                                                                                                5, 5))
    ttk.Entry(jog_params_frame, textvariable=variables['pinch_jog_torque_percent_var'], width=field_width,
              font=font_small).grid(row=1, column=3, sticky="ew")
    jog_buttons_area = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg'])
    jog_buttons_area.pack(fill=tk.X, pady=2)
    jog_buttons_area.grid_columnconfigure(0, weight=1)
    jog_buttons_area.grid_columnconfigure(1, weight=1)
    pinch_jog_cmd_str = lambda \
        d: f"PINCH_JOG_MOVE {d} {variables['pinch_jog_torque_percent_var'].get()} {variables['jog_pinch_velocity_var'].get()} {variables['jog_pinch_accel_var'].get()}"
    ttk.Button(jog_buttons_area, text="Pinch Open", style="Jog.TButton",
               command=lambda: send_cmd_func(pinch_jog_cmd_str(variables['jog_pinch_dist_var'].get()))).grid(row=0,
                                                                                                             column=0,
                                                                                                             sticky="ew",
                                                                                                             padx=(
                                                                                                             0, 1))
    ttk.Button(jog_buttons_area, text="Pinch Close", style="Jog.TButton",
               command=lambda: send_cmd_func(pinch_jog_cmd_str(f"-{variables['jog_pinch_dist_var'].get()}"))).grid(
        row=0, column=1, sticky="ew", padx=(1, 0))


def create_fillhead_jog_section(parent, send_fillhead_cmd, variables):
    fh_jog_frame = tk.LabelFrame(parent, text="Fillhead", bg="#2a2d3b", fg="#87CEEB", font=("Segoe UI", 9, "bold"),
                                 padx=5, pady=5)
    fh_jog_frame.pack(fill=tk.X, pady=(5, 0), padx=5, anchor='n')

    jog_params_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b")
    jog_params_frame.pack(fill=tk.X)
    jog_params_frame.grid_columnconfigure(1, weight=1)
    jog_params_frame.grid_columnconfigure(3, weight=1)
    tk.Label(jog_params_frame, text="Dist (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=0, padx=(0, 5), pady=1,
                                                                                 sticky='e')
    ttk.Entry(jog_params_frame, textvariable=variables['fh_jog_dist_mm_var'], width=8).grid(row=0, column=1,
                                                                                            sticky='ew', padx=(0, 5))
    tk.Label(jog_params_frame, text="Speed (mm/s):", bg="#2a2d3b", fg="white").grid(row=0, column=2, padx=(5, 5),
                                                                                    pady=1, sticky='e')
    ttk.Entry(jog_params_frame, textvariable=variables['fh_jog_vel_mms_var'], width=8).grid(row=0, column=3,
                                                                                            sticky='ew')
    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg="#2a2d3b", fg="white").grid(row=1, column=0, padx=(0, 5),
                                                                                     pady=1, sticky='e')
    ttk.Entry(jog_params_frame, textvariable=variables['fh_jog_accel_mms2_var'], width=8).grid(row=1, column=1,
                                                                                               sticky='ew', padx=(0, 5))
    tk.Label(jog_params_frame, text="Torque (%):", bg="#2a2d3b", fg="white").grid(row=1, column=2, padx=(5, 5), pady=1,
                                                                                  sticky='e')
    ttk.Entry(jog_params_frame, textvariable=variables['fh_jog_torque_var'], width=8).grid(row=1, column=3, sticky='ew')
    jog_cmd_str = lambda \
        dist: f"{dist} {variables['fh_jog_vel_mms_var'].get()} {variables['fh_jog_accel_mms2_var'].get()} {variables['fh_jog_torque_var'].get()}"
    jog_btn_frame = tk.Frame(fh_jog_frame, bg="#2a2d3b")
    jog_btn_frame.pack(fill=tk.X, pady=5)
    jog_btn_frame.grid_columnconfigure((0, 1, 2), weight=1)
    ttk.Button(jog_btn_frame, text="X+", style="Jog.TButton",
               command=lambda: send_fillhead_cmd(f"MOVE_X {jog_cmd_str(variables['fh_jog_dist_mm_var'].get())}")).grid(
        row=0, column=0, sticky='ew', padx=(0, 1), pady=(0, 1))
    ttk.Button(jog_btn_frame, text="X-", style="Jog.TButton",
               command=lambda: send_fillhead_cmd(f"MOVE_X -{jog_cmd_str(variables['fh_jog_dist_mm_var'].get())}")).grid(
        row=1, column=0, sticky='ew', padx=(0, 1))
    ttk.Button(jog_btn_frame, text="Y+", style="Jog.TButton",
               command=lambda: send_fillhead_cmd(f"MOVE_Y {jog_cmd_str(variables['fh_jog_dist_mm_var'].get())}")).grid(
        row=0, column=1, sticky='ew', padx=1, pady=(0, 1))
    ttk.Button(jog_btn_frame, text="Y-", style="Jog.TButton",
               command=lambda: send_fillhead_cmd(f"MOVE_Y -{jog_cmd_str(variables['fh_jog_dist_mm_var'].get())}")).grid(
        row=1, column=1, sticky='ew', padx=1)
    ttk.Button(jog_btn_frame, text="Z+", style="Jog.TButton",
               command=lambda: send_fillhead_cmd(f"MOVE_Z {jog_cmd_str(variables['fh_jog_dist_mm_var'].get())}")).grid(
        row=0, column=2, sticky='ew', padx=(1, 0), pady=(0, 1))
    ttk.Button(jog_btn_frame, text="Z-", style="Jog.TButton",
               command=lambda: send_fillhead_cmd(f"MOVE_Z -{jog_cmd_str(variables['fh_jog_dist_mm_var'].get())}")).grid(
        row=1, column=2, sticky='ew', padx=(1, 0))


# --- Homing and Feed Controls ---
def create_homing_controls(parent, command_funcs, variables):
    send_injector_cmd = command_funcs['send_injector']
    send_fillhead_cmd = command_funcs['send_fillhead']
    homing_controls_frame = tk.LabelFrame(parent, text="Homing Controls", bg="#2b1e34", fg="#D8BFD8",
                                          font=("Segoe UI", 10, "bold"), bd=2, relief="ridge", padx=10, pady=10)
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

    home_btn_frame = tk.Frame(homing_controls_frame, bg=homing_controls_frame['bg'])
    home_btn_frame.pack(fill=tk.X, pady=(8, 0))
    for i in range(3):
        home_btn_frame.grid_columnconfigure(i, weight=1)
    ttk.Button(home_btn_frame, text="Machine Home", style='Home.TButton', command=lambda: send_injector_cmd(
        f"MACHINE_HOME_MOVE {variables['homing_stroke_len_var'].get()} {variables['homing_rapid_vel_var'].get()} {variables['homing_touch_vel_var'].get()} {variables['homing_acceleration_var'].get()} {variables['homing_retract_dist_var'].get()} {variables['homing_torque_percent_var'].get()}")).grid(
        row=0, column=0, sticky="ew", padx=(0, 2))
    ttk.Button(home_btn_frame, text="Cartridge Home", style='Home.TButton', command=lambda: send_injector_cmd(
        f"CARTRIDGE_HOME_MOVE {variables['homing_stroke_len_var'].get()} {variables['homing_rapid_vel_var'].get()} {variables['homing_touch_vel_var'].get()} {variables['homing_acceleration_var'].get()} 0 {variables['homing_torque_percent_var'].get()}")).grid(
        row=0, column=1, sticky="ew", padx=2)
    ttk.Button(home_btn_frame, text="Pinch Home", style='Home.TButton',
               command=lambda: send_injector_cmd("PINCH_HOME_MOVE")).grid(row=0, column=2, sticky="ew", padx=(2, 0))

    ttk.Separator(homing_controls_frame, orient='horizontal').pack(fill='x', pady=10)

    fh_home_params_frame = tk.Frame(homing_controls_frame, bg="#2a2d3b")
    fh_home_params_frame.pack(fill=tk.X)
    fh_home_params_frame.grid_columnconfigure(1, weight=1)
    fh_home_params_frame.grid_columnconfigure(3, weight=1)
    tk.Label(fh_home_params_frame, text="FH Torque (%):", bg="#2a2d3b", fg="white").grid(row=0, column=0, sticky="e",
                                                                                         pady=1, padx=(0, 5))
    ttk.Entry(fh_home_params_frame, textvariable=variables['fh_home_torque_var'], width=10).grid(row=0, column=1,
                                                                                                 sticky="ew",
                                                                                                 padx=(0, 5))
    tk.Label(fh_home_params_frame, text="FH Max Dist (mm):", bg="#2a2d3b", fg="white").grid(row=0, column=2, sticky="e",
                                                                                            pady=1, padx=(5, 5))
    ttk.Entry(fh_home_params_frame, textvariable=variables['fh_home_distance_mm_var'], width=10).grid(row=0, column=3,
                                                                                                      sticky="ew")

    fh_home_btn_frame = tk.Frame(homing_controls_frame, bg="#2a2d3b")
    fh_home_btn_frame.pack(pady=(5, 0), fill=tk.X)
    fh_home_btn_frame.grid_columnconfigure((0, 1, 2), weight=1)
    home_cmd_str = lambda \
        axis: f"HOME_{axis} {variables['fh_home_torque_var'].get()} {variables['fh_home_distance_mm_var'].get()}"
    ttk.Button(fh_home_btn_frame, text="Home X", style='Home.TButton',
               command=lambda: send_fillhead_cmd(home_cmd_str("X"))).grid(row=0, column=0, sticky='ew', padx=(0, 2))
    ttk.Button(fh_home_btn_frame, text="Home Y", style='Home.TButton',
               command=lambda: send_fillhead_cmd(home_cmd_str("Y"))).grid(row=0, column=1, sticky='ew', padx=2)
    ttk.Button(fh_home_btn_frame, text="Home Z", style='Home.TButton',
               command=lambda: send_fillhead_cmd(home_cmd_str("Z"))).grid(row=0, column=2, sticky='ew', padx=(2, 0))


def create_feed_controls(parent, send_cmd_func, ui_elements, variables):
    feed_controls_frame = tk.LabelFrame(parent, text="Feed Controls (Injector)", bg="#1e3434", fg="#AFEEEE",
                                        font=("Segoe UI", 10, "bold"), bd=2, relief="ridge", padx=10, pady=10)
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


# --- Main Collapsible Panel Creation ---
def create_manual_controls_panel(parent, command_funcs, shared_gui_refs):
    """Creates the scrollable panel containing all manual device controls."""
    canvas = tk.Canvas(parent, bg="#2a2d3b", highlightthickness=0)
    scrollbar = ttk.Scrollbar(parent, orient="vertical", command=canvas.yview)
    scrollable_frame = tk.Frame(canvas, bg="#2a2d3b")

    def on_canvas_configure(event):
        canvas.itemconfig(window_id, width=event.width)

    scrollable_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

    def _on_mousewheel(event):
        canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    canvas.bind_all("<MouseWheel>", _on_mousewheel)
    scrollable_frame.bind("<MouseWheel>", _on_mousewheel)

    window_id = canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
    canvas.configure(yscrollcommand=scrollbar.set)
    canvas.bind("<Configure>", on_canvas_configure)

    canvas.pack(side="left", fill="both", expand=True)
    scrollbar.pack(side="right", fill="y")

    ui_elements = {}

    # Initialize all necessary StringVars before creating widgets that use them
    if 'jog_individual_unlocked_var' not in shared_gui_refs: shared_gui_refs[
        'jog_individual_unlocked_var'] = tk.BooleanVar(value=False)
    if 'jog_dist_mm_var' not in shared_gui_refs: shared_gui_refs['jog_dist_mm_var'] = tk.StringVar(value="1.0")
    if 'jog_velocity_var' not in shared_gui_refs: shared_gui_refs['jog_velocity_var'] = tk.StringVar(value="5.0")
    if 'jog_acceleration_var' not in shared_gui_refs: shared_gui_refs['jog_acceleration_var'] = tk.StringVar(
        value="25.0")
    if 'jog_torque_percent_var' not in shared_gui_refs: shared_gui_refs['jog_torque_percent_var'] = tk.StringVar(
        value="20.0")
    if 'jog_pinch_dist_var' not in shared_gui_refs: shared_gui_refs['jog_pinch_dist_var'] = tk.StringVar(value="1.0")
    if 'jog_pinch_velocity_var' not in shared_gui_refs: shared_gui_refs['jog_pinch_velocity_var'] = tk.StringVar(
        value="10.0")
    if 'jog_pinch_accel_var' not in shared_gui_refs: shared_gui_refs['jog_pinch_accel_var'] = tk.StringVar(value="50.0")
    if 'pinch_jog_torque_percent_var' not in shared_gui_refs: shared_gui_refs[
        'pinch_jog_torque_percent_var'] = tk.StringVar(value="20.0")
    if 'fh_jog_dist_mm_var' not in shared_gui_refs: shared_gui_refs['fh_jog_dist_mm_var'] = tk.StringVar(value="10.0")
    if 'fh_jog_vel_mms_var' not in shared_gui_refs: shared_gui_refs['fh_jog_vel_mms_var'] = tk.StringVar(value="15.0")
    if 'fh_jog_accel_mms2_var' not in shared_gui_refs: shared_gui_refs['fh_jog_accel_mms2_var'] = tk.StringVar(
        value="50.0")
    if 'fh_jog_torque_var' not in shared_gui_refs: shared_gui_refs['fh_jog_torque_var'] = tk.StringVar(value="20")
    if 'homing_stroke_len_var' not in shared_gui_refs: shared_gui_refs['homing_stroke_len_var'] = tk.StringVar(
        value='50.0')
    if 'homing_rapid_vel_var' not in shared_gui_refs: shared_gui_refs['homing_rapid_vel_var'] = tk.StringVar(
        value='20.0')
    if 'homing_touch_vel_var' not in shared_gui_refs: shared_gui_refs['homing_touch_vel_var'] = tk.StringVar(
        value='2.0')
    if 'homing_acceleration_var' not in shared_gui_refs: shared_gui_refs['homing_acceleration_var'] = tk.StringVar(
        value='100.0')
    if 'homing_retract_dist_var' not in shared_gui_refs: shared_gui_refs['homing_retract_dist_var'] = tk.StringVar(
        value='1.0')
    if 'homing_torque_percent_var' not in shared_gui_refs: shared_gui_refs['homing_torque_percent_var'] = tk.StringVar(
        value='25')
    if 'fh_home_torque_var' not in shared_gui_refs: shared_gui_refs['fh_home_torque_var'] = tk.StringVar(value="20")
    if 'fh_home_distance_mm_var' not in shared_gui_refs: shared_gui_refs['fh_home_distance_mm_var'] = tk.StringVar(
        value="420.0")
    if 'feed_cyl1_dia_var' not in shared_gui_refs: shared_gui_refs['feed_cyl1_dia_var'] = tk.StringVar(value='10.0')
    if 'feed_cyl2_dia_var' not in shared_gui_refs: shared_gui_refs['feed_cyl2_dia_var'] = tk.StringVar(value='10.0')
    if 'feed_ballscrew_pitch_var' not in shared_gui_refs: shared_gui_refs['feed_ballscrew_pitch_var'] = tk.StringVar(
        value='2.0')
    if 'feed_ml_per_rev_var' not in shared_gui_refs: shared_gui_refs['feed_ml_per_rev_var'] = tk.StringVar(value='0.0')
    if 'feed_steps_per_ml_var' not in shared_gui_refs: shared_gui_refs['feed_steps_per_ml_var'] = tk.StringVar(
        value='0.0')
    if 'feed_acceleration_var' not in shared_gui_refs: shared_gui_refs['feed_acceleration_var'] = tk.StringVar(
        value='5000')
    if 'feed_torque_percent_var' not in shared_gui_refs: shared_gui_refs['feed_torque_percent_var'] = tk.StringVar(
        value='50')
    if 'cartridge_retract_offset_mm_var' not in shared_gui_refs: shared_gui_refs[
        'cartridge_retract_offset_mm_var'] = tk.StringVar(value='1.0')
    if 'inject_amount_ml_var' not in shared_gui_refs: shared_gui_refs['inject_amount_ml_var'] = tk.StringVar(
        value='0.1')
    if 'inject_speed_ml_s_var' not in shared_gui_refs: shared_gui_refs['inject_speed_ml_s_var'] = tk.StringVar(
        value='0.1')
    if 'inject_time_var' not in shared_gui_refs: shared_gui_refs['inject_time_var'] = tk.StringVar(value='---')
    if 'inject_dispensed_ml_var' not in shared_gui_refs: shared_gui_refs['inject_dispensed_ml_var'] = tk.StringVar(
        value='---')
    if 'purge_amount_ml_var' not in shared_gui_refs: shared_gui_refs['purge_amount_ml_var'] = tk.StringVar(value='0.5')
    if 'purge_speed_ml_s_var' not in shared_gui_refs: shared_gui_refs['purge_speed_ml_s_var'] = tk.StringVar(
        value='0.5')
    if 'purge_time_var' not in shared_gui_refs: shared_gui_refs['purge_time_var'] = tk.StringVar(value='---')
    if 'purge_dispensed_ml_var' not in shared_gui_refs: shared_gui_refs['purge_dispensed_ml_var'] = tk.StringVar(
        value='---')

    # Create and pack all the control sections into the scrollable frame
    create_motor_power_controls(scrollable_frame, command_funcs, shared_gui_refs)
    create_jog_controls(scrollable_frame, command_funcs, shared_gui_refs, ui_elements)
    create_homing_controls(scrollable_frame, command_funcs, shared_gui_refs)
    create_feed_controls(scrollable_frame, command_funcs['send_injector'], ui_elements, shared_gui_refs)


def create_manual_controls_display(parent, command_funcs, shared_gui_refs):
    """Creates the main collapsible container for the manual controls."""
    container = tk.Frame(parent, bg="#21232b")
    container.pack(side=tk.TOP, fill=tk.BOTH, expand=True, pady=(10, 0))

    manual_panel = tk.Frame(container, bg="#2a2d3b")
    create_manual_controls_panel(manual_panel, command_funcs, shared_gui_refs)

    def toggle_manual_controls():
        if manual_panel.winfo_ismapped():
            manual_panel.pack_forget()
            toggle_button.config(text="Show Manual Controls ▼")
        else:
            manual_panel.pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=(5, 0))
            toggle_button.config(text="Hide Manual Controls ▲")

    toggle_button = ttk.Button(container, text="Show Manual Controls ▼", command=toggle_manual_controls,
                               style="Neutral.TButton")
    toggle_button.pack(side=tk.TOP, fill=tk.X, ipady=5)

    return {}
