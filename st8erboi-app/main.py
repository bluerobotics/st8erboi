import tkinter as tk
from tkinter import ttk
import threading
import time
import datetime
import math
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import numpy as np

import comms
from shared_gui import create_shared_widgets
from injector_gui import create_injector_tab
from fillhead_gui import create_fillhead_tab
from peer_comms_gui import create_peer_comms_tab

GUI_UPDATE_INTERVAL_MS = 100
PLOT_UPDATE_INTERVAL_MS = 250


def configure_styles():
    style = ttk.Style()
    try:
        style.theme_use('clam')
    except tk.TclError:
        print("Warning: 'clam' theme not available. Using default.")

    style.layout('TButton',
                 [('Button.button', {'children': [('Button.focus', {'children': [
                     ('Button.padding', {'children': [('Button.label', {'side': 'left', 'expand': 1})]})]})]})])
    style.configure("Green.TButton", background='#21ba45', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Green.TButton", background=[('pressed', '#198734'), ('active', '#25cc4c')])
    style.configure("Red.TButton", background='#db2828', foreground='white', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Red.TButton", background=[('pressed', '#b32424'), ('active', 'red')])
    style.configure("Neutral.TButton", background='#495057', foreground='#f8f9fa', borderwidth=0)
    style.map("Neutral.TButton", background=[('pressed', '#343a40'), ('active', '#5a6268')])


def main():
    root = tk.Tk()
    root.title("Multi-Device Controller")
    root.configure(bg="#21232b")

    configure_styles()

    shared_gui_refs = {}

    send_injector_cmd = lambda msg: comms.send_to_device("injector", msg, shared_gui_refs)
    send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, shared_gui_refs)

    def send_global_abort():
        comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", shared_gui_refs.get('terminal_cb'))
        send_injector_cmd("ABORT")
        send_fillhead_cmd("ABORT")

    command_funcs = {
        "abort": send_global_abort,
        "clear_errors": lambda: send_injector_cmd("CLEAR_ERRORS"),
        "send_injector": send_injector_cmd,
        "send_fillhead": send_fillhead_cmd
    }

    shared_widgets = create_shared_widgets(root, command_funcs)
    shared_gui_refs.update(shared_widgets)

    notebook = ttk.Notebook(root)

    injector_widgets = create_injector_tab(notebook, send_injector_cmd, shared_gui_refs)
    shared_gui_refs.update(injector_widgets)

    fillhead_widgets = create_fillhead_tab(notebook, send_fillhead_cmd, shared_gui_refs)
    shared_gui_refs.update(fillhead_widgets)

    peer_comms_widgets = create_peer_comms_tab(notebook, command_funcs)
    shared_gui_refs.update(peer_comms_widgets)

    notebook.pack(expand=True, fill="both", padx=10, pady=5)
    shared_widgets['shared_bottom_frame'].pack(fill=tk.X, expand=False, padx=10, pady=(0, 10))

    def update_torque_bar(canvas, bar_item, text_item, torque_val):
        if not all([canvas, bar_item, text_item]): return
        clamped_torque = max(0, min(100, torque_val))
        bar_height = (clamped_torque / 100.0) * 100
        y0 = 110 - bar_height
        canvas.coords(bar_item, 10, y0, 30, 110)
        canvas.itemconfig(text_item, text=f"{torque_val:.1f}%")
        if torque_val > 85:
            color = '#db2828'
        elif torque_val > 60:
            color = '#f2c037'
        else:
            color = '#21ba45'
        canvas.itemconfig(bar_item, fill=color)

    def update_injector_torque_bars():
        for i in range(1, 4):
            try:
                torque_str = shared_gui_refs.get(f'torque_value{i}_var').get()
                torque_val = float(torque_str)
                update_torque_bar(
                    shared_gui_refs.get(f'injector_torque_canvas{i}'),
                    shared_gui_refs.get(f'injector_torque_bar{i}'),
                    shared_gui_refs.get(f'injector_torque_text{i}'),
                    torque_val
                )
            except (ValueError, AttributeError, tk.TclError):
                continue

    def update_fillhead_torque_bars():
        for i in range(4):
            try:
                torque_str = shared_gui_refs.get(f'fh_torque_m{i}_var').get()
                torque_val = float(torque_str)
                update_torque_bar(
                    shared_gui_refs.get(f'fh_torque_canvas{i}'),
                    shared_gui_refs.get(f'fh_torque_bar{i}'),
                    shared_gui_refs.get(f'fh_torque_text{i}'),
                    torque_val
                )
            except (ValueError, AttributeError, tk.TclError):
                continue

    def update_fillhead_position_plot():
        canvas = shared_gui_refs.get('fh_pos_viz_canvas')
        xy_marker = shared_gui_refs.get('fh_xy_marker')
        z_bar = shared_gui_refs.get('fh_z_bar')
        if not all([canvas, xy_marker, z_bar]): return
        try:
            x_pos = float(shared_gui_refs.get('fh_pos_m0_var').get())
            y_pos = float(shared_gui_refs.get('fh_pos_m1_var').get())
            z_pos = float(shared_gui_refs.get('fh_pos_m3_var').get())
            xy_marker.set_data([x_pos], [y_pos])
            z_bar.set_height(z_pos)
            canvas.draw_idle()
        except (ValueError, tk.TclError):
            pass

    def periodic_gui_updater():
        update_injector_torque_bars()
        update_fillhead_torque_bars()
        root.after(GUI_UPDATE_INTERVAL_MS, periodic_gui_updater)

    def periodic_plot_updater():
        update_fillhead_position_plot()
        root.after(PLOT_UPDATE_INTERVAL_MS, periodic_plot_updater)

    def telemetry_requester_loop():
        if comms.devices["injector"]["connected"]:
            comms.send_to_device("injector", "REQUEST_TELEM", shared_gui_refs)
        if comms.devices["fillhead"]["connected"]:
            comms.send_to_device("fillhead", "REQUEST_TELEM", shared_gui_refs)
        root.after(GUI_UPDATE_INTERVAL_MS, telemetry_requester_loop)

    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()

    root.after(250, periodic_gui_updater)
    root.after(250, periodic_plot_updater)
    root.after(1000, telemetry_requester_loop)

    root.mainloop()


if __name__ == "__main__":
    main()