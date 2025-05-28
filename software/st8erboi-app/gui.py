import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import math
import time

MOTOR_STEPS_PER_REV = 800  # Consistent with firmware's pulsesPerRev


def build_gui(send_udp, discover):
    root = tk.Tk()
    root.title("st8erboi app v2")
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
    style.configure('Yellow.TButton', background='#fbbd08', foreground='black');  # For Clear Errors
    style.map('Yellow.TButton', background=[('active', '#ffe277')])
    style.configure('Orange.TButton', background='#f2711c', foreground='white');
    style.map('Orange.TButton', background=[('active', '#ff8000')])
    style.configure('Gray.TButton', background='#767676', foreground='white');
    style.map('Gray.TButton', background=[('active', '#bbbbbb')])
    style.configure('Purple.TButton', background='#582A72', foreground='white');
    style.map('Purple.TButton', background=[('active', '#7D3F9D')])
    style.configure('Cyan.TButton', background='#008080', foreground='white');
    style.map('Cyan.TButton', background=[('active', '#00AAAA')])
    style.configure('Green.TButton', background='#21ba45', foreground='white');
    style.map('Green.TButton', background=[('active', '#33cc66')])
    style.configure('Red.TButton', background='#C00000', foreground='white');
    style.map('Red.TButton', background=[('active', '#E00000')])

    # Styles for Pause/Resume
    style.configure('Pause.TButton', background='#f2c037', foreground='black',
                    font=('Segoe UI', 9, 'bold'))  # Yellowish
    style.map('Pause.TButton', background=[('active', '#f5d061')])
    style.configure('Resume.TButton', background='#38A85C', foreground='white',
                    font=('Segoe UI', 9, 'bold'))  # Greenish
    style.map('Resume.TButton', background=[('active', '#4BC876')])

    style.configure('ActiveBlue.TButton', background='#3399ff', foreground='white', relief=tk.SUNKEN)
    style.configure('ActiveOrange.TButton', background='#ff8000', foreground='white', relief=tk.SUNKEN)
    style.configure('ActiveGray.TButton', background='#bbbbbb', foreground='white', relief=tk.SUNKEN)
    style.configure('ActivePurple.TButton', background='#7D3F9D', foreground='white', relief=tk.SUNKEN)
    style.configure('ActiveCyan.TButton', background='#00AAAA', foreground='white', relief=tk.SUNKEN)

    font_small = ('Segoe UI', 9)
    font_label = ('Segoe UI', 9)
    font_value = ('Segoe UI', 9, 'bold')
    font_motor_disp = ('Segoe UI', 9)

    status_var = tk.StringVar(value="🔌 Not connected")
    main_state_var = tk.StringVar(value="UNKNOWN")
    prominent_firmware_state_var = tk.StringVar(value="---")
    homing_state_var = tk.StringVar(value="---")
    homing_phase_var = tk.StringVar(value="---")
    feed_state_var = tk.StringVar(value="---")
    error_state_var = tk.StringVar(value="---")

    motor_state1 = tk.StringVar(value="N/A");
    enabled_state1 = tk.StringVar(value="N/A")
    torque_value1 = tk.StringVar(value="--- %");
    position_cmd1_var = tk.StringVar(value="0")

    motor_state2 = tk.StringVar(value="N/A");
    enabled_state2 = tk.StringVar(value="N/A")
    torque_value2 = tk.StringVar(value="--- %");
    position_cmd2_var = tk.StringVar(value="0")

    machine_steps_var = tk.StringVar(value="0")
    cartridge_steps_var = tk.StringVar(value="0")

    torque_history1, torque_history2, torque_times = [], [], []
    enable_disable_var = tk.StringVar(value="Enabled")

    jog_steps_var = tk.StringVar(value="800");
    jog_velocity_var = tk.StringVar(value="800");
    jog_acceleration_var = tk.StringVar(value="5000")
    jog_torque_percent_var = tk.StringVar(value="30")

    homing_stroke_len_var = tk.StringVar(value="500")
    homing_rapid_vel_var = tk.StringVar(value="20")
    homing_touch_vel_var = tk.StringVar(value="1")
    homing_acceleration_var = tk.StringVar(value="50")
    homing_retract_dist_var = tk.StringVar(value="20")
    homing_torque_percent_var = tk.StringVar(value="5")

    # Feed variables
    feed_cyl1_dia_var = tk.StringVar(value="75.0")
    feed_cyl2_dia_var = tk.StringVar(value="33.0")
    feed_ballscrew_pitch_var = tk.StringVar(value="5.0")
    feed_ml_per_rev_var = tk.StringVar(value="N/A ml/rev");
    feed_steps_per_ml_var = tk.DoubleVar(value=0.0)
    feed_acceleration_var = tk.StringVar(value="5000")
    feed_torque_percent_var = tk.StringVar(value="50")
    cartridge_retract_offset_mm_var = tk.StringVar(value="50.0")

    # Inject
    inject_amount_ml_var = tk.StringVar(value="10.0");
    inject_speed_ml_s_var = tk.StringVar(value="0.1")
    inject_time_var = tk.StringVar(value="N/A s")
    inject_dispensed_ml_var = tk.StringVar(value="0.00 ml")  # New

    # Purge
    purge_amount_ml_var = tk.StringVar(value="0.5");
    purge_speed_ml_s_var = tk.StringVar(value="0.5")
    purge_time_var = tk.StringVar(value="N/A s")  # New
    purge_dispensed_ml_var = tk.StringVar(value="0.00 ml")  # New

    set_torque_offset_val_var = tk.StringVar(value="-2.4")

    ui_elements = {}

    fig, ax = plt.subplots(figsize=(7, 2.8), facecolor='#21232b')
    line1, = ax.plot([], [], color='#00bfff', label="Motor 0");
    line2, = ax.plot([], [], color='yellow', label="Motor 1")
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
        if 'terminal' in ui_elements and ui_elements['terminal'].winfo_exists():
            ui_elements['terminal'].insert(tk.END, msg);
            ui_elements['terminal'].see(tk.END)

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
        except ValueError:
            feed_ml_per_rev_var.set("Input Error"); feed_steps_per_ml_var.set(0.0)
        except Exception as e:
            feed_ml_per_rev_var.set("Calc Error"); feed_steps_per_ml_var.set(0.0); print(
                f"Error in update_ml_per_rev: {e}")

    feed_cyl1_dia_var.trace_add('write', update_ml_per_rev);
    feed_cyl2_dia_var.trace_add('write', update_ml_per_rev);
    feed_ballscrew_pitch_var.trace_add('write', update_ml_per_rev)

    def update_inject_time(*args):
        try:
            amount_ml = float(inject_amount_ml_var.get());
            speed_ml_s = float(inject_speed_ml_s_var.get())
            if speed_ml_s > 0:
                inject_time_var.set(f"{amount_ml / speed_ml_s:.2f} s")
            else:
                inject_time_var.set("N/A (Speed=0)")
        except ValueError:
            inject_time_var.set("Input Error")
        except Exception:
            inject_time_var.set("Calc Error")

    inject_amount_ml_var.trace_add('write', update_inject_time);
    inject_speed_ml_s_var.trace_add('write', update_inject_time)

    def update_purge_time(*args):  # NEW function for purge time
        try:
            amount_ml = float(purge_amount_ml_var.get());
            speed_ml_s = float(purge_speed_ml_s_var.get())
            if speed_ml_s > 0:
                purge_time_var.set(f"{amount_ml / speed_ml_s:.2f} s")
            else:
                purge_time_var.set("N/A (Speed=0)")
        except ValueError:
            purge_time_var.set("Input Error")
        except Exception:
            purge_time_var.set("Calc Error")

    purge_amount_ml_var.trace_add('write', update_purge_time);
    purge_speed_ml_s_var.trace_add('write', update_purge_time)

    def update_state(new_main_state_from_firmware):  # Includes debug prints from previous step
        print(f"GUI_DEBUG: update_state CALLED with: '{new_main_state_from_firmware}'")  # DEBUG
        main_state_var.set(new_main_state_from_firmware)
        update_jog_button_state(new_main_state_from_firmware)
        active_map = {"STANDBY_MODE": ('standby_mode_btn', 'ActiveBlue.TButton', 'Blue.TButton'),
                      "TEST_MODE": ('test_mode_btn', 'ActiveOrange.TButton', 'Orange.TButton'),
                      "JOG_MODE": ('jog_mode_btn', 'ActiveGray.TButton', 'Gray.TButton'),
                      "HOMING_MODE": ('homing_mode_btn', 'ActivePurple.TButton', 'Purple.TButton'),
                      "FEED_MODE": ('feed_mode_btn', 'ActiveCyan.TButton', 'Cyan.TButton'), }
        current_ui_mode = "Unknown"
        for fw_state, (btn_key, act_style, norm_style) in active_map.items():
            is_active = (new_main_state_from_firmware == fw_state)
            if btn_key in ui_elements and ui_elements[btn_key].winfo_exists():
                ui_elements[btn_key].config(style=act_style if is_active else norm_style)
            if is_active:
                if fw_state == "STANDBY_MODE":
                    current_ui_mode = "Standby"
                elif fw_state == "TEST_MODE":
                    current_ui_mode = "Test Mode"
                elif fw_state == "JOG_MODE":
                    current_ui_mode = "Jog"
                elif fw_state == "HOMING_MODE":
                    current_ui_mode = "Homing Mode"
                elif fw_state == "FEED_MODE":
                    current_ui_mode = "Feed Mode"
        print(f"GUI_DEBUG: current_ui_mode determined as: '{current_ui_mode}'")  # DEBUG
        context_frames = ['jog_controls_frame', 'homing_controls_frame', 'feed_controls_frame',
                          'settings_controls_frame']
        for fk in context_frames:
            if fk in ui_elements and ui_elements[fk].winfo_exists(): ui_elements[fk].pack_forget()
        print(f"GUI_DEBUG: Attempting to pack panel for '{current_ui_mode}'")  # DEBUG
        panel_packed_flag = False
        if current_ui_mode == "Jog" and 'jog_controls_frame' in ui_elements:
            print("GUI_DEBUG: Packing JOG controls")  # DEBUG
            ui_elements['jog_controls_frame'].pack(fill=tk.BOTH, expand=True, padx=5, pady=5);
            panel_packed_flag = True
        elif current_ui_mode == "Homing Mode" and 'homing_controls_frame' in ui_elements:
            print("GUI_DEBUG: Packing HOMING controls")  # DEBUG
            if ui_elements['homing_controls_frame'].winfo_exists():
                ui_elements['homing_controls_frame'].pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
                print("GUI_DEBUG: homing_controls_frame pack() called.")  # DEBUG
                panel_packed_flag = True
            else:
                print(
                    "GUI_DEBUG: ERROR - homing_controls_frame does not exist or was destroyed before packing!")  # DEBUG
        elif current_ui_mode == "Feed Mode" and 'feed_controls_frame' in ui_elements:
            print("GUI_DEBUG: Packing FEED controls")  # DEBUG
            ui_elements['feed_controls_frame'].pack(fill=tk.BOTH, expand=True, padx=5, pady=5);
            panel_packed_flag = True
        if not panel_packed_flag: print(
            f"GUI_DEBUG: No specific contextual panel was packed for current_ui_mode: '{current_ui_mode}'")  # DEBUG
        if 'settings_controls_frame' in ui_elements and ui_elements['settings_controls_frame'].winfo_exists():
            ui_elements['settings_controls_frame'].pack(fill=tk.X, expand=False, padx=5, pady=(10, 5))

    def update_motor_status(motor, text, color="#ffffff"):  # motor is 1 for M0, 2 for M1
        lbl_k = f'motor_status_label{motor}';
        var = motor_state1 if motor == 1 else motor_state2;
        var.set(text)
        if lbl_k in ui_elements and ui_elements[lbl_k].winfo_exists(): ui_elements[lbl_k].config(fg=color)

    def update_enabled_status(motor, text, color="#ffffff"):  # motor is 1 for M0, 2 for M1
        lbl_k = f'enabled_status_label{motor}';
        var = enabled_state1 if motor == 1 else enabled_state2;
        var.set(text)
        if lbl_k in ui_elements and ui_elements[lbl_k].winfo_exists(): ui_elements[lbl_k].config(fg=color)

    def update_torque_plot(v1, v2, ts, h1, h2):
        v1_float = 0.0;
        v2_float = 0.0
        try:
            v1_float = float(v1) if v1 != "---" else 0.0
        except ValueError:
            v1_float = 0.0
        try:
            v2_float = float(v2) if v2 != "---" else 0.0
        except ValueError:
            v2_float = 0.0
        torque_value1.set(f"{v1_float:.2f}%" if v1 != "---" else "--- %");
        torque_value2.set(f"{v2_float:.2f}%" if v2 != "---" else "--- %")
        if h1 and h2 and ts and 'canvas_widget' in ui_elements and ui_elements['canvas_widget'].winfo_exists():
            if not ts: return
            ax.set_xlim(ts[0], ts[-1]);
            line1.set_data(ts, h1);
            line2.set_data(ts, h2)
            min_y_data = min(min(h1, default=-5), min(h2, default=-5)) if h1 and h2 else -5
            max_y_data = max(max(h1, default=105), max(h2, default=105)) if h1 and h2 else 105
            cmin, cmax = ax.get_ylim();
            nmin = min(min_y_data - 10, cmin if len(ts) > 1 else -10);
            nmax = max(max_y_data + 10, cmax if len(ts) > 1 else 110)
            if nmax - nmin < 20: nmax = nmin + 20
            nmin = max(nmin, -10);
            nmax = min(nmax, 110)
            if nmin >= nmax - 10: nmin = nmax - 20
            ax.set_ylim(nmin, nmax);
            fig.canvas.draw_idle()

    def update_jog_button_state(current_main_state):
        names = ['jog_m1_plus', 'jog_m1_minus', 'jog_m2_plus', 'jog_m2_minus', 'jog_both_plus', 'jog_both_minus']
        desired = tk.NORMAL if current_main_state == "JOG_MODE" else tk.DISABLED
        for n in names:
            if n in ui_elements and ui_elements[n].winfo_exists(): ui_elements[n].config(state=desired)

    main_frame = tk.Frame(root, bg="#21232b");
    main_frame.pack(expand=True, fill=tk.BOTH)
    top_row_frame = tk.Frame(main_frame, bg="#21232b");
    top_row_frame.pack(fill=tk.X, pady=(5, 2))

    action_buttons_frame = tk.Frame(top_row_frame, bg="#21232b")
    action_buttons_frame.pack(side=tk.RIGHT, padx=10)
    ui_elements['abort_btn'] = tk.Button(action_buttons_frame, text="🛑 ABORT", bg="#db2828", fg="white",
                                         font=("Segoe UI", 10, "bold"), command=lambda: send_udp("ABORT"),
                                         relief="raised", bd=3, padx=10, pady=5)
    ui_elements['abort_btn'].pack(side=tk.LEFT, padx=(0, 5))
    ui_elements['clear_errors_btn'] = ttk.Button(action_buttons_frame, text="⚠️ Clear Errors", style='Yellow.TButton',
                                                 command=lambda: send_udp("CLEAR_ERRORS"))  # NEW BUTTON
    ui_elements['clear_errors_btn'].pack(side=tk.LEFT, padx=(5, 0))

    tk.Label(top_row_frame, textvariable=status_var, bg="#21232b", fg="white", font=("Segoe UI", 12)).pack(side=tk.LEFT,
                                                                                                           padx=(10, 2))
    ui_elements['prominent_firmware_state_var'] = prominent_firmware_state_var
    prominent_state_display_frame = tk.Frame(top_row_frame, bg="#444", bd=2, relief="sunken");
    prominent_state_display_frame.pack(side=tk.LEFT, fill=tk.X, expand=True, pady=(0, 0), padx=10, ipady=0)
    ui_elements['prominent_state_display_frame'] = prominent_state_display_frame
    ui_elements['prominent_state_label'] = tk.Label(prominent_state_display_frame,
                                                    textvariable=prominent_firmware_state_var,
                                                    font=("Segoe UI", 11, "bold"), fg="white",
                                                    bg=prominent_state_display_frame['bg'])
    ui_elements['prominent_state_label'].pack(expand=True, fill=tk.X, padx=10, pady=(0, 0))

    content_frame = tk.Frame(main_frame, bg="#21232b");
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
                                                 command=lambda: send_udp("STANDBY_MODE"));
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
    enable_disable_frame.pack(fill=tk.X, pady=(0, 5), padx=0);
    button_container_frame = tk.Frame(enable_disable_frame, bg=enable_disable_frame['bg']);
    button_container_frame.pack();
    bright_green, dim_green = "#21ba45", "#198734";
    bright_red, dim_red = "#db2828", "#932020"

    def set_enabled_cmd():
        send_udp("ENABLE")

    def set_disabled_cmd():
        send_udp("DISABLE")

    ui_elements['enable_btn'] = tk.Button(button_container_frame, text="Enable", fg="white", width=12,
                                          command=set_enabled_cmd, font=("Segoe UI", 10, "bold"));
    ui_elements['disable_btn'] = tk.Button(button_container_frame, text="Disable", fg="white", width=12,
                                           command=set_disabled_cmd, font=("Segoe UI", 10, "bold"))

    def update_enable_button_appearance(*args):
        is_en = enable_disable_var.get() == "Enabled"; ui_elements['enable_btn'].config(
            relief="sunken" if is_en else "raised", bg=bright_green if is_en else dim_green); ui_elements[
            'disable_btn'].config(relief="sunken" if not is_en else "raised", bg=bright_red if not is_en else dim_red)

    ui_elements['enable_btn'].pack(side=tk.LEFT, padx=(0, 1));
    ui_elements['disable_btn'].pack(side=tk.LEFT, padx=(1, 0));
    enable_disable_var.trace_add('write', update_enable_button_appearance);
    update_enable_button_appearance()
    always_on_motor_info_frame = tk.Frame(right_of_modes_area, bg="#21232b");
    always_on_motor_info_frame.pack(side=tk.TOP, fill=tk.X, expand=False, pady=(5, 0))
    m1_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 0 Status", bg="#1b2432", fg="#00bfff",
                                       font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2);
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
    tk.Label(m1_display_section, text="Abs Pos Cmd:", bg=m1_display_section['bg'], fg="white",
             font=font_motor_disp).grid(row=3, column=0, sticky="w");
    tk.Label(m1_display_section, textvariable=position_cmd1_var, bg=m1_display_section['bg'], fg="#00bfff",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)
    m2_display_section = tk.LabelFrame(always_on_motor_info_frame, text="Motor 1 Status", bg="#2d253a", fg="yellow",
                                       font=("Segoe UI", 10, "bold"), bd=1, relief="ridge", padx=5, pady=2);
    m2_display_section.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 0));
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
    tk.Label(m2_display_section, text="Abs Pos Cmd:", bg=m2_display_section['bg'], fg="white",
             font=font_motor_disp).grid(row=3, column=0, sticky="w");
    tk.Label(m2_display_section, textvariable=position_cmd2_var, bg=m2_display_section['bg'], fg="yellow",
             font=font_motor_disp).grid(row=3, column=1, sticky="ew", padx=2)

    contextual_display_master_frame = tk.Frame(right_of_modes_area, bg="#21232b");
    contextual_display_master_frame.pack(fill=tk.BOTH, expand=True, pady=(5, 0))
    jog_controls_frame = tk.LabelFrame(contextual_display_master_frame, text="Jog Controls (Active in JOG_MODE)",
                                       bg="#21232b", fg="#0f8", font=("Segoe UI", 10, "bold"));
    ui_elements['jog_controls_frame'] = jog_controls_frame
    jog_params_frame = tk.Frame(jog_controls_frame, bg=jog_controls_frame['bg']);
    jog_params_frame.pack(fill=tk.X, pady=5, padx=5);
    jog_params_frame.grid_columnconfigure(1, weight=1);
    jog_params_frame.grid_columnconfigure(3, weight=1)
    tk.Label(jog_params_frame, text="Steps:", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                           column=0,
                                                                                                           sticky="w",
                                                                                                           pady=1,
                                                                                                           padx=2);
    ttk.Entry(jog_params_frame, textvariable=jog_steps_var, width=8, font=font_small).grid(row=0, column=1, sticky="ew",
                                                                                           pady=1, padx=(0, 10));
    tk.Label(jog_params_frame, text="Vel (sps):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=0,
                                                                                                               column=2,
                                                                                                               sticky="w",
                                                                                                               pady=1,
                                                                                                               padx=2);
    ttk.Entry(jog_params_frame, textvariable=jog_velocity_var, width=8, font=font_small).grid(row=0, column=3,
                                                                                              sticky="ew", pady=1,
                                                                                              padx=(0, 10));
    tk.Label(jog_params_frame, text="Accel (sps²):", bg=jog_params_frame['bg'], fg="white", font=font_small).grid(row=1,
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
    m0_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=5);
    ui_elements['jog_m1_plus'] = ttk.Button(m0_jog_frame, text="▲ M0", style="Jog.TButton", state=tk.DISABLED,
                                            command=lambda: send_udp(
                                                f"JOG_MOVE {jog_steps_var.get()} 0 {jog_torque_percent_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_m1_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2));
    ui_elements['jog_m1_minus'] = ttk.Button(m0_jog_frame, text="▼ M0", style="Jog.TButton", state=tk.DISABLED,
                                             command=lambda: send_udp(
                                                 f"JOG_MOVE -{jog_steps_var.get()} 0 {jog_torque_percent_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_m1_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)
    m1_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg']);
    m1_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=5);
    ui_elements['jog_m2_plus'] = ttk.Button(m1_jog_frame, text="▲ M1", style="Jog.TButton", state=tk.DISABLED,
                                            command=lambda: send_udp(
                                                f"JOG_MOVE 0 {jog_steps_var.get()} {jog_torque_percent_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_m2_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2));
    ui_elements['jog_m2_minus'] = ttk.Button(m1_jog_frame, text="▼ M1", style="Jog.TButton", state=tk.DISABLED,
                                             command=lambda: send_udp(
                                                 f"JOG_MOVE 0 -{jog_steps_var.get()} {jog_torque_percent_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_m2_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)
    both_jog_frame = tk.Frame(jog_buttons_area, bg=jog_controls_frame['bg']);
    both_jog_frame.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=5);
    ui_elements['jog_both_plus'] = ttk.Button(both_jog_frame, text="▲ Both", style="Jog.TButton", state=tk.DISABLED,
                                              command=lambda: send_udp(
                                                  f"JOG_MOVE {jog_steps_var.get()} {jog_steps_var.get()} {jog_torque_percent_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_both_plus'].pack(side=tk.TOP, fill=tk.X, expand=True, pady=(0, 2));
    ui_elements['jog_both_minus'] = ttk.Button(both_jog_frame, text="▼ Both", style="Jog.TButton", state=tk.DISABLED,
                                               command=lambda: send_udp(
                                                   f"JOG_MOVE -{jog_steps_var.get()} -{jog_steps_var.get()} {jog_torque_percent_var.get()} {jog_velocity_var.get()} {jog_acceleration_var.get()}"));
    ui_elements['jog_both_minus'].pack(side=tk.TOP, fill=tk.X, expand=True)
    homing_controls_frame = tk.LabelFrame(contextual_display_master_frame,
                                          text="Homing Controls (Active in HOMING_MODE)", bg="#2b1e34", fg="#D8BFD8",
                                          font=("Segoe UI", 10, "bold"), bd=2, relief="ridge");
    ui_elements['homing_controls_frame'] = homing_controls_frame
    homing_controls_frame.grid_columnconfigure(1, weight=1);
    homing_controls_frame.grid_columnconfigure(3, weight=1);
    h_row = 0;
    field_width_homing = 10;
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
    ttk.Button(home_btn_frame, text="Execute Machine Home", style='Small.TButton', command=lambda: send_udp(
        f"MACHINE_HOME_MOVE {homing_stroke_len_var.get()} {homing_rapid_vel_var.get()} {homing_touch_vel_var.get()} {homing_acceleration_var.get()} {homing_retract_dist_var.get()} {homing_torque_percent_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(home_btn_frame, text="Execute Cartridge Home", style='Small.TButton', command=lambda: send_udp(
        f"CARTRIDGE_HOME_MOVE {homing_stroke_len_var.get()} {homing_rapid_vel_var.get()} {homing_touch_vel_var.get()} {homing_acceleration_var.get()} 0 {homing_torque_percent_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)

    # --- UPDATED Feed Controls Frame ---
    feed_controls_frame = tk.LabelFrame(contextual_display_master_frame, text="Feed Controls (Active in FEED_MODE)",
                                        bg="#1e3434", fg="#AFEEEE", font=("Segoe UI", 10, "bold"), bd=2, relief="ridge")
    ui_elements['feed_controls_frame'] = feed_controls_frame
    feed_controls_frame.grid_columnconfigure(1, weight=1);
    feed_controls_frame.grid_columnconfigure(3, weight=1)
    f_row = 0;
    field_width_feed = 8

    tk.Label(feed_controls_frame, text="Cyl 1 Dia (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_cyl1_dia_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2)
    tk.Label(feed_controls_frame, text="Cyl 2 Dia (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_cyl2_dia_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2)
    f_row += 1
    tk.Label(feed_controls_frame, text="Pitch (mm/rev):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_ballscrew_pitch_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Calc ml/rev:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="w", padx=2, pady=2)
    tk.Label(feed_controls_frame, textvariable=feed_ml_per_rev_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)
    f_row += 1
    tk.Label(feed_controls_frame, text="Calc Steps/ml:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    tk.Label(feed_controls_frame, textvariable=feed_steps_per_ml_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Feed Accel (sps²):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_acceleration_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2)
    f_row += 1
    tk.Label(feed_controls_frame, text="Torque Limit (%):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=feed_torque_percent_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2)
    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew',
                                                                 pady=5);
    f_row += 1

    tk.Label(feed_controls_frame, text="Retract Offset (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, columnspan=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=cartridge_retract_offset_mm_var, width=field_width_feed,
              font=font_small).grid(row=f_row, column=2, sticky="ew", padx=2, pady=2)
    f_row += 1
    positioning_btn_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    positioning_btn_frame.grid(row=f_row, column=0, columnspan=4, pady=(5, 2), sticky="ew")
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Home", style='Green.TButton',
               command=lambda: send_udp("MOVE_TO_CARTRIDGE_HOME")).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Retract", style='Green.TButton',
               command=lambda: send_udp(f"MOVE_TO_CARTRIDGE_RETRACT {cartridge_retract_offset_mm_var.get()}")).pack(
        side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew',
                                                                 pady=5);
    f_row += 1

    # Inject Section
    tk.Label(feed_controls_frame, text="Inject Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=inject_amount_ml_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2)
    tk.Label(feed_controls_frame, text="Inject Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=inject_speed_ml_s_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Est. Inject Time:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    tk.Label(feed_controls_frame, textvariable=inject_time_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2);
    tk.Label(feed_controls_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="w", padx=2, pady=2)  # New Dispensed Label
    tk.Label(feed_controls_frame, textvariable=inject_dispensed_ml_var, bg=feed_controls_frame['bg'], fg='lightgreen',
             font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2);  # New Dispensed Value
    f_row += 1

    inject_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    inject_op_frame.grid(row=f_row, column=0, columnspan=4, pady=(2, 5), sticky="ew")
    ui_elements['start_inject_btn'] = ttk.Button(inject_op_frame, text="Start Inject", style='Small.TButton',
                                                 command=lambda: send_udp(
                                                     f"INJECT_MOVE {inject_amount_ml_var.get()} {inject_speed_ml_s_var.get()} {feed_acceleration_var.get()} {feed_steps_per_ml_var.get()} {feed_torque_percent_var.get()}"))
    ui_elements['start_inject_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(20, 2))
    ui_elements['pause_inject_btn'] = ttk.Button(inject_op_frame, text="Pause", style='Pause.TButton',
                                                 command=lambda: send_udp("PAUSE_INJECT"))
    ui_elements['pause_inject_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ui_elements['resume_inject_btn'] = ttk.Button(inject_op_frame, text="Resume", style='Resume.TButton',
                                                  command=lambda: send_udp("RESUME_INJECT"))
    ui_elements['resume_inject_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 20))
    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew',
                                                                 pady=5);
    f_row += 1

    # Purge Section
    tk.Label(feed_controls_frame, text="Purge Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=purge_amount_ml_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=1, sticky="ew", padx=2, pady=2)
    tk.Label(feed_controls_frame, text="Purge Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="w", padx=2, pady=2)
    ttk.Entry(feed_controls_frame, textvariable=purge_speed_ml_s_var, width=field_width_feed, font=font_small).grid(
        row=f_row, column=3, sticky="ew", padx=2, pady=2);
    f_row += 1
    tk.Label(feed_controls_frame, text="Est. Purge Time:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="w", padx=2, pady=2)  # New
    tk.Label(feed_controls_frame, textvariable=purge_time_var, bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2);  # New
    tk.Label(feed_controls_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="w", padx=2, pady=2)  # New Dispensed Label
    tk.Label(feed_controls_frame, textvariable=purge_dispensed_ml_var, bg=feed_controls_frame['bg'], fg='lightgreen',
             font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2);  # New Dispensed Value
    f_row += 1
    purge_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    purge_op_frame.grid(row=f_row, column=0, columnspan=4, pady=(2, 5), sticky="ew")
    ui_elements['start_purge_btn'] = ttk.Button(purge_op_frame, text="Start Purge", style='Small.TButton',
                                                command=lambda: send_udp(
                                                    f"PURGE_MOVE {purge_amount_ml_var.get()} {purge_speed_ml_s_var.get()} {feed_acceleration_var.get()} {feed_steps_per_ml_var.get()} {feed_torque_percent_var.get()}"))
    ui_elements['start_purge_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(20, 2))
    ui_elements['pause_purge_btn'] = ttk.Button(purge_op_frame, text="Pause", style='Pause.TButton',
                                                command=lambda: send_udp("PAUSE_PURGE"), state=tk.DISABLED)
    ui_elements['pause_purge_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
    ui_elements['resume_purge_btn'] = ttk.Button(purge_op_frame, text="Resume", style='Resume.TButton',
                                                 command=lambda: send_udp("RESUME_PURGE"), state=tk.DISABLED)
    ui_elements['resume_purge_btn'].pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 20))

    update_ml_per_rev();
    update_inject_time();
    update_purge_time()

    settings_controls_frame = tk.LabelFrame(right_of_modes_area, text="Global Settings", bg="#303030", fg="#DDD",
                                            font=("Segoe UI", 10, "bold"), bd=2, relief="ridge");
    ui_elements['settings_controls_frame'] = settings_controls_frame;
    settings_controls_frame.grid_columnconfigure(1, weight=1);
    s_row = 0
    tk.Label(settings_controls_frame, text="Torque Offset:", bg=settings_controls_frame['bg'], fg='white',
             font=font_small).grid(row=s_row, column=0, sticky="w", padx=2, pady=2);
    ttk.Entry(settings_controls_frame, textvariable=set_torque_offset_val_var, width=10, font=font_small).grid(
        row=s_row, column=1, sticky="ew", padx=2, pady=2);
    ttk.Button(settings_controls_frame, text="Set Offset", style='Small.TButton',
               command=lambda: send_udp(f"SET_TORQUE_OFFSET {set_torque_offset_val_var.get()}")).grid(row=s_row,
                                                                                                      column=2,
                                                                                                      padx=(5, 2),
                                                                                                      pady=2)
    right_column_frame = tk.Frame(content_frame, bg="#21232b");
    right_column_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(5, 10))
    chart_frame = tk.Frame(right_column_frame, bg="#21232b");
    chart_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 5));
    canvas = FigureCanvasTkAgg(fig, master=chart_frame);
    ui_elements['canvas_widget'] = canvas.get_tk_widget();
    ui_elements['canvas_widget'].pack(side=tk.TOP, fill=tk.BOTH, expand=True)
    ui_elements['terminal'] = tk.Text(right_column_frame, height=8, bg="#1b1e2b", fg="#0f8", insertbackground="white",
                                      wrap="word", highlightbackground="#34374b", highlightthickness=1, bd=0,
                                      font=("Consolas", 10));
    ui_elements['terminal'].pack(fill=tk.X, expand=False, pady=(5, 0))

    ui_elements.update({
        'root': root, 'update_status': update_status,
        'update_motor_status': update_motor_status, 'update_enabled_status': update_enabled_status,
        'machine_steps_var': machine_steps_var, 'cartridge_steps_var': cartridge_steps_var,
        'torque_history1': torque_history1, 'torque_history2': torque_history2, 'torque_times': torque_times,
        'update_torque_plot': update_torque_plot, 'terminal_cb': terminal_cb,
        'update_state': update_state, 'update_position_cmd': update_position_cmd,
        'main_state_var': main_state_var, 'homing_state_var': homing_state_var, 'homing_phase_var': homing_phase_var,
        'feed_state_var': feed_state_var, 'error_state_var': error_state_var,
        'prominent_firmware_state_var': prominent_firmware_state_var,
        'prominent_state_display_frame': prominent_state_display_frame,
        'prominent_state_label': ui_elements['prominent_state_label'],
        'enable_disable_var': enable_disable_var,
        'motor_state1_var': motor_state1, 'enabled_state1_var': enabled_state1, 'torque_value1_var': torque_value1,
        'position_cmd1_var': position_cmd1_var,
        'motor_state2_var': motor_state2, 'enabled_state2_var': enabled_state2, 'torque_value2_var': torque_value2,
        'position_cmd2_var': position_cmd2_var,
        'enabled_status_label1': ui_elements['enabled_status_label1'],
        'enabled_status_label2': ui_elements['enabled_status_label2'],
        'motor_status_label1': ui_elements['motor_status_label1'],
        'motor_status_label2': ui_elements['motor_status_label2'],
        'inject_dispensed_ml_var': inject_dispensed_ml_var,
        'purge_dispensed_ml_var': purge_dispensed_ml_var,
        'start_inject_btn': ui_elements['start_inject_btn'], 'pause_inject_btn': ui_elements['pause_inject_btn'],
        'resume_inject_btn': ui_elements['resume_inject_btn'],
        'start_purge_btn': ui_elements['start_purge_btn'], 'pause_purge_btn': ui_elements['pause_purge_btn'],
        'resume_purge_btn': ui_elements['resume_purge_btn'],
    })
    root.after(20, lambda: update_state(main_state_var.get()))
    return ui_elements


if __name__ == '__main__':
    mock_send_udp = lambda msg: print(f"GUI SEND: {msg}")
    mock_discover = lambda: print("GUI DISCOVER")
    gui = build_gui(mock_send_udp, mock_discover)


    def mock_update_telemetry():
        if gui['root'].winfo_exists():
            gui['inject_dispensed_ml_var'].set(f"{(time.time() % 5):.2f} ml")
            gui['purge_dispensed_ml_var'].set(f"{(time.time() % 2):.2f} ml")
            gui['root'].after(100, mock_update_telemetry)


    if gui['root'].winfo_exists():
        gui['update_ml_per_rev']();
        gui['update_inject_time']();
        gui['update_purge_time']()
        # gui['root'].after(100, mock_update_telemetry)
    gui['root'].mainloop()