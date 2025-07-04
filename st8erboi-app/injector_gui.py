import tkinter as tk
from tkinter import ttk
import math
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

MOTOR_STEPS_PER_REV = 800


def create_injector_tab(notebook, send_injector_cmd, shared_gui_refs):
    injector_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(injector_tab, text='  Injector  ')

    # --- Style and Font Definitions ---
    font_small = ('Segoe UI', 9)
    font_label = ('Segoe UI', 9)
    font_value = ('Segoe UI', 9, 'bold')
    font_motor_disp = ('Segoe UI', 9)

    # --- Tkinter Variable Definitions ---
    main_state_var = tk.StringVar(value="UNKNOWN")
    homing_state_var = tk.StringVar(value="---")
    homing_phase_var = tk.StringVar(value="---")
    feed_state_var = tk.StringVar(value="IDLE")
    error_state_var = tk.StringVar(value="---")
    machine_steps_var = tk.StringVar(value="0")
    cartridge_steps_var = tk.StringVar(value="0")
    enable_disable_var = tk.StringVar(value="Disabled")
    enable_disable_pinch_var = tk.StringVar(value="Disabled")
    motor_state1 = tk.StringVar(value="N/A");
    enabled_state1 = tk.StringVar(value="N/A");
    torque_value1 = tk.StringVar(value="---");
    position_cmd1_var = tk.StringVar(value="0")
    motor_state2 = tk.StringVar(value="N/A");
    enabled_state2 = tk.StringVar(value="N/A");
    torque_value2 = tk.StringVar(value="---");
    position_cmd2_var = tk.StringVar(value="0")
    motor_state3 = tk.StringVar(value="N/A");
    enabled_state3 = tk.StringVar(value="N/A");
    torque_value3 = tk.StringVar(value="---");
    position_cmd3_var = tk.StringVar(value="0");
    pinch_homed_var = tk.StringVar(value="N/A")

    # Jogging Variables (Metric for M0/M1)
    jog_dist_mm_var = tk.StringVar(value="10.0")
    jog_velocity_var = tk.StringVar(value="15.0")
    jog_acceleration_var = tk.StringVar(value="50.0")
    jog_torque_percent_var = tk.StringVar(value="30")

    # Jogging Variables (Degrees and SPS for M2/Pinch)
    jog_pinch_degrees_var = tk.StringVar(value="90.0")
    jog_pinch_velocity_sps_var = tk.StringVar(value="800")
    jog_pinch_accel_sps2_var = tk.StringVar(value="5000")

    # Homing Variables
    homing_stroke_len_var = tk.StringVar(value="500");
    homing_rapid_vel_var = tk.StringVar(value="20");
    homing_touch_vel_var = tk.StringVar(value="1");
    homing_acceleration_var = tk.StringVar(value="50");
    homing_retract_dist_var = tk.StringVar(value="20");
    homing_torque_percent_var = tk.StringVar(value="5")

    # Feed/Dispense Variables
    feed_cyl1_dia_var = tk.StringVar(value="75.0");
    feed_cyl2_dia_var = tk.StringVar(value="33.0");
    feed_ballscrew_pitch_var = tk.StringVar(value="5.0");
    feed_ml_per_rev_var = tk.StringVar(value="N/A ml/rev");
    feed_steps_per_ml_var = tk.DoubleVar(value=0.0);
    feed_acceleration_var = tk.StringVar(value="5000");
    feed_torque_percent_var = tk.StringVar(value="50");
    cartridge_retract_offset_mm_var = tk.StringVar(value="50.0")
    inject_amount_ml_var = tk.StringVar(value="10.0");
    inject_speed_ml_s_var = tk.StringVar(value="0.1");
    inject_time_var = tk.StringVar(value="N/A s");
    inject_dispensed_ml_var = tk.StringVar(value="0.00 ml")
    purge_amount_ml_var = tk.StringVar(value="0.5");
    purge_speed_ml_s_var = tk.StringVar(value="0.5");
    purge_time_var = tk.StringVar(value="N/A s");
    purge_dispensed_ml_var = tk.StringVar(value="0.00 ml")
    set_torque_offset_val_var = tk.StringVar(value="-2.4")

    # --- GUI Update Functions ---
    ui_elements = {}

    # Variable to track the currently displayed contextual frame to prevent flickering
    current_active_mode_frame = None

    def update_state(new_main_state_from_firmware):
        nonlocal current_active_mode_frame
        update_jog_button_state(new_main_state_from_firmware)
        active_map = {"STANDBY_MODE": ('standby_mode_btn', 'ActiveBlue.TButton', 'Blue.TButton'),
                      "JOG_MODE": ('jog_mode_btn', 'ActiveGray.TButton', 'Gray.TButton'),
                      "HOMING_MODE": ('homing_mode_btn', 'ActivePurple.TButton', 'Purple.TButton'),
                      "FEED_MODE": ('feed_mode_btn', 'ActiveCyan.TButton', 'Cyan.TButton')}

        for fw_state, (btn_key, act_style, norm_style) in active_map.items():
            is_active = (new_main_state_from_firmware == fw_state)
            if btn_key in ui_elements and ui_elements[btn_key].winfo_exists():
                ui_elements[btn_key].config(style=act_style if is_active else norm_style)

        if new_main_state_from_firmware == "UNKNOWN":
            return

        # Determine which frame *should* be visible
        target_frame_key = None
        if new_main_state_from_firmware == "JOG_MODE":
            target_frame_key = 'jog_controls_frame'
        elif new_main_state_from_firmware == "HOMING_MODE":
            target_frame_key = 'homing_controls_frame'
        elif new_main_state_from_firmware == "FEED_MODE":
            target_frame_key = 'feed_controls_frame'

        # Only update the layout if the required frame has changed
        if target_frame_key != current_active_mode_frame:
            # Hide all contextual frames
            for fk in ['jog_controls_frame', 'homing_controls_frame', 'feed_controls_frame']:
                if fk in ui_elements and ui_elements[fk].winfo_exists():
                    ui_elements[fk].pack_forget()

            # Show the correct frame if one is needed
            if target_frame_key and target_frame_key in ui_elements:
                ui_elements[target_frame_key].pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

            # Update the tracker for the currently visible frame
            current_active_mode_frame = target_frame_key

    def update_feed_button_states(new_feed_state):
        is_inject_active = new_feed_state in ["FEED_INJECT_STARTING", "FEED_INJECT_ACTIVE", "FEED_INJECT_RESUMING"]
        is_purge_active = new_feed_state in ["FEED_PURGE_STARTING", "FEED_PURGE_ACTIVE", "FEED_PURGE_RESUMING"]
        is_paused = new_feed_state in ["FEED_INJECT_PAUSED", "FEED_PURGE_PAUSED"]
        is_idle = not (is_inject_active or is_purge_active or is_paused)

        if 'start_inject_btn' in ui_elements:
            ui_elements['start_inject_btn'].config(state=tk.NORMAL if is_idle else tk.DISABLED)
            ui_elements['pause_inject_btn'].config(state=tk.NORMAL if is_inject_active else tk.DISABLED)
            ui_elements['resume_inject_btn'].config(
                state=tk.NORMAL if new_feed_state == "FEED_INJECT_PAUSED" else tk.DISABLED)
            ui_elements['cancel_inject_btn'].config(
                state=tk.NORMAL if is_inject_active or new_feed_state == "FEED_INJECT_PAUSED" else tk.DISABLED)

        if 'start_purge_btn' in ui_elements:
            ui_elements['start_purge_btn'].config(state=tk.NORMAL if is_idle else tk.DISABLED)
            ui_elements['pause_purge_btn'].config(state=tk.NORMAL if is_purge_active else tk.DISABLED)
            ui_elements['resume_purge_btn'].config(
                state=tk.NORMAL if new_feed_state == "FEED_PURGE_PAUSED" else tk.DISABLED)
            ui_elements['cancel_purge_btn'].config(
                state=tk.NORMAL if is_purge_active or new_feed_state == "FEED_PURGE_PAUSED" else tk.DISABLED)

    # --- Traces to link variables to GUI update functions ---
    main_state_var.trace_add('write', lambda *args: update_state(main_state_var.get()))
    feed_state_var.trace_add('write', lambda *args: update_feed_button_states(feed_state_var.get()))

    # --- Calculation Functions ---
    def update_ml_per_rev(*args):
        try:
            dia1_mm = float(feed_cyl1_dia_var.get());
            dia2_mm = float(feed_cyl2_dia_var.get());
            pitch_mm_per_rev = float(feed_ballscrew_pitch_var.get())
            if dia1_mm <= 0 or dia2_mm <= 0 or pitch_mm_per_rev <= 0:
                feed_ml_per_rev_var.set("Invalid dims");
                feed_steps_per_ml_var.set(0.0);
                return
            area1_mm2 = math.pi * (dia1_mm / 2) ** 2;
            area2_mm2 = math.pi * (dia2_mm / 2) ** 2;
            total_area_mm2 = area1_mm2 + area2_mm2
            vol_mm3_per_rev = total_area_mm2 * pitch_mm_per_rev;
            vol_ml_per_rev = vol_mm3_per_rev / 1000.0
            feed_ml_per_rev_var.set(f"{vol_ml_per_rev:.4f} ml/rev")
            if vol_ml_per_rev > 0:
                feed_steps_per_ml_var.set(round(MOTOR_STEPS_PER_REV / vol_ml_per_rev, 2))
            else:
                feed_steps_per_ml_var.set(0.0)
        except (ValueError, Exception):
            feed_ml_per_rev_var.set("Input Error");
            feed_steps_per_ml_var.set(0.0)

    for var in [feed_cyl1_dia_var, feed_cyl2_dia_var, feed_ballscrew_pitch_var]: var.trace_add('write',
                                                                                               update_ml_per_rev)

    def update_inject_time(*args):
        try:
            amount_ml = float(inject_amount_ml_var.get());
            speed_ml_s = float(inject_speed_ml_s_var.get())
        except (ValueError, Exception):
            inject_time_var.set("Input Error");
            return
        if speed_ml_s > 0:
            inject_time_var.set(f"{amount_ml / speed_ml_s:.2f} s")
        else:
            inject_time_var.set("N/A (Speed=0)")

    for var in [inject_amount_ml_var, inject_speed_ml_s_var]: var.trace_add('write', update_inject_time)

    def update_purge_time(*args):
        try:
            amount_ml = float(purge_amount_ml_var.get());
            speed_ml_s = float(purge_speed_ml_s_var.get())
        except (ValueError, Exception):
            purge_time_var.set("Input Error");
            return
        if speed_ml_s > 0:
            purge_time_var.set(f"{amount_ml / speed_ml_s:.2f} s")
        else:
            purge_time_var.set("N/A (Speed=0)")

    for var in [purge_amount_ml_var, purge_speed_ml_s_var]: var.trace_add('write', update_purge_time)

    def update_jog_button_state(current_main_state):
        names = ['jog_m0_plus', 'jog_m0_minus', 'jog_m1_plus', 'jog_m1_minus', 'jog_both_plus', 'jog_both_minus',
                 'jog_m2_pinch_plus', 'jog_m2_pinch_minus']
        desired = tk.NORMAL if current_main_state == "JOG_MODE" else tk.DISABLED
        for n in names:
            if n in ui_elements and ui_elements[n].winfo_exists(): ui_elements[n].config(state=desired)

    # --- GUI Layout ---
    content_frame = tk.Frame(injector_tab, bg="#21232b");
    content_frame.pack(side=tk.TOP, expand=True, fill=tk.BOTH, padx=0)

    top_content_frame = tk.Frame(content_frame, bg="#21232b")
    top_content_frame.pack(side=tk.TOP, fill=tk.X)

    left_column_frame = tk.Frame(top_content_frame, bg="#21232b");
    left_column_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, padx=(10, 5), pady=(5, 0))

    sub_state_display_frame = tk.Frame(left_column_frame, bg="#2a2d3b", bd=1, relief="groove");
    sub_state_display_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 5), ipady=3);
    sub_state_display_frame.grid_columnconfigure(1, weight=1)
    tk.Label(sub_state_display_frame, text="Main:", bg=sub_state_display_frame['bg'], fg="#ccc", font=font_small).grid(
        row=0, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=main_state_var, bg=sub_state_display_frame['bg'], fg="white",
             font=font_small).grid(row=0, column=1, padx=(0, 5), pady=1, sticky="w")
    tk.Label(sub_state_display_frame, text="Homing:", bg=sub_state_display_frame['bg'], fg="#ccc",
             font=font_small).grid(row=1, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=homing_state_var, bg=sub_state_display_frame['bg'], fg="white",
             font=font_small).grid(row=1, column=1, padx=(0, 5), pady=1, sticky="w")
    tk.Label(sub_state_display_frame, text="H. Phase:", bg=sub_state_display_frame['bg'], fg="#ccc",
             font=font_small).grid(row=2, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=homing_phase_var, bg=sub_state_display_frame['bg'], fg="white",
             font=font_small).grid(row=2, column=1, padx=(0, 5), pady=1, sticky="w")
    tk.Label(sub_state_display_frame, text="Feed:", bg=sub_state_display_frame['bg'], fg="#ccc", font=font_small).grid(
        row=3, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=feed_state_var, bg=sub_state_display_frame['bg'], fg="white",
             font=font_small).grid(row=3, column=1, padx=(0, 5), pady=1, sticky="w")
    tk.Label(sub_state_display_frame, text="Error:", bg=sub_state_display_frame['bg'], fg="#ccc", font=font_small).grid(
        row=4, column=0, padx=(5, 2), pady=1, sticky="e");
    tk.Label(sub_state_display_frame, textvariable=error_state_var, bg=sub_state_display_frame['bg'], fg="orange red",
             font=font_small).grid(row=4, column=1, padx=(0, 5), pady=1, sticky="w")
    global_counters_frame = tk.LabelFrame(left_column_frame, text="Position Relative to Home", bg="#2a2d3b",
                                          fg="#aaddff", font=("Segoe UI", 10, "bold"), bd=1, relief="groove", padx=5,
                                          pady=5);
    global_counters_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(0, 5), ipady=3);
    global_counters_frame.grid_columnconfigure(1, weight=1)
    tk.Label(global_counters_frame, text="Machine (mm):", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=0, column=0, sticky="e", padx=2, pady=2);
    tk.Label(global_counters_frame, textvariable=machine_steps_var, bg=global_counters_frame['bg'], fg="#00bfff",
             font=font_value).grid(row=0, column=1, sticky="w", padx=2, pady=2)
    tk.Label(global_counters_frame, text="Cartridge (mm):", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=1, column=0, sticky="e", padx=2, pady=2);
    tk.Label(global_counters_frame, textvariable=cartridge_steps_var, bg=global_counters_frame['bg'], fg="yellow",
             font=font_value).grid(row=1, column=1, sticky="w", padx=2, pady=2)

    controls_area_frame = tk.Frame(top_content_frame, bg="#21232b");
    controls_area_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    modes_frame = tk.LabelFrame(controls_area_frame, text="Modes", bg="#34374b", fg="#0f8",
                                font=("Segoe UI", 11, "bold"), bd=2, relief="ridge", padx=5, pady=5);
    modes_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, pady=(0, 0), padx=(0, 5))
    ui_elements['standby_mode_btn'] = ttk.Button(modes_frame, text="Standby", style='Blue.TButton',
                                                 command=lambda: send_injector_cmd("STANDBY_MODE"));
    ui_elements['standby_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['jog_mode_btn'] = ttk.Button(modes_frame, text="Jog", style='Gray.TButton',
                                             command=lambda: send_injector_cmd("JOG_MODE"));
    ui_elements['jog_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['homing_mode_btn'] = ttk.Button(modes_frame, text="Homing", style='Purple.TButton',
                                                command=lambda: send_injector_cmd("HOMING_MODE"));
    ui_elements['homing_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['feed_mode_btn'] = ttk.Button(modes_frame, text="Feed", style='Cyan.TButton',
                                              command=lambda: send_injector_cmd("FEED_MODE"));
    ui_elements['feed_mode_btn'].pack(fill=tk.X, pady=3, padx=5)

    right_of_modes_area = tk.Frame(controls_area_frame, bg="#21232b");
    right_of_modes_area.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    enable_disable_master_frame = tk.Frame(right_of_modes_area, bg="#21232b")
    enable_disable_master_frame.pack(fill=tk.X, pady=(0, 5), padx=0)
    injector_enable_frame = tk.LabelFrame(enable_disable_master_frame, text="Injector Motors (0 & 1)", bg="#21232b",
                                          fg="white", font=font_label, padx=5, pady=2)
    injector_enable_frame.pack(side=tk.LEFT, expand=True, padx=(0, 5), fill=tk.X)
    pinch_enable_frame = tk.LabelFrame(enable_disable_master_frame, text="Pinch Motor (2)", bg="#21232b", fg="white",
                                       font=font_label, padx=5, pady=2)
    pinch_enable_frame.pack(side=tk.LEFT, expand=True, fill=tk.X)
    bright_green, dim_green, bright_red, dim_red = "#21ba45", "#198734", "#db2828", "#932020"
    ui_elements['enable_btn'] = tk.Button(injector_enable_frame, text="Enable", fg="white", width=10,
                                          command=lambda: send_injector_cmd("ENABLE"), font=("Segoe UI", 10, "bold"));
    ui_elements['disable_btn'] = tk.Button(injector_enable_frame, text="Disable", fg="white", width=10,
                                           command=lambda: send_injector_cmd("DISABLE"), font=("Segoe UI", 10, "bold"))

    def update_injector_enable_buttons(*args):
        is_en = enable_disable_var.get() == "Enabled";
        ui_elements['enable_btn'].config(
            relief="sunken" if is_en else "raised", bg=bright_green if is_en else dim_green);
        ui_elements[
            'disable_btn'].config(relief="sunken" if not is_en else "raised", bg=bright_red if not is_en else dim_red)

    ui_elements['enable_btn'].pack(side=tk.LEFT, padx=(0, 1), expand=True, fill=tk.X);
    ui_elements['disable_btn'].pack(side=tk.LEFT, padx=(1, 0), expand=True, fill=tk.X);
    enable_disable_var.trace_add('write', update_injector_enable_buttons);
    ui_elements['enable_pinch_btn'] = tk.Button(pinch_enable_frame, text="Enable", fg="white", width=10,
                                                command=lambda: send_injector_cmd("ENABLE_PINCH"),
                                                font=("Segoe UI", 10, "bold"));
    ui_elements['disable_pinch_btn'] = tk.Button(pinch_enable_frame, text="Disable", fg="white", width=10,
                                                 command=lambda: send_injector_cmd("DISABLE_PINCH"),
                                                 font=("Segoe UI", 10, "bold"))

    def update_pinch_enable_buttons(*args):
        is_en = enable_disable_pinch_var.get() == "Enabled";
        ui_elements['enable_pinch_btn'].config(
            relief="sunken" if is_en else "raised", bg=bright_green if is_en else dim_green);
        ui_elements[
            'disable_pinch_btn'].config(relief="sunken" if not is_en else "raised",
                                        bg=bright_red if not is_en else dim_red)

    ui_elements['enable_pinch_btn'].pack(side=tk.LEFT, padx=(0, 1), expand=True, fill=tk.X);
    ui_elements['disable_pinch_btn'].pack(side=tk.LEFT, padx=(1, 0), expand=True, fill=tk.X);
    enable_disable_pinch_var.trace_add('write', update_pinch_enable_buttons);
    update_injector_enable_buttons();
    update_pinch_enable_buttons()

    always_on_motor_info_frame = tk.Frame(right_of_modes_area, bg="#21232b");
    always_on_motor_info_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 0))
    m1_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 0 (Injector)", bg="#1b2432",
                                       fg="#00bfff", font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5,
                                       pady=2);
    m1_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 2));
    m1_display_section.grid_columnconfigure(1, weight=1)
    tk.Label(m1_display_section, text="HLFB:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=0, column=0, sticky="w");
    ui_elements['motor_status_label1'] = tk.Label(m1_display_section, textvariable=motor_state1,
                                                  bg=m1_display_section['bg'], fg="#00bfff", font=font_motor_disp);
    ui_elements['motor_status_label1'].grid(row=0, column=1, sticky="ew", padx=2)
    tk.Label(m1_display_section, text="Enabled:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    ui_elements['enabled_status_label1'] = tk.Label(m1_display_section, textvariable=enabled_state1,
                                                    bg=m1_display_section['bg'], fg="#00ff88", font=font_motor_disp);
    ui_elements['enabled_status_label1'].grid(row=1, column=1, sticky="ew", padx=2)
    tk.Label(m1_display_section, text="Torque:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    tk.Label(m1_display_section, textvariable=torque_value1, bg=m1_display_section['bg'], fg="#00bfff",
             font=font_motor_disp).grid(row=2, column=1, sticky="ew", padx=2)
    tk.Label(m1_display_section, text="Abs Pos (mm):", bg=m1_display_section['bg'], fg="white",
             font=font_motor_disp).grid(
        row=3, column=0, sticky="w");
    tk.Label(m1_display_section, textvariable=position_cmd1_var, bg=m1_display_section['bg'], fg="#00bfff",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    m2_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 1 (Injector)", bg="#2d253a", fg="yellow",
                                       font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2);
    m2_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 2));
    m2_display_section.grid_columnconfigure(1, weight=1)
    tk.Label(m2_display_section, text="HLFB:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=0, column=0, sticky="w");
    ui_elements['motor_status_label2'] = tk.Label(m2_display_section, textvariable=motor_state2,
                                                  bg=m2_display_section['bg'], fg="yellow", font=font_motor_disp);
    ui_elements['motor_status_label2'].grid(row=0, column=1, sticky="ew", padx=2)
    tk.Label(m2_display_section, text="Enabled:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    ui_elements['enabled_status_label2'] = tk.Label(m2_display_section, textvariable=enabled_state2,
                                                    bg=m2_display_section['bg'], fg="#00ff88", font=font_motor_disp);
    ui_elements['enabled_status_label2'].grid(row=1, column=1, sticky="ew", padx=2)
    tk.Label(m2_display_section, text="Torque:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    tk.Label(m2_display_section, textvariable=torque_value2, bg=m2_display_section['bg'], fg="yellow",
             font=font_motor_disp).grid(row=2, column=1, sticky="ew", padx=2)
    tk.Label(m2_display_section, text="Abs Pos (mm):", bg=m2_display_section['bg'], fg="white",
             font=font_motor_disp).grid(
        row=3, column=0, sticky="w");
    tk.Label(m2_display_section, textvariable=position_cmd2_var, bg=m2_display_section['bg'], fg="yellow",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    m3_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 2 (Pinch)", bg="#341c1c", fg="#ff8888",
                                       font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2);
    m3_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 0));
    m3_display_section.grid_columnconfigure(1, weight=1)
    tk.Label(m3_display_section, text="HLFB:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=0, column=0, sticky="w");
    ui_elements['motor_status_label3'] = tk.Label(m3_display_section, textvariable=motor_state3,
                                                  bg=m3_display_section['bg'], fg="#ff8888", font=font_motor_disp);
    ui_elements['motor_status_label3'].grid(row=0, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Enabled:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    ui_elements['enabled_status_label3'] = tk.Label(m3_display_section, textvariable=enabled_state3,
                                                    bg=m3_display_section['bg'], fg="#00ff88", font=font_motor_disp);
    ui_elements['enabled_status_label3'].grid(row=1, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Torque:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=torque_value3, bg=m3_display_section['bg'], fg="#ff8888",
             font=font_motor_disp).grid(row=2, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Abs Pos (mm):", bg=m3_display_section['bg'], fg="white",
             font=font_motor_disp).grid(
        row=3, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=position_cmd3_var, bg=m3_display_section['bg'], fg="#ff8888",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Homed:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=4, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=pinch_homed_var, bg=m3_display_section['bg'], fg="lightgreen",
             font=font_motor_disp).grid(row=4, column=1, sticky="ew", padx=2)

    contextual_display_master_frame = tk.Frame(right_of_modes_area, bg="#21232b");
    contextual_display_master_frame.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

    jog_controls_frame = tk.LabelFrame(contextual_display_master_frame, text="Jog Controls (Active in JOG_MODE)",
                                       bg="#21232b", fg="#0f8", font=("Segoe UI", 10, "bold"));
    ui_elements['jog_controls_frame'] = jog_controls_frame
    homing_controls_frame = tk.LabelFrame(contextual_display_master_frame,
                                          text="Homing Controls (Active in HOMING_MODE)", bg="#2b1e34", fg="#D8BFD8",
                                          font=("Segoe UI", 10, "bold"), bd=2, relief="ridge");
    ui_elements['homing_controls_frame'] = homing_controls_frame
    feed_controls_frame = tk.LabelFrame(contextual_display_master_frame, text="Feed Controls (Active in FEED_MODE)",
                                        bg="#1e3434", fg="#AFEEEE", font=("Segoe UI", 10, "bold"), bd=2,
                                        relief="ridge");
    ui_elements['feed_controls_frame'] = feed_controls_frame
    settings_controls_frame = tk.LabelFrame(right_of_modes_area, text="Global Settings", bg="#303030", fg="#DDD",
                                            font=("Segoe UI", 10, "bold"), bd=2, relief="ridge");
    ui_elements['settings_controls_frame'] = settings_controls_frame

    # --- Populate Jog Frame ---
    jog_params_frame = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg']);
    jog_params_frame.pack(fill=tk.X, pady=5, padx=5);
    jog_params_frame.grid_columnconfigure(1, weight=1);
    jog_params_frame.grid_columnconfigure(3, weight=1)
    tk.Label(jog_params_frame, text="Dist (mm):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                               column=0,
                                                                                                               sticky="w",
                                                                                                               pady=1,
                                                                                                               padx=2);
    ttk.Entry(jog_params_frame, textvariable=jog_dist_mm_var, width=8, font=font_small).grid(row=0, column=1,
                                                                                             sticky="ew",
                                                                                             pady=1, padx=(0, 10));
    tk.Label(jog_params_frame, text="Vel (mm/s):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                                column=2,
                                                                                                                sticky="w",
                                                                                                                pady=1,
                                                                                                                padx=2);
    ttk.Entry(jog_params_frame, textvariable=jog_velocity_var, width=8, font=font_small).grid(row=0, column=3,
                                                                                              sticky="ew", pady=1,
                                                                                              padx=(0, 10));
    tk.Label(jog_params_frame, text="Accel (mm/s²):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(
        row=1,
        column=0,
        sticky="w",
        pady=1,
        padx=2);
    ttk.Entry(jog_params_frame, textvariable=jog_acceleration_var, width=8, font=font_small).grid(row=1, column=1,
                                                                                                  sticky="ew", pady=1,
                                                                                                  padx=(0, 10));
    tk.Label(jog_params_frame, text="Torque (%):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=1,
                                                                                                                column=2,
                                                                                                                sticky="w",
                                                                                                                pady=1,
                                                                                                                padx=2);
    ttk.Entry(jog_params_frame, textvariable=jog_torque_percent_var, width=8, font=font_small).grid(row=1, column=3,
                                                                                                    sticky="ew", pady=1,
                                                                                                    padx=(0, 10))
    jog_buttons_area = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg']);
    jog_buttons_area.pack(fill=tk.X, pady=5)
    m0_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg']);
    m0_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2);
    ui_elements['jog_m0_plus'] = ttk.Button(m0_jog_frame, text="▲ M0", style="Jog.TButton", state=tk.DISABLED,
                                            command=lambda: send_injector_cmd(
                                                f"JOG_MOVE {jog_dist_mm_var.get()} 0 {jog_velocity_var.get()} {jog_acceleration_var.get()} {jog_torque_percent_var.get()}"));
    ui_elements['jog_m0_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2));
    ui_elements['jog_m0_minus'] = ttk.Button(m0_jog_frame, text="▼ M0", style="Jog.TButton", state=tk.DISABLED,
                                             command=lambda: send_injector_cmd(
                                                 f"JOG_MOVE -{jog_dist_mm_var.get()} 0 {jog_velocity_var.get()} {jog_acceleration_var.get()} {jog_torque_percent_var.get()}"));
    ui_elements['jog_m0_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)
    m1_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg']);
    m1_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2);
    ui_elements['jog_m1_plus'] = ttk.Button(m1_jog_frame, text="▲ M1", style="Jog.TButton", state=tk.DISABLED,
                                            command=lambda: send_injector_cmd(
                                                f"JOG_MOVE 0 {jog_dist_mm_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} {jog_torque_percent_var.get()}"));
    ui_elements['jog_m1_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2));
    ui_elements['jog_m1_minus'] = ttk.Button(m1_jog_frame, text="▼ M1", style="Jog.TButton", state=tk.DISABLED,
                                             command=lambda: send_injector_cmd(
                                                 f"JOG_MOVE 0 -{jog_dist_mm_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} {jog_torque_percent_var.get()}"));
    ui_elements['jog_m1_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)
    both_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg']);
    both_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2);
    ui_elements['jog_both_plus'] = ttk.Button(both_jog_frame, text="▲ Both", style="Jog.TButton", state=tk.DISABLED,
                                              command=lambda: send_injector_cmd(
                                                  f"JOG_MOVE {jog_dist_mm_var.get()} {jog_dist_mm_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} {jog_torque_percent_var.get()}"));
    ui_elements['jog_both_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2));
    ui_elements['jog_both_minus'] = ttk.Button(both_jog_frame, text="▼ Both", style="Jog.TButton", state=tk.DISABLED,
                                               command=lambda: send_injector_cmd(
                                                   f"JOG_MOVE -{jog_dist_mm_var.get()} -{jog_dist_mm_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} {jog_torque_percent_var.get()}"));
    ui_elements['jog_both_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)
    m2_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg']);
    m2_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=2);
    ui_elements['jog_m2_pinch_plus'] = ttk.Button(m2_jog_frame, text="▲ M2 (Pinch)", style="Jog.TButton",
                                                  state=tk.DISABLED, command=lambda: send_injector_cmd(
            f"PINCH_JOG_MOVE {jog_pinch_degrees_var.get()} {jog_torque_percent_var.get()} {jog_pinch_velocity_sps_var.get()} {jog_pinch_accel_sps2_var.get()}"));
    ui_elements['jog_m2_pinch_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2));
    ui_elements['jog_m2_pinch_minus'] = ttk.Button(m2_jog_frame, text="▼ M2 (Pinch)", style="Jog.TButton",
                                                   state=tk.DISABLED, command=lambda: send_injector_cmd(
            f"PINCH_JOG_MOVE -{jog_pinch_degrees_var.get()} {jog_torque_percent_var.get()} {jog_pinch_velocity_sps_var.get()} {jog_pinch_accel_sps2_var.get()}"));
    ui_elements['jog_m2_pinch_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)

    # --- Populate Homing Frame ---
    homing_controls_frame.grid_columnconfigure(1, weight=1);
    homing_controls_frame.grid_columnconfigure(3, weight=1);
    h_row = 0;
    field_width_homing = 10
    tk.Label(homing_controls_frame, text="Stroke (mm):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_stroke_len_var, width=field_width_homing,
              font=font_small).grid(row=h_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(homing_controls_frame, text="Rapid Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=2, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_rapid_vel_var, width=field_width_homing, font=font_small).grid(
        row=h_row, column=3, sticky="ew", padx=2, pady=2);
    h_row += 1
    tk.Label(homing_controls_frame, text="Touch Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_touch_vel_var, width=field_width_homing, font=font_small).grid(
        row=h_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(homing_controls_frame, text="Accel (mm/s²):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=2, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_acceleration_var, width=field_width_homing,
              font=font_small).grid(row=h_row, column=3, sticky="ew", padx=2, pady=2);
    h_row += 1
    tk.Label(homing_controls_frame, text="M.Retract (mm):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_retract_dist_var, width=field_width_homing,
              font=font_small).grid(row=h_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(homing_controls_frame, text="Torque (%):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=2, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_torque_percent_var, width=field_width_homing,
              font=font_small).grid(row=h_row, column=3, sticky="ew", padx=2, pady=2);
    h_row += 1
    home_btn_frame = tk.Frame(homing_controls_frame, bg=homing_controls_frame['bg']);
    home_btn_frame.grid(row=h_row, column=0, columnspan=4, pady=5, sticky="ew")
    ttk.Button(home_btn_frame, text="Execute Machine Home", style='Small.TButton', command=lambda: send_injector_cmd(
        f"MACHINE_HOME_MOVE {homing_stroke_len_var.get()} {homing_rapid_vel_var.get()} {homing_touch_vel_var.get()} {homing_acceleration_var.get()} {homing_retract_dist_var.get()} {homing_torque_percent_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(home_btn_frame, text="Execute Cartridge Home", style='Small.TButton', command=lambda: send_injector_cmd(
        f"CARTRIDGE_HOME_MOVE {homing_stroke_len_var.get()} {homing_rapid_vel_var.get()} {homing_touch_vel_var.get()} {homing_acceleration_var.get()} 0 {homing_torque_percent_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(home_btn_frame, text="Execute Pinch Home", style='Small.TButton',
               command=lambda: send_injector_cmd("PINCH_HOME_MOVE")).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)

    # --- Populate Feed Frame ---
    feed_controls_frame.grid_columnconfigure(1, weight=1);
    feed_controls_frame.grid_columnconfigure(3, weight=1);
    f_row = 0;
    field_width_feed = 8
    tk.Label(feed_controls_frame, text="Cyl 1 Dia (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=feed_cyl1_dia_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Cyl 2 Dia (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=feed_cyl2_dia_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Pitch (mm/rev):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=feed_ballscrew_pitch_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Calc ml/rev:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, textvariable=feed_ml_per_rev_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Calc Steps/ml:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, textvariable=feed_steps_per_ml_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Feed Accel (sps²):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=feed_acceleration_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Torque Limit (%):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=feed_torque_percent_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2);
    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew',
                                                                 pady=5);
    f_row += 1
    tk.Label(feed_controls_frame, text="Retract Offset (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, columnspan=2, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=cartridge_retract_offset_mm_var, width=field_width_feed,
              font=font_small).grid(row=f_row, column=2, sticky="ew", padx=2, pady=2);
    f_row += 1
    positioning_btn_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg']);
    positioning_btn_frame.grid(row=f_row, column=0, columnspan=4, pady=(5, 2), sticky="ew")
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Home", style='Green.TButton',
               command=lambda: send_injector_cmd("MOVE_TO_CARTRIDGE_HOME")).pack(side=tk.LEFT, fill=tk.X, expand=True,
                                                                                 padx=2)
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Retract", style='Green.TButton',
               command=lambda: send_injector_cmd(
                   f"MOVE_TO_CARTRIDGE_RETRACT {cartridge_retract_offset_mm_var.get()}")).pack(side=tk.LEFT, fill=tk.X,
                                                                                               expand=True, padx=2);
    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew',
                                                                 pady=5);
    f_row += 1
    tk.Label(feed_controls_frame, text="Inject Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=inject_amount_ml_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Inject Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=inject_speed_ml_s_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Est. Inject Time:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, textvariable=inject_time_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, textvariable=inject_dispensed_ml_var, bg=feed_controls_frame['bg'], fg='lightgreen',
             font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2);
    f_row += 1
    inject_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg']);
    inject_op_frame.grid(row=f_row, column=0, columnspan=4, pady=(2, 5), sticky="ew")
    ui_elements['start_inject_btn'] = ttk.Button(inject_op_frame, text="Start Inject", style='Start.TButton',
                                                 state='normal', command=lambda: send_injector_cmd(
            f"INJECT_MOVE {inject_amount_ml_var.get()} {inject_speed_ml_s_var.get()} {feed_acceleration_var.get()} {feed_steps_per_ml_var.get()} {feed_torque_percent_var.get()}"));
    ui_elements['start_inject_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(10, 2))
    ui_elements['pause_inject_btn'] = ttk.Button(inject_op_frame, text="Pause", style='Pause.TButton', state='disabled',
                                                 command=lambda: send_injector_cmd("PAUSE_INJECTION"));
    ui_elements['pause_inject_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ui_elements['resume_inject_btn'] = ttk.Button(inject_op_frame, text="Resume", style='Resume.TButton',
                                                  state='disabled',
                                                  command=lambda: send_injector_cmd("RESUME_INJECTION"));
    ui_elements['resume_inject_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ui_elements['cancel_inject_btn'] = ttk.Button(inject_op_frame, text="Cancel", style='Cancel.TButton',
                                                  state='disabled',
                                                  command=lambda: send_injector_cmd("CANCEL_INJECTION"));
    ui_elements['cancel_inject_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 10));
    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew',
                                                                 pady=5);
    f_row += 1
    tk.Label(feed_controls_frame, text="Purge Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=purge_amount_ml_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Purge Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2);
    ttk.Entry(feed_controls_frame, textvariable=purge_speed_ml_s_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Est. Purge Time:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, textvariable=purge_time_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, textvariable=purge_dispensed_ml_var, bg=feed_controls_frame['bg'], fg='lightgreen',
             font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2);
    f_row += 1
    purge_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg']);
    purge_op_frame.grid(row=f_row, column=0, columnspan=4, pady=(2, 5), sticky="ew")
    ui_elements['start_purge_btn'] = ttk.Button(purge_op_frame, text="Start Purge", style='Start.TButton',
                                                state='normal', command=lambda: send_injector_cmd(
            f"PURGE_MOVE {purge_amount_ml_var.get()} {purge_speed_ml_s_var.get()} {feed_acceleration_var.get()} {feed_steps_per_ml_var.get()} {feed_torque_percent_var.get()}"));
    ui_elements['start_purge_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(10, 2))
    ui_elements['pause_purge_btn'] = ttk.Button(purge_op_frame, text="Pause", style='Pause.TButton', state='disabled',
                                                command=lambda: send_injector_cmd("PAUSE_INJECTION"));
    ui_elements['pause_purge_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ui_elements['resume_purge_btn'] = ttk.Button(purge_op_frame, text="Resume", style='Resume.TButton',
                                                 state='disabled',
                                                 command=lambda: send_injector_cmd("RESUME_INJECTION"));
    ui_elements['resume_purge_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ui_elements['cancel_purge_btn'] = ttk.Button(purge_op_frame, text="Cancel", style='Cancel.TButton',
                                                 state='disabled',
                                                 command=lambda: send_injector_cmd("CANCEL_INJECTION"));
    ui_elements['cancel_purge_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 10))

    # --- Populate Settings Frame ---
    settings_controls_frame.grid_columnconfigure(1, weight=1);
    s_row = 0
    tk.Label(settings_controls_frame, text="Torque Offset:", bg=settings_controls_frame['bg'], fg='white',
             font=font_small).grid(row=s_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(settings_controls_frame, textvariable=set_torque_offset_val_var, width=10, font=font_small).grid(
        row=s_row, column=1, sticky="ew", padx=2, pady=2);
    ttk.Button(settings_controls_frame, text="Set Offset", style='Small.TButton',
               command=lambda: send_injector_cmd(f"SET_INJECTOR_TORQUE_OFFSET {set_torque_offset_val_var.get()}")).grid(
        row=s_row, column=2, padx=(5, 2), pady=2)

    settings_controls_frame.pack(fill=tk.X, expand=False, padx=5, pady=(10, 5))

    # --- Torque Plot ---
    plot_frame = tk.Frame(content_frame, bg="#21232b")
    plot_frame.pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=(10, 0))

    fig, ax = plt.subplots(figsize=(7, 2.5), facecolor='#21232b')
    line1, = ax.plot([], [], color='#00bfff', label="M0");
    line2, = ax.plot([], [], color='yellow', label="M1")
    line3, = ax.plot([], [], color='#ff8888', label="M2 (Pinch)")
    ax.set_title("Injector Torque", color='white')
    ax.set_facecolor('#1b1e2b');
    ax.tick_params(axis='x', colors='white');
    ax.tick_params(axis='y', colors='white')
    ax.spines['bottom'].set_color('white');
    ax.spines['left'].set_color('white');
    ax.spines['top'].set_visible(False);
    ax.spines['right'].set_visible(False)
    ax.set_ylabel("Torque (%)", color='white');
    ax.set_ylim(-10, 110);
    ax.legend(facecolor='#21232b', edgecolor='white', labelcolor='white')
    canvas = FigureCanvasTkAgg(fig, master=plot_frame)
    canvas_widget = canvas.get_tk_widget()
    canvas_widget.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    ui_elements['injector_torque_plot_canvas'] = canvas
    ui_elements['injector_torque_plot_ax'] = ax
    ui_elements['injector_torque_plot_lines'] = [line1, line2, line3]

    # --- Initial Calculations ---
    update_ml_per_rev()
    update_inject_time()
    update_purge_time()

    # --- Return Dictionary ---
    final_return_dict = {
        'update_state': update_state,
        'update_feed_buttons': update_feed_button_states,
        'main_state_var': main_state_var, 'homing_state_var': homing_state_var,
        'homing_phase_var': homing_phase_var, 'feed_state_var': feed_state_var, 'error_state_var': error_state_var,
        'machine_steps_var': machine_steps_var, 'cartridge_steps_var': cartridge_steps_var,
        'enable_disable_var': enable_disable_var, 'enable_disable_pinch_var': enable_disable_pinch_var,
        'motor_state1_var': motor_state1, 'enabled_state1_var': enabled_state1, 'torque_value1_var': torque_value1,
        'position_cmd1_var': position_cmd1_var,
        'motor_state2_var': motor_state2, 'enabled_state2_var': enabled_state2, 'torque_value2_var': torque_value2,
        'position_cmd2_var': position_cmd2_var,
        'motor_state3_var': motor_state3, 'enabled_state3_var': enabled_state3, 'torque_value3_var': torque_value3,
        'position_cmd3_var': position_cmd3_var, 'pinch_homed_var': pinch_homed_var,
        'inject_dispensed_ml_var': inject_dispensed_ml_var, 'purge_dispensed_ml_var': purge_dispensed_ml_var,
    }
    final_return_dict.update(ui_elements)
    return final_return_dict
