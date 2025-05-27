import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import math

MOTOR_STEPS_PER_REV = 800


def build_gui(send_udp, discover):
    root = tk.Tk()
    root.title("st8erboi app")
    root.configure(bg="#21232b")

    style = ttk.Style()
    style.theme_use('clam')
    style.configure('.', background="#21232b", foreground="#ffffff", fieldbackground="#34374b",
                    selectbackground="#21232b")
    style.configure('TButton', padding=6, font=('Segoe UI', 10, 'bold'), relief=tk.RAISED)
    style.configure('Small.TButton', padding=3, font=('Segoe UI', 9))

    style.configure('Jog.TButton', padding=[0, 6], font=('Segoe UI', 10, 'bold'))
    style.map('Jog.TButton',
              background=[('active', '#999999'), ('!disabled', '#666666'), ('disabled', '#444444')],
              foreground=[('!disabled', 'white'), ('disabled', '#888888')])

    style.configure('Blue.TButton', background='#0069d9', foreground='white');
    style.map('Blue.TButton', background=[('active', '#3399ff')])
    style.configure('Orange.TButton', background='#f2711c', foreground='white');
    style.map('Orange.TButton', background=[('active', '#ff8000')])
    style.configure('Gray.TButton', background='#767676', foreground='white');
    style.map('Gray.TButton', background=[('active', '#bbbbbb')])
    style.configure('Purple.TButton', background='#582A72', foreground='white');
    style.map('Purple.TButton', background=[('active', '#7D3F9D')])
    style.configure('Cyan.TButton', background='#008080', foreground='white');
    style.map('Cyan.TButton', background=[('active', '#00AAAA')])

    style.configure('ActiveBlue.TButton', background='#3399ff', foreground='white', relief=tk.SUNKEN)
    style.configure('ActiveOrange.TButton', background='#ff8000', foreground='white', relief=tk.SUNKEN)
    style.configure('ActiveGray.TButton', background='#bbbbbb', foreground='white', relief=tk.SUNKEN)
    style.configure('ActivePurple.TButton', background='#7D3F9D', foreground='white', relief=tk.SUNKEN)
    style.configure('ActiveCyan.TButton', background='#00AAAA', foreground='white', relief=tk.SUNKEN)

    status_var = tk.StringVar(value="🔌 Not connected")
    main_state_var = tk.StringVar(value="Unknown")
    prominent_firmware_state_var = tk.StringVar(value="---")

    motor_state1 = tk.StringVar(value="N/A");
    enabled_state1 = tk.StringVar(value="N/A")
    torque_value1 = tk.StringVar(value="--- %");
    position_cmd1_var = tk.StringVar(value="0")
    commanded_steps_var = tk.StringVar(value="0")  # Shared for M1/M2 machine steps display
    motor_state2 = tk.StringVar(value="N/A");
    enabled_state2 = tk.StringVar(value="N/A")
    torque_value2 = tk.StringVar(value="--- %");
    position_cmd2_var = tk.StringVar(value="0")
    torque_history1, torque_history2, torque_times = [], [], []

    enable_disable_var = tk.StringVar(value="Enabled")
    jog_steps_var = tk.StringVar(value="50");
    jog_velocity_var = tk.StringVar(value="500");
    jog_acceleration_var = tk.StringVar(value="5000")
    homing_stroke_len_var = tk.StringVar(value="100");
    homing_rapid_vel_var = tk.StringVar(value="20")
    homing_touch_vel_var = tk.StringVar(value="5");
    homing_retract_dist_var = tk.StringVar(value="2")
    feed_syringe_diameter_var = tk.StringVar(value="33");
    feed_ballscrew_pitch_var = tk.StringVar(value="2")
    feed_ml_per_rev_var = tk.StringVar(value="N/A ml/rev");
    feed_steps_per_ml_var = tk.DoubleVar(value=0.0)
    feed_amount_ml_var = tk.StringVar(value="1.0");
    feed_speed_ml_s_var = tk.StringVar(value="0.1")
    purge_amount_ml_var = tk.StringVar(value="0.5");
    purge_speed_ml_s_var = tk.StringVar(value="0.5")

    ui_elements = {}

    fig, ax = plt.subplots(figsize=(7, 2.8), facecolor='#21232b')
    line1, = ax.plot([], [], color='#00bfff', label="Motor 1");
    line2, = ax.plot([], [], color='yellow', label="Motor 2")
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

    def update_status(text):
        status_var.set(text)

    def update_position_cmd(pos1, pos2):
        position_cmd1_var.set(str(pos1)); position_cmd2_var.set(str(pos2))

    def terminal_cb(msg):
        if 'terminal' in ui_elements and ui_elements['terminal'].winfo_exists(): ui_elements['terminal'].insert(tk.END,
                                                                                                                msg);
        ui_elements['terminal'].see(tk.END)

    def update_ml_per_rev(*args):
        try:
            diameter_mm = float(feed_syringe_diameter_var.get());
            pitch_mm_per_rev = float(feed_ballscrew_pitch_var.get())
            if diameter_mm <= 0 or pitch_mm_per_rev <= 0: feed_ml_per_rev_var.set(
                "Invalid dims"); feed_steps_per_ml_var.set(0.0); return
            area_mm2 = math.pi * (diameter_mm / 2) ** 2;
            vol_mm3_per_rev = area_mm2 * pitch_mm_per_rev;
            vol_ml_per_rev = vol_mm3_per_rev / 1000.0
            feed_ml_per_rev_var.set(f"{vol_ml_per_rev:.4f} ml/rev")
            if vol_ml_per_rev > 0:
                steps_ml = MOTOR_STEPS_PER_REV / vol_ml_per_rev; feed_steps_per_ml_var.set(round(steps_ml, 2))
            else:
                feed_steps_per_ml_var.set(0.0)
        except ValueError:
            feed_ml_per_rev_var.set("Error"); feed_steps_per_ml_var.set(0.0)
        except Exception:
            feed_ml_per_rev_var.set("Calc Error"); feed_steps_per_ml_var.set(0.0)

    feed_syringe_diameter_var.trace_add('write', update_ml_per_rev);
    feed_ballscrew_pitch_var.trace_add('write', update_ml_per_rev)

    def update_state(new_app_state_for_ui_logic):
        main_state_var.set(new_app_state_for_ui_logic)
        update_jog_button_state(new_app_state_for_ui_logic)
        active_map = {"Standby": ('standby_mode_btn', 'ActiveBlue.TButton', 'Blue.TButton'),
                      "Test Mode": ('test_mode_btn', 'ActiveOrange.TButton', 'Orange.TButton'),
                      "Jog": ('jog_mode_btn', 'ActiveGray.TButton', 'Gray.TButton'),
                      "Homing Mode": ('homing_mode_btn', 'ActivePurple.TButton', 'Purple.TButton'),
                      "Feed Mode": ('feed_mode_btn', 'ActiveCyan.TButton', 'Cyan.TButton')}
        for state, (btn_key, act_style, norm_style) in active_map.items():
            if btn_key in ui_elements and ui_elements[btn_key].winfo_exists(): ui_elements[btn_key].config(
                style=act_style if new_app_state_for_ui_logic == state else norm_style)

        context_frames = ['jog_controls_frame', 'homing_controls_frame', 'feed_controls_frame']
        for fk in context_frames:
            if fk in ui_elements and ui_elements[fk].winfo_exists(): ui_elements[fk].pack_forget()

        if new_app_state_for_ui_logic == "Jog" and 'jog_controls_frame' in ui_elements:
            ui_elements['jog_controls_frame'].pack(fill=tk.BOTH, expand=True)
        elif new_app_state_for_ui_logic == "Homing Mode" and 'homing_controls_frame' in ui_elements:
            ui_elements['homing_controls_frame'].pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        elif new_app_state_for_ui_logic == "Feed Mode" and 'feed_controls_frame' in ui_elements:
            ui_elements['feed_controls_frame'].pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

    def update_motor_status(motor, text, color="#ffffff"):
        lbl_k = f'motor_status_label{motor}';
        var = motor_state1 if motor == 1 else motor_state2;
        var.set(text)
        if lbl_k in ui_elements and ui_elements[lbl_k].winfo_exists(): ui_elements[lbl_k].config(fg=color)

    def update_enabled_status(motor, text, color="#ffffff"):
        lbl_k = f'enabled_status_label{motor}';
        var = enabled_state1 if motor == 1 else enabled_state2;
        var.set(text)
        if lbl_k in ui_elements and ui_elements[lbl_k].winfo_exists(): ui_elements[lbl_k].config(fg=color)

    def update_torque_plot(v1, v2, ts, h1, h2):
        torque_value1.set(f"{v1:.2f}%");
        torque_value2.set(f"{v2:.2f}%")
        if h1 and h2 and ts and 'canvas_widget' in ui_elements and ui_elements['canvas_widget'].winfo_exists():
            if not ts: return
            ax.set_xlim(ts[0], ts[-1]);
            line1.set_data(ts, h1);
            line2.set_data(ts, h2)
            min_y = min(min(h1, default=-5), min(h2, default=-5));
            max_y = max(max(h1, default=105), max(h2, default=105))
            cmin, cmax = ax.get_ylim();
            nmin = min(min_y - 10, cmin if len(ts) > 1 else -10);
            nmax = max(max_y + 10, cmax if len(ts) > 1 else 110)
            if nmax - nmin < 20: nmax = nmin + 20
            nmin = max(nmin, -10);
            nmax = min(nmax, 110)
            if nmin >= nmax - 10: nmin = nmax - 20
            ax.set_ylim(nmin, nmax);
            fig.canvas.draw_idle()

    def update_jog_button_state(state):
        names = ['jog_m1_plus', 'jog_m1_minus', 'jog_m2_plus', 'jog_m2_minus', 'jog_both_plus', 'jog_both_minus']
        desired = tk.NORMAL if state == "Jog" else tk.DISABLED
        for n in names:
            if n in ui_elements and ui_elements[n].winfo_exists(): ui_elements[n].config(state=desired)

    main_frame = tk.Frame(root, bg="#21232b");
    main_frame.pack(expand=True, fill=tk.BOTH)
    top_row_frame = tk.Frame(main_frame, bg="#21232b");
    top_row_frame.pack(fill=tk.X, pady=(5, 2))
    ui_elements['abort_btn'] = tk.Button(top_row_frame, text="🛑 ABORT", bg="#db2828", fg="white",
                                         font=("Segoe UI", 10, "bold"), command=lambda: send_udp("ABORT"),
                                         relief="raised", bd=3, padx=10, pady=5)
    ui_elements['abort_btn'].pack(side=tk.RIGHT, padx=10)
    tk.Label(top_row_frame, textvariable=status_var, bg="#21232b", fg="white", font=("Segoe UI", 12)).pack(side=tk.LEFT,
                                                                                                           padx=(10, 2))
    tk.Label(top_row_frame, text="State:", bg="#21232b", fg="#bbb", font=("Segoe UI", 10, "italic")).pack(side=tk.LEFT)
    tk.Label(top_row_frame, textvariable=main_state_var, bg="#21232b", fg="#0f8", font=("Segoe UI", 14, "bold")).pack(
        side=tk.LEFT, padx=(0, 15))

    content_frame = tk.Frame(main_frame, bg="#21232b");
    content_frame.pack(expand=True, fill=tk.BOTH, padx=0)
    left_column_frame = tk.Frame(content_frame, bg="#21232b");
    left_column_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, padx=(10, 5))

    ui_elements['prominent_firmware_state_var'] = prominent_firmware_state_var
    prominent_state_display_frame = tk.Frame(left_column_frame, bg="#444", bd=2, relief="sunken")
    prominent_state_display_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(0, 10), ipady=3)
    ui_elements['prominent_state_display_frame'] = prominent_state_display_frame
    ui_elements['prominent_state_label'] = tk.Label(prominent_state_display_frame,
                                                    textvariable=prominent_firmware_state_var,
                                                    font=("Segoe UI", 11, "bold"), fg="white",
                                                    bg=prominent_state_display_frame['bg'])
    ui_elements['prominent_state_label'].pack(expand=True, fill=tk.X, padx=10, pady=(2, 2))

    controls_area_frame = tk.Frame(left_column_frame, bg="#21232b");
    controls_area_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
    modes_frame = tk.LabelFrame(controls_area_frame, text="Modes", bg="#34374b", fg="#0f8",
                                font=("Segoe UI", 11, "bold"), bd=2, relief="ridge", padx=5, pady=5)
    modes_frame.pack(side=tk.LEFT, fill=tk.Y, expand=False, pady=(0, 0), padx=(0, 5))  # expand=False for height
    ui_elements['standby_mode_btn'] = ttk.Button(modes_frame, text="Standby", style='Blue.TButton',
                                                 command=lambda: send_udp("STANDBY"));
    ui_elements['standby_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['test_mode_btn'] = ttk.Button(modes_frame, text="Test Mode", style='Orange.TButton',
                                              command=lambda: send_udp("TEST_MODE"));
    ui_elements['test_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['jog_mode_btn'] = ttk.Button(modes_frame, text="Jog Mode", style='Gray.TButton',
                                             command=lambda: send_udp("JOG_MODE"));
    ui_elements['jog_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['homing_mode_btn'] = ttk.Button(modes_frame, text="Homing Mode", style='Purple.TButton',
                                                command=lambda: send_udp("HOMING_MODE"));
    ui_elements['homing_mode_btn'].pack(fill=tk.X, pady=3, padx=5)
    ui_elements['feed_mode_btn'] = ttk.Button(modes_frame, text="Feed Mode", style='Cyan.TButton',
                                              command=lambda: send_udp("FEED_MODE"));
    ui_elements['feed_mode_btn'].pack(fill=tk.X, pady=3, padx=5)

    right_of_modes_area = tk.Frame(controls_area_frame, bg="#21232b");
    right_of_modes_area.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    enable_disable_frame = tk.Frame(right_of_modes_area, bg="#21232b");
    enable_disable_frame.pack(fill=tk.X, pady=(0, 5), padx=0)
    button_container_frame = tk.Frame(enable_disable_frame, bg=enable_disable_frame['bg']);
    button_container_frame.pack()
    bright_green, dim_green = "#21ba45", "#198734";
    bright_red, dim_red = "#db2828", "#932020"

    def set_enabled():
        enable_disable_var.set("Enabled");send_udp("ENABLE")

    def set_disabled():
        enable_disable_var.set("Disabled");send_udp("DISABLE")

    ui_elements['enable_btn'] = tk.Button(button_container_frame, text="Enable", fg="white", width=12,
                                          command=set_enabled, font=("Segoe UI", 10, "bold"))
    ui_elements['disable_btn'] = tk.Button(button_container_frame, text="Disable", fg="white", width=12,
                                           command=set_disabled, font=("Segoe UI", 10, "bold"))

    def update_slider_appearance():
        is_en = enable_disable_var.get() == "Enabled"
        if 'enable_btn' in ui_elements and ui_elements['enable_btn'].winfo_exists(): ui_elements['enable_btn'].config(
            relief="sunken" if is_en else "raised", bg=bright_green if is_en else dim_green)
        if 'disable_btn' in ui_elements and ui_elements['disable_btn'].winfo_exists(): ui_elements[
            'disable_btn'].config(relief="sunken" if not is_en else "raised", bg=bright_red if not is_en else dim_red)

    ui_elements['enable_btn'].pack(side=tk.LEFT, padx=(0, 1));
    ui_elements['disable_btn'].pack(side=tk.LEFT, padx=(1, 0))
    enable_disable_var.trace_add('write', lambda *a: update_slider_appearance());
    update_slider_appearance()

    # Frame for always-on motor status displays
    always_on_motor_info_frame = tk.Frame(right_of_modes_area, bg="#21232b")
    always_on_motor_info_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 0))

    # Motor 1 Display Section (Always Visible)
    m1_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 1 Info", bg="#1b2432", fg="#00bfff",
                                       font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2)
    m1_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 2))
    m1_display_section.grid_columnconfigure(1, weight=1)
    font_motor_disp = ('Segoe UI', 9)
    tk.Label(m1_display_section, text="Status:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
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
    tk.Label(m1_display_section, text="Pos Cmd:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=3, column=0, sticky="w");
    tk.Label(m1_display_section, textvariable=position_cmd1_var, bg=m1_display_section['bg'], fg="#00bfff",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    tk.Label(m1_display_section, text="M. Steps:", bg=m1_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=4, column=0, sticky="w");
    tk.Label(m1_display_section, textvariable=commanded_steps_var, bg=m1_display_section['bg'], fg="#00bfff",
             font=font_motor_disp).grid(row=4, column=1, sticky="ew", padx=2)

    # Motor 2 Display Section (Always Visible)
    m2_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 2 Info", bg="#2d253a", fg="yellow",
                                       font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2)
    m2_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 0))
    m2_display_section.grid_columnconfigure(1, weight=1)
    tk.Label(m2_display_section, text="Status:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
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
    tk.Label(m2_display_section, text="Pos Cmd:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=3, column=0, sticky="w");
    tk.Label(m2_display_section, textvariable=position_cmd2_var, bg=m2_display_section['bg'], fg="yellow",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    tk.Label(m2_display_section, text="M. Steps:", bg=m2_display_section['bg'], fg="white", font=font_motor_disp).grid(
        row=4, column=0, sticky="w");
    tk.Label(m2_display_section, textvariable=commanded_steps_var, bg=m2_display_section['bg'], fg="yellow",
             font=font_motor_disp).grid(row=4, column=1, sticky="ew", padx=2)

    contextual_display_master_frame = tk.Frame(right_of_modes_area, bg="#21232b")
    contextual_display_master_frame.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

    # --- JOG CONTROLS FRAME ---
    jog_controls_frame = tk.Frame(contextual_display_master_frame,
                                  bg="#21232b")  # Master container for all jog related UI
    ui_elements['jog_controls_frame'] = jog_controls_frame

    jog_specific_controls_area = tk.Frame(jog_controls_frame, bg="#21232b")  # To hold M1/M2 jog inputs side-by-side
    jog_specific_controls_area.pack(fill=tk.X)

    # Motor 1 Jog-Specific Controls
    m1_jog_inputs_frame = tk.LabelFrame(jog_specific_controls_area, text="Motor 1 Jog", bg="#1b2432", fg="#00bfff",
                                        font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2)
    m1_jog_inputs_frame.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 2), pady=(5, 0))
    m1_jog_inputs_frame.grid_columnconfigure(1, weight=1);
    row_idx = 0;
    font_small = ('Segoe UI', 9)
    tk.Label(m1_jog_inputs_frame, text="Steps:", bg="#1b2432", fg="white", font=font_small).grid(row=row_idx, column=0,
                                                                                                 sticky="w", pady=1);
    ttk.Entry(m1_jog_inputs_frame, textvariable=jog_steps_var, width=8, font=font_small).grid(row=row_idx, column=1,
                                                                                              sticky="ew", pady=1);
    row_idx += 1
    tk.Label(m1_jog_inputs_frame, text="Vel(s/s):", bg="#1b2432", fg="white", font=font_small).grid(row=row_idx,
                                                                                                    column=0,
                                                                                                    sticky="w", pady=1);
    ttk.Entry(m1_jog_inputs_frame, textvariable=jog_velocity_var, width=8, font=font_small).grid(row=row_idx, column=1,
                                                                                                 sticky="ew", pady=1);
    row_idx += 1
    tk.Label(m1_jog_inputs_frame, text="Acc(s/s²):", bg="#1b2432", fg="white", font=font_small).grid(row=row_idx,
                                                                                                     column=0,
                                                                                                     sticky="w",
                                                                                                     pady=1);
    ttk.Entry(m1_jog_inputs_frame, textvariable=jog_acceleration_var, width=8, font=font_small).grid(row=row_idx,
                                                                                                     column=1,
                                                                                                     sticky="ew",
                                                                                                     pady=1);
    row_idx += 1
    jog_m1_btn_frame = tk.Frame(m1_jog_inputs_frame, bg=m1_jog_inputs_frame['bg']);
    jog_m1_btn_frame.grid(row=row_idx, column=0, columnspan=2, sticky="ew", pady=2)
    ui_elements['jog_m1_plus'] = ttk.Button(jog_m1_btn_frame, text="▲ M1", style="Jog.TButton",
                                            command=lambda: send_udp(
                                                f"JOG {jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} 0 0 0"));
    ui_elements['jog_m1_plus'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 1))
    ui_elements['jog_m1_minus'] = ttk.Button(jog_m1_btn_frame, text="▼ M1", style="Jog.TButton",
                                             command=lambda: send_udp(
                                                 f"JOG -{jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} 0 0 0"));
    ui_elements['jog_m1_minus'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(1, 0))

    # Motor 2 Jog-Specific Controls
    m2_jog_inputs_frame = tk.LabelFrame(jog_specific_controls_area, text="Motor 2 Jog", bg="#2d253a", fg="yellow",
                                        font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2)
    m2_jog_inputs_frame.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 0), pady=(5, 0))
    m2_jog_inputs_frame.grid_columnconfigure(1, weight=1);
    row_idx = 0
    tk.Label(m2_jog_inputs_frame, text="Steps:", bg="#2d253a", fg="white", font=font_small).grid(row=row_idx, column=0,
                                                                                                 sticky="w", pady=1);
    ttk.Entry(m2_jog_inputs_frame, textvariable=jog_steps_var, width=8, font=font_small).grid(row=row_idx, column=1,
                                                                                              sticky="ew", pady=1);
    row_idx += 1
    tk.Label(m2_jog_inputs_frame, text="Vel(s/s):", bg="#2d253a", fg="white", font=font_small).grid(row=row_idx,
                                                                                                    column=0,
                                                                                                    sticky="w", pady=1);
    ttk.Entry(m2_jog_inputs_frame, textvariable=jog_velocity_var, width=8, font=font_small).grid(row=row_idx, column=1,
                                                                                                 sticky="ew", pady=1);
    row_idx += 1
    tk.Label(m2_jog_inputs_frame, text="Acc(s/s²):", bg="#2d253a", fg="white", font=font_small).grid(row=row_idx,
                                                                                                     column=0,
                                                                                                     sticky="w",
                                                                                                     pady=1);
    ttk.Entry(m2_jog_inputs_frame, textvariable=jog_acceleration_var, width=8, font=font_small).grid(row=row_idx,
                                                                                                     column=1,
                                                                                                     sticky="ew",
                                                                                                     pady=1);
    row_idx += 1
    jog_m2_btn_frame = tk.Frame(m2_jog_inputs_frame, bg=m2_jog_inputs_frame['bg']);
    jog_m2_btn_frame.grid(row=row_idx, column=0, columnspan=2, sticky="ew", pady=2)
    ui_elements['jog_m2_plus'] = ttk.Button(jog_m2_btn_frame, text="▲ M2", style="Jog.TButton",
                                            command=lambda: send_udp(
                                                f"JOG 0 0 0 {jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_m2_plus'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 1))
    ui_elements['jog_m2_minus'] = ttk.Button(jog_m2_btn_frame, text="▼ M2", style="Jog.TButton",
                                             command=lambda: send_udp(
                                                 f"JOG 0 0 0 -{jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_m2_minus'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(1, 0))

    # Sync Jog section (within jog_controls_frame)
    sync_jog_frame = tk.LabelFrame(jog_controls_frame, text="Synchronized Jog", bg="#333", fg="#0f8",
                                   font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2)
    sync_jog_frame.pack(fill=tk.X, expand=False, pady=(5, 0))
    sync_jog_frame.grid_columnconfigure(1, weight=1);
    sync_jog_frame.grid_columnconfigure(3, weight=1);
    row_idx_sync = 0
    tk.Label(sync_jog_frame, text="Steps:", bg="#333", fg="white", font=font_small).grid(row=row_idx_sync, column=0,
                                                                                         sticky="w", pady=1);
    ttk.Entry(sync_jog_frame, textvariable=jog_steps_var, width=10, font=font_small).grid(row=row_idx_sync, column=1,
                                                                                          sticky="ew", padx=(5, 10),
                                                                                          pady=1)
    tk.Label(sync_jog_frame, text="Jog Both:", bg="#333", fg="white", font=font_small).grid(row=row_idx_sync, column=2,
                                                                                            sticky="e", padx=(10, 5),
                                                                                            pady=1)
    jog_both_plus_frame = tk.Frame(sync_jog_frame, bg=sync_jog_frame['bg']);
    jog_both_plus_frame.grid(row=row_idx_sync, column=3, sticky="ew", pady=(0, 1), padx=(0, 5));
    ui_elements['jog_both_plus'] = ttk.Button(jog_both_plus_frame, text="▲ Both", style="Jog.TButton",
                                              command=lambda: send_udp(
                                                  f"JOG {jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} {jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_both_plus'].pack(fill=tk.X, expand=True, padx=10);
    row_idx_sync += 1  # Fill X, expand, reduced padx
    tk.Label(sync_jog_frame, text="Vel(s/s):", bg="#333", fg="white", font=font_small).grid(row=row_idx_sync, column=0,
                                                                                            sticky="w", pady=1);
    ttk.Entry(sync_jog_frame, textvariable=jog_velocity_var, width=10, font=font_small).grid(row=row_idx_sync, column=1,
                                                                                             sticky="ew", padx=(5, 10),
                                                                                             pady=1)
    jog_both_minus_frame = tk.Frame(sync_jog_frame, bg=sync_jog_frame['bg']);
    jog_both_minus_frame.grid(row=row_idx_sync, column=3, sticky="ew", pady=(1, 0), padx=(0, 5));
    ui_elements['jog_both_minus'] = ttk.Button(jog_both_minus_frame, text="▼ Both", style="Jog.TButton",
                                               command=lambda: send_udp(
                                                   f"JOG -{jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()} -{jog_steps_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_both_minus'].pack(fill=tk.X, expand=True, padx=10);
    row_idx_sync += 1  # Fill X, expand, reduced padx
    tk.Label(sync_jog_frame, text="Acc(s/s²):", bg="#333", fg="white", font=font_small).grid(row=row_idx_sync, column=0,
                                                                                             sticky="w", pady=1);
    ttk.Entry(sync_jog_frame, textvariable=jog_acceleration_var, width=10, font=font_small).grid(row=row_idx_sync,
                                                                                                 column=1, sticky="ew",
                                                                                                 padx=(5, 10), pady=1)

    # --- HOMING CONTROLS FRAME ---
    homing_controls_frame = tk.LabelFrame(contextual_display_master_frame, text="Homing Controls", bg="#2b1e34",
                                          fg="#D8BFD8", font=("Segoe UI", 11, "bold"), bd=2, relief="ridge")
    ui_elements['homing_controls_frame'] = homing_controls_frame
    homing_controls_frame.grid_columnconfigure(1, weight=1);
    h_row = 0;
    field_width = 10;
    font_small = ('Segoe UI', 9)
    tk.Label(homing_controls_frame, text="Stroke (mm):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_stroke_len_var, width=field_width, font=font_small).grid(
        row=h_row, column=1, sticky="ew", padx=2, pady=2);
    h_row += 1
    tk.Label(homing_controls_frame, text="Rapid Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_rapid_vel_var, width=field_width, font=font_small).grid(
        row=h_row, column=1, sticky="ew", padx=2, pady=2);
    h_row += 1
    tk.Label(homing_controls_frame, text="Touch Vel (mm/s):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_touch_vel_var, width=field_width, font=font_small).grid(
        row=h_row, column=1, sticky="ew", padx=2, pady=2);
    h_row += 1
    tk.Label(homing_controls_frame, text="Retract (mm):", bg=homing_controls_frame['bg'], fg='white',
             font=font_small).grid(row=h_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(homing_controls_frame, textvariable=homing_retract_dist_var, width=field_width, font=font_small).grid(
        row=h_row, column=1, sticky="ew", padx=2, pady=2);
    h_row += 1
    home_btn_frame = tk.Frame(homing_controls_frame, bg=homing_controls_frame['bg']);
    home_btn_frame.grid(row=h_row, column=0, columnspan=2, pady=5, sticky="ew")  # sticky ew
    ttk.Button(home_btn_frame, text="Home M1", style='Small.TButton', command=lambda: send_udp(
        f"HOME_EXECUTE 0 {homing_stroke_len_var.get()} {homing_rapid_vel_var.get()} {homing_touch_vel_var.get()} {homing_retract_dist_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(home_btn_frame, text="Home M2", style='Small.TButton', command=lambda: send_udp(
        f"HOME_EXECUTE 1 {homing_stroke_len_var.get()} {homing_rapid_vel_var.get()} {homing_touch_vel_var.get()} {homing_retract_dist_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(home_btn_frame, text="Home Both", style='Small.TButton', command=lambda: send_udp(
        f"HOME_EXECUTE 2 {homing_stroke_len_var.get()} {homing_rapid_vel_var.get()} {homing_touch_vel_var.get()} {homing_retract_dist_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)

    # --- FEED CONTROLS FRAME ---
    feed_controls_frame = tk.LabelFrame(contextual_display_master_frame, text="Feed Controls", bg="#1e3434",
                                        fg="#AFEEEE", font=("Segoe UI", 11, "bold"), bd=2, relief="ridge")
    ui_elements['feed_controls_frame'] = feed_controls_frame
    feed_controls_frame.grid_columnconfigure(1, weight=1);
    feed_controls_frame.grid_columnconfigure(3, weight=1)
    f_row = 0;
    field_width_feed = 8
    tk.Label(feed_controls_frame, text="Syringe Dia (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Combobox(feed_controls_frame, textvariable=feed_syringe_diameter_var, values=["33", "75"],
                 width=field_width_feed - 2, font=font_small, state="readonly").grid(row=f_row, column=1, sticky="ew",
                                                                                     padx=2, pady=2)
    tk.Label(feed_controls_frame, text="Pitch (mm/rev):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_ballscrew_pitch_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Calc ml/rev:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=0, sticky="w", padx=2, pady=2)
    tk.Label(feed_controls_frame, textvariable=feed_ml_per_rev_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)
    tk.Label(feed_controls_frame, text="Calc Steps/ml:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    tk.Label(feed_controls_frame, textvariable=feed_steps_per_ml_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Feed Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_amount_ml_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2)
    tk.Label(feed_controls_frame, text="Feed Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_speed_ml_s_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    ttk.Button(feed_controls_frame, text="Feed Volume", style='Small.TButton', command=lambda: send_udp(
        f"FEED_EXECUTE {feed_amount_ml_var.get()} {feed_speed_ml_s_var.get()} {feed_steps_per_ml_var.get()}")).grid(
        row=f_row, column=0, columnspan=4, pady=3, sticky="ew", padx=20);
    f_row += 1
    tk.Label(feed_controls_frame, text="Purge Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=purge_amount_ml_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2)
    tk.Label(feed_controls_frame, text="Purge Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=purge_speed_ml_s_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    ttk.Button(feed_controls_frame, text="Purge Volume", style='Small.TButton', command=lambda: send_udp(
        f"PURGE_EXECUTE {purge_amount_ml_var.get()} {purge_speed_ml_s_var.get()} {feed_steps_per_ml_var.get()}")).grid(
        row=f_row, column=0, columnspan=4, pady=3, sticky="ew", padx=20)
    update_ml_per_rev()

    right_column_frame = tk.Frame(content_frame, bg="#21232b");
    right_column_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(5, 10))
    chart_frame = tk.Frame(right_column_frame, bg="#21232b");
    chart_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 5))
    canvas = FigureCanvasTkAgg(fig, master=chart_frame);
    ui_elements['canvas_widget'] = canvas.get_tk_widget();
    ui_elements['canvas_widget'].pack(side=tk.TOP, fill=tk.BOTH, expand=True)
    ui_elements['terminal'] = tk.Text(right_column_frame, height=8, bg="#1b1e2b", fg="#0f8", insertbackground="white",
                                      wrap="word", highlightbackground="#34374b", highlightthickness=1, bd=0,
                                      font=("Consolas", 10))
    ui_elements['terminal'].pack(fill=tk.X, expand=False, pady=(5, 0))

    ui_elements.update({'root': root, 'update_status': update_status, 'update_motor_status': update_motor_status,
                        'update_enabled_status': update_enabled_status,
                        'commanded_steps_var': commanded_steps_var, 'torque_history1': torque_history1,
                        'torque_history2': torque_history2, 'torque_times': torque_times,
                        'update_torque_plot': update_torque_plot, 'terminal_cb': terminal_cb,
                        'update_state': update_state, 'update_position_cmd': update_position_cmd,
                        'main_state_var': main_state_var})
    root.after(20, lambda: update_state(main_state_var.get()))
    return ui_elements