import tkinter as tk
from tkinter import ttk
import math

from injector_jog_gui import create_injector_jog_controls, create_pinch_jog_controls
from injector_feed_gui import create_feed_controls

MOTOR_STEPS_PER_REV = 800


def create_injector_motor_boxes(parent, send_injector_cmd, shared_gui_refs, ui_elements):
    """
    Creates and packs the individual motor status and control frames for the Injector
    into a given parent widget.
    """
    variables = shared_gui_refs
    font_motor_disp = ('Segoe UI', 8)

    # --- FIX: Initialize missing StringVars ---
    if 'enabled_state1_var' not in variables:
        variables['enabled_state1_var'] = tk.StringVar(value="Disabled")
    if 'enabled_state2_var' not in variables:
        variables['enabled_state2_var'] = tk.StringVar(value="Disabled")
    if 'enabled_state3_var' not in variables:
        variables['enabled_state3_var'] = tk.StringVar(value="Disabled")
    if 'homed0_var' not in variables:
        variables['homed0_var'] = tk.StringVar(value="Not Homed")
    if 'homed1_var' not in variables:
        variables['homed1_var'] = tk.StringVar(value="Not Homed")
    if 'pinch_homed_var' not in variables:
        variables['pinch_homed_var'] = tk.StringVar(value="Not Homed")
    if 'enable_disable_var' not in variables:
        variables['enable_disable_var'] = tk.StringVar(value="Disable")
    if 'enable_disable_pinch_var' not in variables:
        variables['enable_disable_pinch_var'] = tk.StringVar(value="Disable")

    # --- END FIX ---

    def update_enable_color(var, label):
        label.config(fg="lightgreen" if var.get() == "Enabled" else "orangered")

    def update_homed_color(var, label):
        label.config(fg="lightgreen" if var.get() == "Homed" else "orangered")

    def update_button_styles(enabled_var, enable_btn, disable_btn, enabled_value="Enabled"):
        if enabled_var.get() == enabled_value:
            enable_btn.config(style="Green.TButton")
            disable_btn.config(style="Neutral.TButton")
        else:
            enable_btn.config(style="Neutral.TButton")
            disable_btn.config(style="Red.TButton")

    def create_enable_buttons(p, enabled_var, enable_cmd, disable_cmd, enabled_value="Enabled"):
        button_pair_frame = tk.Frame(p, bg=p['bg'])
        enable_btn = ttk.Button(button_pair_frame, text="Enable", width=7, style="Neutral.TButton", command=enable_cmd)
        enable_btn.pack(side=tk.LEFT, expand=True, fill=tk.X)
        disable_btn = ttk.Button(button_pair_frame, text="Disable", width=7, style="Red.TButton", command=disable_cmd)
        disable_btn.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(2, 0))
        enabled_var.trace_add('write',
                              lambda *args: update_button_styles(enabled_var, enable_btn, disable_btn, enabled_value))
        update_button_styles(enabled_var, enable_btn, disable_btn, enabled_value)
        return button_pair_frame

    # --- Injector Motors (0 & 1) Frame ---
    injector_motors_frame = tk.LabelFrame(parent, text="Injector Motors (0 & 1)", bg="#1b2432", fg="#00bfff",
                                          font=("Segoe UI", 9, "bold"), bd=1, relief="ridge", padx=5, pady=5)
    injector_motors_frame.pack(side=tk.TOP, fill=tk.X, expand=True, pady=(2, 2))

    m0_frame = tk.Frame(injector_motors_frame, bg=injector_motors_frame['bg']);
    m0_frame.pack(fill=tk.X)
    m0_status_frame = tk.Frame(m0_frame, bg=injector_motors_frame['bg']);
    m0_status_frame.pack(side=tk.LEFT, anchor='n')
    tk.Label(m0_status_frame, text="Enabled:", bg=injector_motors_frame['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    enabled_label_m0 = tk.Label(m0_status_frame, textvariable=variables['enabled_state1_var'],
                                bg=injector_motors_frame['bg'], font=font_motor_disp);
    enabled_label_m0.grid(row=1, column=1, sticky="w", padx=2)
    tk.Label(m0_status_frame, text="Homed:", bg=injector_motors_frame['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    homed_label_m0 = tk.Label(m0_status_frame, textvariable=variables['homed0_var'], bg=injector_motors_frame['bg'],
                              font=font_motor_disp, fg="orangered");
    homed_label_m0.grid(row=2, column=1, sticky="w", padx=2)
    variables['enabled_state1_var'].trace_add('write',
                                              lambda *args: update_enable_color(variables['enabled_state1_var'],
                                                                                enabled_label_m0))
    variables['homed0_var'].trace_add('write',
                                      lambda *args: update_homed_color(variables['homed0_var'], homed_label_m0))

    ttk.Separator(injector_motors_frame, orient='horizontal').pack(fill=tk.X, pady=5)

    m1_frame = tk.Frame(injector_motors_frame, bg=injector_motors_frame['bg']);
    m1_frame.pack(fill=tk.X)
    m1_status_frame = tk.Frame(m1_frame, bg=injector_motors_frame['bg']);
    m1_status_frame.pack(side=tk.LEFT, anchor='n')
    tk.Label(m1_status_frame, text="Enabled:", bg=injector_motors_frame['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    enabled_label_m1 = tk.Label(m1_status_frame, textvariable=variables['enabled_state2_var'],
                                bg=injector_motors_frame['bg'], font=font_motor_disp);
    enabled_label_m1.grid(row=1, column=1, sticky="w", padx=2)
    tk.Label(m1_status_frame, text="Homed:", bg=injector_motors_frame['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    homed_label_m1 = tk.Label(m1_status_frame, textvariable=variables['homed1_var'], bg=injector_motors_frame['bg'],
                              font=font_motor_disp, fg="orangered");
    homed_label_m1.grid(row=2, column=1, sticky="w", padx=2)
    variables['enabled_state2_var'].trace_add('write',
                                              lambda *args: update_enable_color(variables['enabled_state2_var'],
                                                                                enabled_label_m1))
    variables['homed1_var'].trace_add('write',
                                      lambda *args: update_homed_color(variables['homed1_var'], homed_label_m1))

    create_enable_buttons(injector_motors_frame, variables['enable_disable_var'], lambda: send_injector_cmd("ENABLE"),
                          lambda: send_injector_cmd("DISABLE")).pack(pady=(5, 0))
    injector_home_btn_frame = tk.Frame(injector_motors_frame, bg=injector_motors_frame['bg']);
    injector_home_btn_frame.pack(fill=tk.X, pady=(5, 0));
    injector_home_btn_frame.grid_columnconfigure((0, 1), weight=1)
    ttk.Button(injector_home_btn_frame, text="Machine Home", style='Small.TButton',
               command=lambda: send_injector_cmd("MACHINE_HOME_MOVE")).grid(row=0, column=0, sticky="ew", padx=(0, 2))
    ttk.Button(injector_home_btn_frame, text="Cartridge Home", style='Small.TButton',
               command=lambda: send_injector_cmd("CARTRIDGE_HOME_MOVE")).grid(row=0, column=1, sticky="ew", padx=(2, 0))

    # --- Pinch Motor (2) Frame ---
    pinch_motor_frame = tk.LabelFrame(parent, text="Pinch Motor (2)", bg="#2d253a", fg="#E0B0FF",
                                      font=("Segoe UI", 9, "bold"), bd=1, relief="ridge", padx=5, pady=5)
    pinch_motor_frame.pack(side=tk.TOP, fill=tk.X, expand=True, pady=(2, 2))
    m2_frame = tk.Frame(pinch_motor_frame, bg=pinch_motor_frame['bg']);
    m2_frame.pack(fill=tk.X)
    m2_status_frame = tk.Frame(m2_frame, bg=pinch_motor_frame['bg']);
    m2_status_frame.pack(side=tk.LEFT, anchor='n')
    tk.Label(m2_status_frame, text="Enabled:", bg=pinch_motor_frame['bg'], fg="white", font=font_motor_disp).grid(row=1,
                                                                                                                  column=0,
                                                                                                                  sticky="w");
    enabled_label_m2 = tk.Label(m2_status_frame, textvariable=variables['enabled_state3_var'],
                                bg=pinch_motor_frame['bg'], font=font_motor_disp);
    enabled_label_m2.grid(row=1, column=1, sticky="w", padx=2)
    tk.Label(m2_status_frame, text="Homed:", bg=pinch_motor_frame['bg'], fg="white", font=font_motor_disp).grid(row=2,
                                                                                                                column=0,
                                                                                                                sticky="w");
    pinch_homed_label = tk.Label(m2_status_frame, textvariable=variables['pinch_homed_var'], bg=pinch_motor_frame['bg'],
                                 font=font_motor_disp, fg="orangered");
    pinch_homed_label.grid(row=2, column=1, sticky="w", padx=2)
    variables['enabled_state3_var'].trace_add('write',
                                              lambda *args: update_enable_color(variables['enabled_state3_var'],
                                                                                enabled_label_m2))
    variables['pinch_homed_var'].trace_add('write', lambda *args: update_homed_color(variables['pinch_homed_var'],
                                                                                     pinch_homed_label))
    create_enable_buttons(pinch_motor_frame, variables['enable_disable_pinch_var'],
                          lambda: send_injector_cmd("ENABLE_PINCH"), lambda: send_injector_cmd("DISABLE_PINCH")).pack(
        pady=(5, 0))
    pinch_home_btn_frame = tk.Frame(pinch_motor_frame, bg=pinch_motor_frame['bg']);
    pinch_home_btn_frame.pack(fill=tk.X, pady=(5, 0));
    ttk.Button(pinch_home_btn_frame, text="Pinch Home", style='Small.TButton',
               command=lambda: send_injector_cmd("PINCH_HOME_MOVE")).pack(fill=tk.X)

    return ui_elements


def create_injector_ancillary_controls(parent, send_injector_cmd, shared_gui_refs, ui_elements):
    """Creates the Jog, Feed, and other non-motor-box controls for the Injector."""
    variables = shared_gui_refs

    # --- FIX: Initialize all ancillary control variables ---
    if 'jog_individual_unlocked_var' not in variables:
        variables['jog_individual_unlocked_var'] = tk.BooleanVar(value=False)
    if 'jog_dist_mm_var' not in variables:
        variables['jog_dist_mm_var'] = tk.StringVar(value="1.0")
    if 'jog_velocity_var' not in variables:
        variables['jog_velocity_var'] = tk.StringVar(value="5.0")
    if 'jog_acceleration_var' not in variables:
        variables['jog_acceleration_var'] = tk.StringVar(value="25.0")
    if 'jog_torque_percent_var' not in variables:
        variables['jog_torque_percent_var'] = tk.StringVar(value="20.0")
    if 'main_state_var' not in variables:
        variables['main_state_var'] = tk.StringVar(value="---")
    if 'homing_state_var' not in variables:
        variables['homing_state_var'] = tk.StringVar(value="---")
    if 'homing_phase_var' not in variables:
        variables['homing_phase_var'] = tk.StringVar(value="---")
    if 'feed_state_var' not in variables:
        variables['feed_state_var'] = tk.StringVar(value="---")
    if 'error_state_var' not in variables:
        variables['error_state_var'] = tk.StringVar(value="---")
    if 'temp_c_var' not in variables:
        variables['temp_c_var'] = tk.StringVar(value="---")
    if 'heater_mode_var' not in variables:
        variables['heater_mode_var'] = tk.StringVar(value="---")
    if 'vacuum_psig_var' not in variables:
        variables['vacuum_psig_var'] = tk.StringVar(value="---")
    if 'machine_steps_var' not in variables:
        variables['machine_steps_var'] = tk.StringVar(value="---")
    if 'cartridge_steps_var' not in variables:
        variables['cartridge_steps_var'] = tk.StringVar(value="---")
    if 'jog_pinch_degrees_var' not in variables:
        variables['jog_pinch_degrees_var'] = tk.StringVar(value="15.0")
    if 'jog_pinch_velocity_sps_var' not in variables:
        variables['jog_pinch_velocity_sps_var'] = tk.StringVar(value="1000")
    if 'jog_pinch_accel_sps2_var' not in variables:
        variables['jog_pinch_accel_sps2_var'] = tk.StringVar(value="5000")
    if 'pinch_jog_torque_percent_var' not in variables:
        variables['pinch_jog_torque_percent_var'] = tk.StringVar(value="25.0")
    # --- END FIX ---

    ancillary_frame = tk.Frame(parent, bg="#2a2d3b")
    ancillary_frame.pack(fill=tk.X, expand=True)

    font_small = ('Segoe UI', 8)
    font_label = ('Segoe UI', 8)
    font_value = ('Segoe UI', 8, 'bold')

    # --- Jog Interlock Checkbox ---
    def toggle_individual_jog():
        new_state = tk.NORMAL if variables['jog_individual_unlocked_var'].get() else tk.DISABLED
        for btn_key in ['jog_m0_up_btn', 'jog_m0_down_btn', 'jog_m1_up_btn', 'jog_m1_down_btn']:
            if btn_key in ui_elements: ui_elements[btn_key].config(state=new_state)

    s = ttk.Style();
    s.configure('Modern.TCheckbutton', background="#2a2d3b", foreground='white');
    s.map('Modern.TCheckbutton', background=[('active', "#2a2d3b")],
          indicatorcolor=[('selected', '#4299e1'), ('!selected', '#718096')])
    interlock_frame = tk.Frame(ancillary_frame, bg="#2a2d3b");
    interlock_frame.pack(fill=tk.X, pady=(5, 0), padx=5)
    ttk.Checkbutton(interlock_frame, text="Jog Individual Motors", style='Modern.TCheckbutton',
                    variable=variables['jog_individual_unlocked_var'], command=toggle_individual_jog).pack(side=tk.LEFT)

    create_injector_jog_controls(ancillary_frame, send_injector_cmd, variables, ui_elements)
    create_pinch_jog_controls(ancillary_frame, send_injector_cmd, variables)

    # --- System State Display ---
    sub_state_display_frame = tk.LabelFrame(ancillary_frame, text="System State", bg="#2a2d3b", fg="white", bd=1,
                                            relief="groove");
    sub_state_display_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 5), ipady=3)
    sub_state_display_frame.grid_columnconfigure(1, weight=1)
    s_row = 0
    tk.Label(sub_state_display_frame, text="Main:", bg=sub_state_display_frame['bg'], fg="#ccc", font=font_small).grid(
        row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['main_state_var'], bg=sub_state_display_frame['bg'],
             fg="white", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1
    tk.Label(sub_state_display_frame, text="Homing:", bg=sub_state_display_frame['bg'], fg="#ccc",
             font=font_small).grid(row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['homing_state_var'], bg=sub_state_display_frame['bg'],
             fg="white", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1
    tk.Label(sub_state_display_frame, text="H. Phase:", bg=sub_state_display_frame['bg'], fg="#ccc",
             font=font_small).grid(row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['homing_phase_var'], bg=sub_state_display_frame['bg'],
             fg="white", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1
    tk.Label(sub_state_display_frame, text="Feed:", bg=sub_state_display_frame['bg'], fg="#ccc", font=font_small).grid(
        row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['feed_state_var'], bg=sub_state_display_frame['bg'],
             fg="white", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1
    tk.Label(sub_state_display_frame, text="Error:", bg=sub_state_display_frame['bg'], fg="#ccc", font=font_small).grid(
        row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['error_state_var'], bg=sub_state_display_frame['bg'],
             fg="orange red", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1
    tk.Label(sub_state_display_frame, text="Temp:", bg=sub_state_display_frame['bg'], fg="#ccc", font=font_small).grid(
        row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['temp_c_var'], bg=sub_state_display_frame['bg'],
             fg="orange", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1
    tk.Label(sub_state_display_frame, text="Heater:", bg=sub_state_display_frame['bg'], fg="#ccc",
             font=font_small).grid(row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['heater_mode_var'], bg=sub_state_display_frame['bg'],
             fg="lightgreen", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1
    tk.Label(sub_state_display_frame, text="Vacuum:", bg=sub_state_display_frame['bg'], fg="#ccc",
             font=font_small).grid(row=s_row, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=variables['vacuum_psig_var'], bg=sub_state_display_frame['bg'],
             fg="#aaddff", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w")

    global_counters_frame = tk.LabelFrame(ancillary_frame, text="Position (Relative to Home)", bg="#2a2d3b",
                                          fg="#aaddff", font=("Segoe UI", 9, "bold"), bd=1, relief="groove", padx=5,
                                          pady=5)
    global_counters_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 5), ipady=3)
    global_counters_frame.grid_columnconfigure(1, weight=1)
    tk.Label(global_counters_frame, text="Machine (mm):", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=0, column=0, sticky="e", padx=2, pady=2);
    tk.Label(global_counters_frame, textvariable=variables['machine_steps_var'], bg=global_counters_frame['bg'],
             fg="#00bfff", font=font_value).grid(row=0, column=1, sticky="w", padx=2, pady=2)
    tk.Label(global_counters_frame, text="Cartridge (mm):", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=1, column=0, sticky="e", padx=2, pady=2);
    tk.Label(global_counters_frame, textvariable=variables['cartridge_steps_var'], bg=global_counters_frame['bg'],
             fg="yellow", font=font_value).grid(row=1, column=1, sticky="w", padx=2, pady=2)

    create_feed_controls(ancillary_frame, send_injector_cmd, ui_elements, variables)

    return {}