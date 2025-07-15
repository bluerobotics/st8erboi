import tkinter as tk
from tkinter import ttk
import math

# Import the new sub-modules for each control frame
from injector_jog_gui import create_jog_controls
from injector_homing_gui import create_homing_controls
from injector_feed_gui import create_feed_controls

MOTOR_STEPS_PER_REV = 800


def create_torque_bar(parent, label_text, bar_color):
    """Helper function to create a single torque bar indicator."""
    frame = tk.Frame(parent, bg=parent['bg'])

    label = tk.Label(frame, text=label_text, bg=parent['bg'], fg='white', font=('Segoe UI', 8))
    label.pack(pady=(0, 2))

    canvas = tk.Canvas(frame, width=40, height=80, bg='#1b1e2b', highlightthickness=0)
    canvas.pack()

    canvas.create_rectangle(10, 10, 30, 70, fill='#333', outline='')
    bar_item = canvas.create_rectangle(10, 70, 30, 70, fill=bar_color, outline='')
    text_item = canvas.create_text(20, 40, text="0%", fill='white', font=('Segoe UI', 9, 'bold'))

    return frame, canvas, bar_item, text_item


def create_injector_tab(notebook, send_injector_cmd, shared_gui_refs):
    injector_tab = tk.Frame(notebook, bg="#21232b")
    notebook.add(injector_tab, text='  Injector  ')

    # --- Style and Font Definitions ---
    font_small = ('Segoe UI', 8)
    font_label = ('Segoe UI', 8)
    font_value = ('Segoe UI', 8, 'bold')
    font_motor_disp = ('Segoe UI', 8)

    # --- Tkinter Variable Definitions ---
    variables = {
        'main_state_var': tk.StringVar(value="UNKNOWN"),
        'homing_state_var': tk.StringVar(value="---"),
        'homing_phase_var': tk.StringVar(value="---"),
        'feed_state_var': tk.StringVar(value="IDLE"),
        'error_state_var': tk.StringVar(value="---"),
        'machine_steps_var': tk.StringVar(value="0"),
        'cartridge_steps_var': tk.StringVar(value="0"),
        'enable_disable_var': tk.StringVar(value="Disabled"),
        'enable_disable_pinch_var': tk.StringVar(value="Disabled"),
        'motor_state1': tk.StringVar(value="N/A"),
        'enabled_state1': tk.StringVar(value="N/A"),
        'torque_value1': tk.StringVar(value="0.0"),
        'position_cmd1_var': tk.StringVar(value="0"),
        'motor_state2': tk.StringVar(value="N/A"),
        'enabled_state2': tk.StringVar(value="N/A"),
        'torque_value2': tk.StringVar(value="0.0"),
        'position_cmd2_var': tk.StringVar(value="0"),
        'motor_state3': tk.StringVar(value="N/A"),
        'enabled_state3': tk.StringVar(value="N/A"),
        'torque_value3': tk.StringVar(value="0.0"),
        'position_cmd3_var': tk.StringVar(value="0"),
        'pinch_homed_var': tk.StringVar(value="N/A"),
        'temp_c_var': tk.StringVar(value="--- °C"),
        'vacuum_state_var': tk.StringVar(value="Off"),
        'vacuum_valve_state_var': tk.StringVar(value="Off"),
        'heater_mode_var': tk.StringVar(value="OFF"),
        'vacuum_psig_var': tk.StringVar(value="--- PSIG"),
        'pid_setpoint_var': tk.StringVar(value="70.0"),
        'pid_kp_var': tk.StringVar(value="22.2"),
        'pid_ki_var': tk.StringVar(value="1.08"),
        'pid_kd_var': tk.StringVar(value="114.0"),
        'pid_output_var': tk.StringVar(value="0.0%"),
        'jog_dist_mm_var': tk.StringVar(value="10.0"),
        'jog_velocity_var': tk.StringVar(value="15.0"),
        'jog_acceleration_var': tk.StringVar(value="50.0"),
        'jog_torque_percent_var': tk.StringVar(value="30"),
        'jog_pinch_degrees_var': tk.StringVar(value="90.0"),
        'jog_pinch_velocity_sps_var': tk.StringVar(value="800"),
        'jog_pinch_accel_sps2_var': tk.StringVar(value="5000"),
        'homing_stroke_len_var': tk.StringVar(value="500"),
        'homing_rapid_vel_var': tk.StringVar(value="20"),
        'homing_touch_vel_var': tk.StringVar(value="1"),
        'homing_acceleration_var': tk.StringVar(value="50"),
        'homing_retract_dist_var': tk.StringVar(value="20"),
        'homing_torque_percent_var': tk.StringVar(value="5"),
        'feed_cyl1_dia_var': tk.StringVar(value="75.0"),
        'feed_cyl2_dia_var': tk.StringVar(value="33.0"),
        'feed_ballscrew_pitch_var': tk.StringVar(value="5.0"),
        'feed_ml_per_rev_var': tk.StringVar(value="N/A ml/rev"),
        'feed_steps_per_ml_var': tk.DoubleVar(value=0.0),
        'feed_acceleration_var': tk.StringVar(value="5000"),
        'feed_torque_percent_var': tk.StringVar(value="50"),
        'cartridge_retract_offset_mm_var': tk.StringVar(value="50.0"),
        'inject_amount_ml_var': tk.StringVar(value="10.0"),
        'inject_speed_ml_s_var': tk.StringVar(value="0.1"),
        'inject_time_var': tk.StringVar(value="N/A s"),
        'inject_dispensed_ml_var': tk.StringVar(value="0.00 ml"),
        'purge_amount_ml_var': tk.StringVar(value="0.5"),
        'purge_speed_ml_s_var': tk.StringVar(value="0.5"),
        'purge_time_var': tk.StringVar(value="N/A s"),
        'purge_dispensed_ml_var': tk.StringVar(value="0.00 ml"),
        'set_torque_offset_val_var': tk.StringVar(value="-2.4")
    }

    ui_elements = {}
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

        target_frame_key = None
        if new_main_state_from_firmware == "JOG_MODE":
            target_frame_key = 'jog_controls_frame'
        elif new_main_state_from_firmware == "HOMING_MODE":
            target_frame_key = 'homing_controls_frame'
        elif new_main_state_from_firmware == "FEED_MODE":
            target_frame_key = 'feed_controls_frame'

        if target_frame_key != current_active_mode_frame:
            for fk in ['jog_controls_frame', 'homing_controls_frame', 'feed_controls_frame']:
                if fk in ui_elements and ui_elements[fk].winfo_exists():
                    ui_elements[fk].pack_forget()

            if target_frame_key and target_frame_key in ui_elements:
                ui_elements[target_frame_key].pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

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

    variables['main_state_var'].trace_add('write', lambda *args: update_state(variables['main_state_var'].get()))
    variables['feed_state_var'].trace_add('write',
                                          lambda *args: update_feed_button_states(variables['feed_state_var'].get()))

    def update_ml_per_rev(*args):
        try:
            dia1_mm = float(variables['feed_cyl1_dia_var'].get());
            dia2_mm = float(variables['feed_cyl2_dia_var'].get());
            pitch_mm_per_rev = float(variables['feed_ballscrew_pitch_var'].get())
            if dia1_mm <= 0 or dia2_mm <= 0 or pitch_mm_per_rev <= 0:
                variables['feed_ml_per_rev_var'].set("Invalid dims");
                variables['feed_steps_per_ml_var'].set(0.0);
                return
            area1_mm2 = math.pi * (dia1_mm / 2) ** 2;
            area2_mm2 = math.pi * (dia2_mm / 2) ** 2;
            total_area_mm2 = area1_mm2 + area2_mm2
            vol_mm3_per_rev = total_area_mm2 * pitch_mm_per_rev;
            vol_ml_per_rev = vol_mm3_per_rev / 1000.0
            variables['feed_ml_per_rev_var'].set(f"{vol_ml_per_rev:.4f} ml/rev")
            if vol_ml_per_rev > 0:
                variables['feed_steps_per_ml_var'].set(round(MOTOR_STEPS_PER_REV / vol_ml_per_rev, 2))
            else:
                variables['feed_steps_per_ml_var'].set(0.0)
        except (ValueError, Exception):
            variables['feed_ml_per_rev_var'].set("Input Error");
            variables['feed_steps_per_ml_var'].set(0.0)

    for var_key in ['feed_cyl1_dia_var', 'feed_cyl2_dia_var', 'feed_ballscrew_pitch_var']:
        variables[var_key].trace_add('write', update_ml_per_rev)

    def update_inject_time(*args):
        try:
            amount_ml = float(variables['inject_amount_ml_var'].get());
            speed_ml_s = float(variables['inject_speed_ml_s_var'].get())
        except (ValueError, Exception):
            variables['inject_time_var'].set("Input Error");
            return
        if speed_ml_s > 0:
            variables['inject_time_var'].set(f"{amount_ml / speed_ml_s:.2f} s")
        else:
            variables['inject_time_var'].set("N/A (Speed=0)")

    for var_key in ['inject_amount_ml_var', 'inject_speed_ml_s_var']:
        variables[var_key].trace_add('write', update_inject_time)

    def update_purge_time(*args):
        try:
            amount_ml = float(variables['purge_amount_ml_var'].get());
            speed_ml_s = float(variables['purge_speed_ml_s_var'].get())
        except (ValueError, Exception):
            variables['purge_time_var'].set("Input Error");
            return
        if speed_ml_s > 0:
            variables['purge_time_var'].set(f"{amount_ml / speed_ml_s:.2f} s")
        else:
            variables['purge_time_var'].set("N/A (Speed=0)")

    for var_key in ['purge_amount_ml_var', 'purge_speed_ml_s_var']:
        variables[var_key].trace_add('write', update_purge_time)

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
             fg="#aaddff", font=font_small).grid(row=s_row, column=1, padx=(0, 5), pady=1, sticky="w");
    s_row += 1

    global_counters_frame = tk.LabelFrame(left_column_frame, text="Position Relative to Home", bg="#2a2d3b",
                                          fg="#aaddff", font=("Segoe UI", 9, "bold"), bd=1, relief="groove", padx=5,
                                          pady=5)
    global_counters_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(0, 5), ipady=3)
    global_counters_frame.grid_columnconfigure(1, weight=1)
    tk.Label(global_counters_frame, text="Machine (mm):", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=0, column=0, sticky="e", padx=2, pady=2)
    tk.Label(global_counters_frame, textvariable=variables['machine_steps_var'], bg=global_counters_frame['bg'],
             fg="#00bfff", font=font_value).grid(row=0, column=1, sticky="w", padx=2, pady=2)
    tk.Label(global_counters_frame, text="Cartridge (mm):", bg=global_counters_frame['bg'], fg="white",
             font=font_label).grid(row=1, column=0, sticky="e", padx=2, pady=2)
    tk.Label(global_counters_frame, textvariable=variables['cartridge_steps_var'], bg=global_counters_frame['bg'],
             fg="yellow", font=font_value).grid(row=1, column=1, sticky="w", padx=2, pady=2)

    torque_bar_frame = tk.LabelFrame(left_column_frame, text="Injector Torque", bg="#2a2d3b", fg="white",
                                     font=("Segoe UI", 9, "bold"))
    torque_bar_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 5), padx=0)

    bar_frame1, bar_canvas1, bar_item1, bar_text1 = create_torque_bar(torque_bar_frame, "Motor 0", "#00bfff")
    bar_frame2, bar_canvas2, bar_item2, bar_text2 = create_torque_bar(torque_bar_frame, "Motor 1", "yellow")
    bar_frame3, bar_canvas3, bar_item3, bar_text3 = create_torque_bar(torque_bar_frame, "Pinch M2", "#ff8888")

    bar_frame1.pack(side=tk.TOP, expand=True, fill=tk.X, padx=5, pady=(2, 3))
    bar_frame2.pack(side=tk.TOP, expand=True, fill=tk.X, padx=5, pady=3)
    bar_frame3.pack(side=tk.TOP, expand=True, fill=tk.X, padx=5, pady=(3, 2))

    ui_elements['injector_torque_canvas1'] = bar_canvas1
    ui_elements['injector_torque_bar1'] = bar_item1
    ui_elements['injector_torque_text1'] = bar_text1
    ui_elements['injector_torque_canvas2'] = bar_canvas2
    ui_elements['injector_torque_bar2'] = bar_item2
    ui_elements['injector_torque_text2'] = bar_text2
    ui_elements['injector_torque_canvas3'] = bar_canvas3
    ui_elements['injector_torque_bar3'] = bar_item3
    ui_elements['injector_torque_text3'] = bar_text3

    controls_area_frame = tk.Frame(top_content_frame, bg="#21232b");
    controls_area_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    modes_frame = tk.LabelFrame(controls_area_frame, text="Modes", bg="#34374b", fg="#0f8",
                                font=("Segoe UI", 10, "bold"), bd=2, relief="ridge", padx=5, pady=5);
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

    pid_frame = tk.LabelFrame(right_of_modes_area, text="Heater PID Control", bg="#2a2d3b", fg="#aaddff",
                              font=("Segoe UI", 9, "bold"), padx=10, pady=5)
    pid_frame.pack(fill=tk.X, pady=(5, 5))
    pid_frame.grid_columnconfigure((1, 3, 5), weight=1)

    tk.Label(pid_frame, text="Kp:", bg=pid_frame['bg'], fg='white', font=font_small).grid(row=0, column=0, sticky='e')
    kp_entry = ttk.Entry(pid_frame, textvariable=variables['pid_kp_var'], width=6, font=font_small)
    kp_entry.grid(row=0, column=1, sticky='ew', padx=(2, 10))
    ui_elements['pid_kp_entry'] = kp_entry

    tk.Label(pid_frame, text="Ki:", bg=pid_frame['bg'], fg='white', font=font_small).grid(row=0, column=2, sticky='e')
    ki_entry = ttk.Entry(pid_frame, textvariable=variables['pid_ki_var'], width=6, font=font_small)
    ki_entry.grid(row=0, column=3, sticky='ew', padx=(2, 10))
    ui_elements['pid_ki_entry'] = ki_entry

    tk.Label(pid_frame, text="Kd:", bg=pid_frame['bg'], fg='white', font=font_small).grid(row=0, column=4, sticky='e')
    kd_entry = ttk.Entry(pid_frame, textvariable=variables['pid_kd_var'], width=6, font=font_small)
    kd_entry.grid(row=0, column=5, sticky='ew', padx=2)
    ui_elements['pid_kd_entry'] = kd_entry

    ttk.Button(pid_frame, text="Set Gains", command=lambda: send_injector_cmd(
        f"SET_HEATER_GAINS {variables['pid_kp_var'].get()} {variables['pid_ki_var'].get()} {variables['pid_kd_var'].get()}")).grid(
        row=0, column=6, padx=5)

    tk.Label(pid_frame, text="Setpoint (°C):", bg=pid_frame['bg'], fg='white', font=font_small).grid(row=1, column=0,
                                                                                                     columnspan=2,
                                                                                                     sticky='e',
                                                                                                     pady=(5, 0))
    setpoint_entry = ttk.Entry(pid_frame, textvariable=variables['pid_setpoint_var'], width=6, font=font_small)
    setpoint_entry.grid(row=1, column=2, columnspan=2, sticky='ew', padx=(2, 10), pady=(5, 0))
    ui_elements['pid_setpoint_entry'] = setpoint_entry
    ttk.Button(pid_frame, text="Set Temp",
               command=lambda: send_injector_cmd(f"SET_HEATER_SETPOINT {variables['pid_setpoint_var'].get()}")).grid(
        row=1, column=4, columnspan=2, padx=5, pady=(5, 0))

    tk.Label(pid_frame, text="PID Output:", bg=pid_frame['bg'], fg='white', font=font_small).grid(row=2, column=0,
                                                                                                  columnspan=2,
                                                                                                  sticky='e',
                                                                                                  pady=(5, 0))
    tk.Label(pid_frame, textvariable=variables['pid_output_var'], bg=pid_frame['bg'], fg='orange',
             font=font_small).grid(row=2, column=2, columnspan=2, sticky='w', padx=2, pady=(5, 0))

    pid_btn_frame = tk.Frame(pid_frame, bg=pid_frame['bg'])
    pid_btn_frame.grid(row=3, column=0, columnspan=7, pady=5)

    ttk.Button(pid_btn_frame, text="PID ON", style="Green.TButton",
               command=lambda: send_injector_cmd("HEATER_PID_ON")).pack(side=tk.LEFT, padx=5)
    ttk.Button(pid_btn_frame, text="PID OFF", style="Red.TButton",
               command=lambda: send_injector_cmd("HEATER_PID_OFF")).pack(side=tk.LEFT, padx=5)
    ttk.Button(pid_btn_frame, text="Manual ON", command=lambda: send_injector_cmd("HEATER_ON")).pack(side=tk.LEFT,
                                                                                                     padx=5)
    ttk.Button(pid_btn_frame, text="Manual OFF", command=lambda: send_injector_cmd("HEATER_OFF")).pack(side=tk.LEFT,
                                                                                                       padx=5)

    vac_frame = tk.LabelFrame(right_of_modes_area, text="Vacuum Pump Control", bg="#2a2d3b", fg="#aaddff",
                              font=("Segoe UI", 9, "bold"), padx=10, pady=5)
    vac_frame.pack(fill=tk.X, pady=5)
    tk.Label(vac_frame, text="Vacuum Pump:", bg=vac_frame['bg'], fg='white', font=font_small).pack(side=tk.LEFT)
    ttk.Button(vac_frame, text="ON", style="Green.TButton", command=lambda: send_injector_cmd("VACUUM_ON")).pack(
        side=tk.LEFT, padx=5)
    ttk.Button(vac_frame, text="OFF", style="Red.TButton", command=lambda: send_injector_cmd("VACUUM_OFF")).pack(
        side=tk.LEFT, padx=5)
    tk.Label(vac_frame, text="Status:", bg=vac_frame['bg'], fg='white', font=font_small).pack(side=tk.LEFT,
                                                                                              padx=(20, 5))
    tk.Label(vac_frame, textvariable=variables['vacuum_state_var'], bg=vac_frame['bg'], fg='lightgreen',
             font=font_small).pack(side=tk.LEFT)

    vac_valve_frame = tk.LabelFrame(right_of_modes_area, text="Vacuum Valve Control", bg="#2a2d3b", fg="#aaddff",
                                    font=("Segoe UI", 9, "bold"), padx=10, pady=5)
    vac_valve_frame.pack(fill=tk.X, pady=(0, 5))
    tk.Label(vac_valve_frame, text="Vacuum Valve:", bg=vac_valve_frame['bg'], fg='white', font=font_small).pack(
        side=tk.LEFT)
    ttk.Button(vac_valve_frame, text="ON", style="Green.TButton",
               command=lambda: send_injector_cmd("VACUUM_VALVE_ON")).pack(side=tk.LEFT, padx=5)
    ttk.Button(vac_valve_frame, text="OFF", style="Red.TButton",
               command=lambda: send_injector_cmd("VACUUM_VALVE_OFF")).pack(side=tk.LEFT, padx=5)
    tk.Label(vac_valve_frame, text="Status:", bg=vac_valve_frame['bg'], fg='white', font=font_small).pack(side=tk.LEFT,
                                                                                                          padx=(20, 5))
    tk.Label(vac_valve_frame, textvariable=variables['vacuum_valve_state_var'], bg=vac_valve_frame['bg'],
             fg='lightgreen', font=font_small).pack(side=tk.LEFT)

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
                                          command=lambda: send_injector_cmd("ENABLE"), font=("Segoe UI", 9, "bold"));
    ui_elements['disable_btn'] = tk.Button(injector_enable_frame, text="Disable", fg="white", width=10,
                                           command=lambda: send_injector_cmd("DISABLE"), font=("Segoe UI", 9, "bold"))

    def update_injector_enable_buttons(*args):
        is_en = variables['enable_disable_var'].get() == "Enabled";
        ui_elements['enable_btn'].config(relief="sunken" if is_en else "raised",
                                         bg=bright_green if is_en else dim_green);
        ui_elements['disable_btn'].config(relief="sunken" if not is_en else "raised",
                                          bg=bright_red if not is_en else dim_red)

    ui_elements['enable_btn'].pack(side=tk.LEFT, padx=(0, 1), expand=True, fill=tk.X);
    ui_elements['disable_btn'].pack(side=tk.LEFT, padx=(1, 0), expand=True, fill=tk.X);
    variables['enable_disable_var'].trace_add('write', update_injector_enable_buttons);
    ui_elements['enable_pinch_btn'] = tk.Button(pinch_enable_frame, text="Enable", fg="white", width=10,
                                                command=lambda: send_injector_cmd("ENABLE_PINCH"),
                                                font=("Segoe UI", 9, "bold"));
    ui_elements['disable_pinch_btn'] = tk.Button(pinch_enable_frame, text="Disable", fg="white", width=10,
                                                 command=lambda: send_injector_cmd("DISABLE_PINCH"),
                                                 font=("Segoe UI", 9, "bold"))

    def update_pinch_enable_buttons(*args):
        is_en = variables['enable_disable_pinch_var'].get() == "Enabled";
        ui_elements['enable_pinch_btn'].config(relief="sunken" if is_en else "raised",
                                               bg=bright_green if is_en else dim_green);
        ui_elements['disable_pinch_btn'].config(relief="sunken" if not is_en else "raised",
                                                bg=bright_red if not is_en else dim_red)

    ui_elements['enable_pinch_btn'].pack(side=tk.LEFT, padx=(0, 1), expand=True, fill=tk.X);
    ui_elements['disable_pinch_btn'].pack(side=tk.LEFT, padx=(1, 0), expand=True, fill=tk.X);
    variables['enable_disable_pinch_var'].trace_add('write', update_pinch_enable_buttons);
    update_injector_enable_buttons();
    update_pinch_enable_buttons()

    always_on_motor_info_frame = tk.Frame(right_of_modes_area, bg="#21232b");
    always_on_motor_info_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 0))
    m1_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 0 (Injector)", bg="#1b2432",
                                       fg="#00bfff", font=("Segoe UI", 9, "bold"), bd=1, relief="ridge", padx=5,
                                       pady=2);
    m1_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 2));
    m1_display_section.grid_columnconfigure(1, weight=1)
    tk.Label(m1_display_section, text="HLFB:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=0, column=0, sticky="w");
    ui_elements['motor_status_label1'] = tk.Label(m1_display_section, textvariable=variables['motor_state1'],
                                                  bg=m1_display_section['bg'], fg="#00bfff", font=font_motor_disp);
    ui_elements['motor_status_label1'].grid(row=0, column=1, sticky="ew", padx=2)
    tk.Label(m1_display_section, text="Enabled:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    ui_elements['enabled_status_label1'] = tk.Label(m1_display_section, textvariable=variables['enabled_state1'],
                                                    bg=m1_display_section['bg'], fg="#00ff88", font=font_motor_disp);
    ui_elements['enabled_status_label1'].grid(row=1, column=1, sticky="ew", padx=2)
    tk.Label(m1_display_section, text="Torque:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    tk.Label(m1_display_section, textvariable=variables['torque_value1'], bg=m1_display_section['bg'], fg="#00bfff",
             font=font_motor_disp).grid(row=2, column=1, sticky="ew", padx=2)
    tk.Label(m1_display_section, text="Abs Pos (mm):", bg=m1_display_section['bg'], fg="white",
             font=font_motor_disp).grid(row=3, column=0, sticky="w");
    tk.Label(m1_display_section, textvariable=variables['position_cmd1_var'], bg=m1_display_section['bg'], fg="#00bfff",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    m2_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 1 (Injector)", bg="#2d253a", fg="yellow",
                                       font=("Segoe UI", 9, "bold"), bd=1, relief="ridge", padx=5, pady=2);
    m2_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 2));
    m2_display_section.grid_columnconfigure(1, weight=1)
    tk.Label(m2_display_section, text="HLFB:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=0, column=0, sticky="w");
    ui_elements['motor_status_label2'] = tk.Label(m2_display_section, textvariable=variables['motor_state2'],
                                                  bg=m2_display_section['bg'], fg="yellow", font=font_motor_disp);
    ui_elements['motor_status_label2'].grid(row=0, column=1, sticky="ew", padx=2)
    tk.Label(m2_display_section, text="Enabled:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    ui_elements['enabled_status_label2'] = tk.Label(m2_display_section, textvariable=variables['enabled_state2'],
                                                    bg=m2_display_section['bg'], fg="#00ff88", font=font_motor_disp);
    ui_elements['enabled_status_label2'].grid(row=1, column=1, sticky="ew", padx=2)
    tk.Label(m2_display_section, text="Torque:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    tk.Label(m2_display_section, textvariable=variables['torque_value2'], bg=m2_display_section['bg'], fg="yellow",
             font=font_motor_disp).grid(row=2, column=1, sticky="ew", padx=2)
    tk.Label(m2_display_section, text="Abs Pos (mm):", bg=m2_display_section['bg'], fg="white",
             font=font_motor_disp).grid(row=3, column=0, sticky="w");
    tk.Label(m2_display_section, textvariable=variables['position_cmd2_var'], bg=m2_display_section['bg'], fg="yellow",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    m3_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 2 (Pinch)", bg="#341c1c", fg="#ff8888",
                                       font=("Segoe UI", 9, "bold"), bd=1, relief="ridge", padx=5, pady=2);
    m3_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 0));
    m3_display_section.grid_columnconfigure(1, weight=1)
    tk.Label(m3_display_section, text="HLFB:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=0, column=0, sticky="w");
    ui_elements['motor_status_label3'] = tk.Label(m3_display_section, textvariable=variables['motor_state3'],
                                                  bg=m3_display_section['bg'], fg="#ff8888", font=font_motor_disp);
    ui_elements['motor_status_label3'].grid(row=0, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Enabled:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=1, column=0, sticky="w");
    ui_elements['enabled_status_label3'] = tk.Label(m3_display_section, textvariable=variables['enabled_state3'],
                                                    bg=m3_display_section['bg'], fg="#00ff88", font=font_motor_disp);
    ui_elements['enabled_status_label3'].grid(row=1, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Torque:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=2, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=variables['torque_value3'], bg=m3_display_section['bg'], fg="#ff8888",
             font=font_motor_disp).grid(row=2, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Abs Pos (mm):", bg=m3_display_section['bg'], fg="white",
             font=font_motor_disp).grid(row=3, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=variables['position_cmd3_var'], bg=m3_display_section['bg'], fg="#ff8888",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    tk.Label(m3_display_section, text="Homed:", bg=m3_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=4, column=0, sticky="w");
    tk.Label(m3_display_section, textvariable=variables['pinch_homed_var'], bg=m3_display_section['bg'],
             fg="lightgreen", font=font_motor_disp).grid(row=4, column=1, sticky="ew", padx=2)

    contextual_display_master_frame = tk.Frame(right_of_modes_area, bg="#21232b");
    contextual_display_master_frame.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

    create_jog_controls(contextual_display_master_frame, send_injector_cmd, ui_elements, variables)
    create_homing_controls(contextual_display_master_frame, send_injector_cmd, ui_elements, variables)
    create_feed_controls(contextual_display_master_frame, send_injector_cmd, ui_elements, variables)

    settings_controls_frame = tk.LabelFrame(right_of_modes_area, text="Global Settings", bg="#303030", fg="#DDD",
                                            font=("Segoe UI", 9, "bold"), bd=2, relief="ridge");
    settings_controls_frame.pack(fill=tk.X, expand=False, padx=5, pady=(10, 5))
    settings_controls_frame.grid_columnconfigure(1, weight=1);
    s_row = 0
    tk.Label(settings_controls_frame, text="Torque Offset:", bg=settings_controls_frame['bg'], fg='white',
             font=font_small).grid(row=s_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(settings_controls_frame, textvariable=variables['set_torque_offset_val_var'], width=10,
              font=font_small).grid(row=s_row, column=1, sticky="ew", padx=2, pady=2);
    ttk.Button(settings_controls_frame, text="Set Offset", style='Small.TButton', command=lambda: send_injector_cmd(
        f"SET_INJECTOR_TORQUE_OFFSET {variables['set_torque_offset_val_var'].get()}")).grid(row=s_row, column=2,
                                                                                            padx=(5, 2), pady=2)

    update_ml_per_rev()
    update_inject_time()
    update_purge_time()

    final_return_dict = {
        'update_state': update_state,
        'update_feed_buttons': update_feed_button_states,
    }
    final_return_dict.update(variables)
    final_return_dict.update(ui_elements)

    return final_return_dict