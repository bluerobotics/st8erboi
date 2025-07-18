import tkinter as tk
from tkinter import ttk


def create_feed_controls(parent, send_cmd_func, ui_elements, variables):
    """
    Creates and populates the Feed controls frame.
    """

    # --- FIX: Initialize all required StringVars for this component ---
    if 'feed_cyl1_dia_var' not in variables:
        variables['feed_cyl1_dia_var'] = tk.StringVar(value='10.0')
    if 'feed_cyl2_dia_var' not in variables:
        variables['feed_cyl2_dia_var'] = tk.StringVar(value='10.0')
    if 'feed_ballscrew_pitch_var' not in variables:
        variables['feed_ballscrew_pitch_var'] = tk.StringVar(value='2.0')
    if 'feed_ml_per_rev_var' not in variables:
        variables['feed_ml_per_rev_var'] = tk.StringVar(value='0.0')
    if 'feed_steps_per_ml_var' not in variables:
        variables['feed_steps_per_ml_var'] = tk.StringVar(value='0.0')
    if 'feed_acceleration_var' not in variables:
        variables['feed_acceleration_var'] = tk.StringVar(value='5000')
    if 'feed_torque_percent_var' not in variables:
        variables['feed_torque_percent_var'] = tk.StringVar(value='50')
    if 'cartridge_retract_offset_mm_var' not in variables:
        variables['cartridge_retract_offset_mm_var'] = tk.StringVar(value='1.0')
    if 'inject_amount_ml_var' not in variables:
        variables['inject_amount_ml_var'] = tk.StringVar(value='0.1')
    if 'inject_speed_ml_s_var' not in variables:
        variables['inject_speed_ml_s_var'] = tk.StringVar(value='0.1')
    if 'inject_time_var' not in variables:
        variables['inject_time_var'] = tk.StringVar(value='---')
    if 'inject_dispensed_ml_var' not in variables:
        variables['inject_dispensed_ml_var'] = tk.StringVar(value='---')
    if 'purge_amount_ml_var' not in variables:
        variables['purge_amount_ml_var'] = tk.StringVar(value='0.5')
    if 'purge_speed_ml_s_var' not in variables:
        variables['purge_speed_ml_s_var'] = tk.StringVar(value='0.5')
    if 'purge_time_var' not in variables:
        variables['purge_time_var'] = tk.StringVar(value='---')
    if 'purge_dispensed_ml_var' not in variables:
        variables['purge_dispensed_ml_var'] = tk.StringVar(value='---')
    # --- END FIX ---

    feed_controls_frame = tk.LabelFrame(parent, text="Feed Controls",
                                        bg="#1e3434", fg="#AFEEEE", font=("Segoe UI", 10, "bold"),
                                        bd=2, relief="ridge", padx=10, pady=10)
    feed_controls_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)


    font_small = ('Segoe UI', 9)
    field_width_feed = 8

    # Removed column weighting to keep widgets compact
    # feed_controls_frame.grid_columnconfigure(1, weight=1)
    # feed_controls_frame.grid_columnconfigure(3, weight=1)

    f_row = 0
    tk.Label(feed_controls_frame, text="Cyl 1 Dia (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['feed_cyl1_dia_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    tk.Label(feed_controls_frame, text="Cyl 2 Dia (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="e", padx=(10,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['feed_cyl2_dia_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)

    f_row += 1
    tk.Label(feed_controls_frame, text="Pitch (mm/rev):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['feed_ballscrew_pitch_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    tk.Label(feed_controls_frame, text="Calc ml/rev:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="e", padx=(10,5), pady=2)
    tk.Label(feed_controls_frame, textvariable=variables['feed_ml_per_rev_var'], bg=feed_controls_frame['bg'],
             fg='cyan', font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)

    f_row += 1
    tk.Label(feed_controls_frame, text="Calc Steps/ml:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    tk.Label(feed_controls_frame, textvariable=variables['feed_steps_per_ml_var'], bg=feed_controls_frame['bg'],
             fg='cyan', font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    tk.Label(feed_controls_frame, text="Feed Accel (sps²):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="e", padx=(10,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['feed_acceleration_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)

    f_row += 1
    tk.Label(feed_controls_frame, text="Torque Limit (%):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['feed_torque_percent_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew', pady=5)

    f_row += 1
    tk.Label(feed_controls_frame, text="Retract Offset (mm):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, columnspan=2, sticky="e", padx=(0,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['cartridge_retract_offset_mm_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=2, sticky="w", columnspan=2, padx=2, pady=2)

    f_row += 1
    positioning_btn_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    positioning_btn_frame.grid(row=f_row, column=0, columnspan=4, pady=(5, 2), sticky="ew")
    positioning_btn_frame.grid_columnconfigure((0,1), weight=1)
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Home", style='Green.TButton',
               command=lambda: send_cmd_func("MOVE_TO_CARTRIDGE_HOME")).grid(row=0, column=0, sticky="ew", padx=(0,2))
    ttk.Button(positioning_btn_frame, text="Go to Cartridge Retract", style='Green.TButton',
               command=lambda: send_cmd_func(
                   f"MOVE_TO_CARTRIDGE_RETRACT {variables['cartridge_retract_offset_mm_var'].get()}")).grid(
        row=0, column=1, sticky="ew", padx=(2,0))

    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew', pady=5)

    f_row += 1
    tk.Label(feed_controls_frame, text="Inject Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['inject_amount_ml_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    tk.Label(feed_controls_frame, text="Inject Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="e", padx=(10,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['inject_speed_ml_s_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)

    f_row += 1
    tk.Label(feed_controls_frame, text="Est. Inject Time:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    tk.Label(feed_controls_frame, textvariable=variables['inject_time_var'], bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    tk.Label(feed_controls_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="e", padx=(10,5), pady=2)
    tk.Label(feed_controls_frame, textvariable=variables['inject_dispensed_ml_var'], bg=feed_controls_frame['bg'],
             fg='lightgreen', font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)

    f_row += 1
    inject_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    inject_op_frame.grid(row=f_row, column=0, columnspan=4, pady=(2, 5), sticky="ew")
    for i in range(4): inject_op_frame.grid_columnconfigure(i, weight=1)
    ui_elements['start_inject_btn'] = ttk.Button(inject_op_frame, text="Start Inject", style='Start.TButton',
                                                 state='normal', command=lambda: send_cmd_func(
            f"INJECT_MOVE {variables['inject_amount_ml_var'].get()} {variables['inject_speed_ml_s_var'].get()} {variables['feed_acceleration_var'].get()} {variables['feed_steps_per_ml_var'].get()} {variables['feed_torque_percent_var'].get()}"))
    ui_elements['start_inject_btn'].grid(row=0, column=0, sticky="ew", padx=(0,2))
    ui_elements['pause_inject_btn'] = ttk.Button(inject_op_frame, text="Pause", style='Pause.TButton', state='disabled',
                                                 command=lambda: send_cmd_func("PAUSE_INJECTION"))
    ui_elements['pause_inject_btn'].grid(row=0, column=1, sticky="ew", padx=2)
    ui_elements['resume_inject_btn'] = ttk.Button(inject_op_frame, text="Resume", style='Resume.TButton',
                                                  state='disabled', command=lambda: send_cmd_func("RESUME_INJECTION"))
    ui_elements['resume_inject_btn'].grid(row=0, column=2, sticky="ew", padx=2)
    ui_elements['cancel_inject_btn'] = ttk.Button(inject_op_frame, text="Cancel", style='Cancel.TButton',
                                                  state='disabled', command=lambda: send_cmd_func("CANCEL_INJECTION"))
    ui_elements['cancel_inject_btn'].grid(row=0, column=3, sticky="ew", padx=(2,0))

    f_row += 1
    ttk.Separator(feed_controls_frame, orient='horizontal').grid(row=f_row, column=0, columnspan=4, sticky='ew', pady=5)

    f_row += 1
    tk.Label(feed_controls_frame, text="Purge Vol (ml):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['purge_amount_ml_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    tk.Label(feed_controls_frame, text="Purge Vel (ml/s):", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=2, sticky="e", padx=(10,5), pady=2)
    ttk.Entry(feed_controls_frame, textvariable=variables['purge_speed_ml_s_var'], width=field_width_feed,
              font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)

    f_row += 1
    tk.Label(feed_controls_frame, text="Est. Purge Time:", bg=feed_controls_frame['bg'], fg='white',
             font=font_small).grid(row=f_row, column=0, sticky="e", padx=(0,5), pady=2)
    tk.Label(feed_controls_frame, textvariable=variables['purge_time_var'], bg=feed_controls_frame['bg'], fg='cyan',
             font=font_small).grid(row=f_row, column=1, sticky="w", padx=2, pady=2)

    tk.Label(feed_controls_frame, text="Dispensed:", bg=feed_controls_frame['bg'], fg='white', font=font_small).grid(
        row=f_row, column=2, sticky="e", padx=(10,5), pady=2)
    tk.Label(feed_controls_frame, textvariable=variables['purge_dispensed_ml_var'], bg=feed_controls_frame['bg'],
             fg='lightgreen', font=font_small).grid(row=f_row, column=3, sticky="w", padx=2, pady=2)

    f_row += 1
    purge_op_frame = tk.Frame(feed_controls_frame, bg=feed_controls_frame['bg'])
    purge_op_frame.grid(row=f_row, column=0, columnspan=4, pady=(2, 5), sticky="ew")
    for i in range(4): purge_op_frame.grid_columnconfigure(i, weight=1)
    ui_elements['start_purge_btn'] = ttk.Button(purge_op_frame, text="Start Purge", style='Start.TButton',
                                                state='normal', command=lambda: send_cmd_func(
            f"PURGE_MOVE {variables['purge_amount_ml_var'].get()} {variables['purge_speed_ml_s_var'].get()} {variables['feed_acceleration_var'].get()} {variables['feed_steps_per_ml_var'].get()} {variables['feed_torque_percent_var'].get()}"))
    ui_elements['start_purge_btn'].grid(row=0, column=0, sticky="ew", padx=(0,2))
    ui_elements['pause_purge_btn'] = ttk.Button(purge_op_frame, text="Pause", style='Pause.TButton', state='disabled',
                                                command=lambda: send_cmd_func("PAUSE_INJECTION"))
    ui_elements['pause_purge_btn'].grid(row=0, column=1, sticky="ew", padx=2)
    ui_elements['resume_purge_btn'] = ttk.Button(purge_op_frame, text="Resume", style='Resume.TButton',
                                                 state='disabled', command=lambda: send_cmd_func("RESUME_INJECTION"))
    ui_elements['resume_purge_btn'].grid(row=0, column=2, sticky="ew", padx=2)
    ui_elements['cancel_purge_btn'] = ttk.Button(purge_op_frame, text="Cancel", style='Cancel.TButton',
                                                 state='disabled', command=lambda: send_cmd_func("CANCEL_INJECTION"))
    ui_elements['cancel_purge_btn'].grid(row=0, column=3, sticky="ew", padx=(2,0))
