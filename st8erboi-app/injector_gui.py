import tkinter as tk
from tkinter import ttk
import math

MOTOR_STEPS_PER_REV = 800


def create_injector_tab(notebook, send_injector_cmd):
    """
    Creates and populates the 'Injector' tab with all its specific controls and displays.
    """
    injector_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(injector_tab, text='  Injector  ')

    # --- Styles (specific to this tab if needed, but defined globally in main is fine) ---
    font_small = ('Segoe UI', 9)
    font_label = ('Segoe UI', 9)
    font_value = ('Segoe UI', 9, 'bold')
    font_motor_disp = ('Segoe UI', 9)

    # --- All StringVars for the Injector UI ---
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

    jog_steps_var = tk.StringVar(value="800");
    jog_velocity_var = tk.StringVar(value="800");
    jog_acceleration_var = tk.StringVar(value="5000");
    jog_torque_percent_var = tk.StringVar(value="30")
    homing_stroke_len_var = tk.StringVar(value="500");
    homing_rapid_vel_var = tk.StringVar(value="20");
    homing_touch_vel_var = tk.StringVar(value="1");
    homing_acceleration_var = tk.StringVar(value="50");
    homing_retract_dist_var = tk.StringVar(value="20");
    homing_torque_percent_var = tk.StringVar(value="5")
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

    ui_elements = {}

    # --- Helper functions for this tab ---
    def update_ml_per_rev(*args):
        try:
            dia1_mm = float(feed_cyl1_dia_var.get());
            dia2_mm = float(feed_cyl2_dia_var.get());
            pitch_mm_per_rev = float(feed_ballscrew_pitch_var.get())
            if dia1_mm <= 0 or dia2_mm <= 0 or pitch_mm_per_rev <= 0: feed_ml_per_rev_var.set(
                "Invalid dims"); feed_steps_per_ml_var.set(0.0); return
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
            feed_ml_per_rev_var.set("Input Error"); feed_steps_per_ml_var.set(0.0)

    feed_cyl1_dia_var.trace_add('write', update_ml_per_rev);
    feed_cyl2_dia_var.trace_add('write', update_ml_per_rev);
    feed_ballscrew_pitch_var.trace_add('write', update_ml_per_rev)

    def update_inject_time(*args):
        try:
            amount_ml = float(inject_amount_ml_var.get()); speed_ml_s = float(inject_speed_ml_s_var.get())
        except (ValueError, Exception):
            inject_time_var.set("Input Error"); return
        if speed_ml_s > 0:
            inject_time_var.set(f"{amount_ml / speed_ml_s:.2f} s")
        else:
            inject_time_var.set("N/A (Speed=0)")

    inject_amount_ml_var.trace_add('write', update_inject_time);
    inject_speed_ml_s_var.trace_add('write', update_inject_time)

    def update_purge_time(*args):
        try:
            amount_ml = float(purge_amount_ml_var.get()); speed_ml_s = float(purge_speed_ml_s_var.get())
        except (ValueError, Exception):
            purge_time_var.set("Input Error"); return
        if speed_ml_s > 0:
            purge_time_var.set(f"{amount_ml / speed_ml_s:.2f} s")
        else:
            purge_time_var.set("N/A (Speed=0)")

    purge_amount_ml_var.trace_add('write', update_purge_time);
    purge_speed_ml_s_var.trace_add('write', update_purge_time)

    def update_jog_button_state(current_main_state):
        names = ['jog_m1_plus', 'jog_m1_minus', 'jog_m2_plus', 'jog_m2_minus', 'jog_both_plus', 'jog_both_minus',
                 'jog_m2_pinch_plus', 'jog_m2_pinch_minus']
        desired = tk.NORMAL if current_main_state == "JOG_MODE" else tk.DISABLED
        for n in names:
            if n in ui_elements and ui_elements[n].winfo_exists(): ui_elements[n].config(state=desired)

    # --- Main Layout within the Injector Tab ---
    content_frame = tk.Frame(injector_tab, bg="#21232b");
    content_frame.pack(expand=True, fill=tk.BOTH, padx=0)
    left_column_frame = tk.Frame(content_frame, bg="#21232b");
    left_column_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, padx=(10, 5))

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
    tk.Label(global_counters_frame, text="Machine (M0) from Home:", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=0, column=0, sticky="e", padx=2, pady=2);
    tk.Label(global_counters_frame, textvariable=machine_steps_var, bg=global_counters_frame['bg'], fg="#00bfff",
             font=font_value).grid(row=0, column=1, sticky="w", padx=2, pady=2)
    tk.Label(global_counters_frame, text="Cartridge (M1) from Home:", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=1, column=0, sticky="e", padx=2, pady=2);
    tk.Label(global_counters_frame, textvariable=cartridge_steps_var, bg=global_counters_frame['bg'], fg="yellow",
             font=font_value).grid(row=1, column=1, sticky="w", padx=2, pady=2)

    controls_area_frame = tk.Frame(left_column_frame, bg="#21232b");
    controls_area_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
    modes_frame = tk.LabelFrame(controls_area_frame, text="Modes", bg="#34374b", fg="#0f8",
                                font=("Segoe UI", 11, "bold"), bd=2, relief="ridge", padx=5, pady=5);
    modes_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, pady=(0, 0), padx=(0, 5))
    ui_elements['standby_mode_btn'] = ttk.Button(modes_frame, text="Standby", style='Blue.TButton',
                                                 command=lambda: send_injector_cmd("STANDBY_MODE"));
    ui_elements['standby_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['test_mode_btn'] = ttk.Button(modes_frame, text="Test Mode", style='Orange.TButton',
                                              command=lambda: send_injector_cmd("TEST_MODE"));
    ui_elements['test_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['jog_mode_btn'] = ttk.Button(modes_frame, text="Jog Mode", style='Gray.TButton',
                                             command=lambda: send_injector_cmd("JOG_MODE"));
    ui_elements['jog_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['homing_mode_btn'] = ttk.Button(modes_frame, text="Homing Mode", style='Purple.TButton',
                                                command=lambda: send_injector_cmd("HOMING_MODE"));
    ui_elements['homing_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['feed_mode_btn'] = ttk.Button(modes_frame, text="Feed Mode", style='Cyan.TButton',
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

    bright_green, dim_green = "#21ba45", "#198734";
    bright_red, dim_red = "#db2828", "#932020"

    ui_elements['enable_btn'] = tk.Button(injector_enable_frame, text="Enable", fg="white", width=10,
                                          command=lambda: send_injector_cmd("ENABLE"), font=("Segoe UI", 10, "bold"));
    ui_elements['disable_btn'] = tk.Button(injector_enable_frame, text="Disable", fg="white", width=10,
                                           command=lambda: send_injector_cmd("DISABLE"), font=("Segoe UI", 10, "bold"))

    def update_injector_enable_buttons(*args):
        is_en = enable_disable_var.get() == "Enabled"
        ui_elements['enable_btn'].config(relief="sunken" if is_en else "raised",
                                         bg=bright_green if is_en else dim_green)
        ui_elements['disable_btn'].config(relief="sunken" if not is_en else "raised",
                                          bg=bright_red if not is_en else dim_red)

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
        is_en = enable_disable_pinch_var.get() == "Enabled"
        ui_elements['enable_pinch_btn'].config(relief="sunken" if is_en else "raised",
                                               bg=bright_green if is_en else dim_green)
        ui_elements['disable_pinch_btn'].config(relief="sunken" if not is_en else "raised",
                                                bg=bright_red if not is_en else dim_red)

    ui_elements['enable_pinch_btn'].pack(side=tk.LEFT, padx=(0, 1), expand=True, fill=tk.X);
    ui_elements['disable_pinch_btn'].pack(side=tk.LEFT, padx=(1, 0), expand=True, fill=tk.X);
    enable_disable_pinch_var.trace_add('write', update_pinch_enable_buttons);

    update_injector_enable_buttons()
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
    tk.Label(m1_display_section, text="Abs Pos:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
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
    tk.Label(m2_display_section, text="Abs Pos:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
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
    tk.Label(m3_display_section, text="Abs Pos:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=3, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=position_cmd3_var, bg=m3_display_section['bg'], fg="#ff8888",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Homed:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=4, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=pinch_homed_var, bg=m3_display_section['bg'], fg="lightgreen",
             font=font_motor_disp).grid(row=4, column=1, sticky="ew", padx=2)

    # This dictionary is returned so main.py can access these elements
    return {
        'main_state_var': main_state_var,
        # ... add all other injector vars and widgets here
    }