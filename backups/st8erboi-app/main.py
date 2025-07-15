import tkinter as tk
from tkinter import ttk
import threading
import time
import datetime
import math
# Matplotlib imports are removed as they are not used in the current implementation
# import matplotlib.pyplot as plt
# from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
# import numpy as np

import comms
from shared_gui import create_shared_widgets
from injector_gui import create_injector_tab
from fillhead_gui import create_fillhead_tab
from peer_comms_gui import create_peer_comms_tab
from scripting_gui import create_scripting_tab  # Import the new tab

GUI_UPDATE_INTERVAL_MS = 100


# PLOT_UPDATE_INTERVAL_MS = 250 # Plotting is disabled


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
    # Add a style for the yellow button
    style.configure("Yellow.TButton", background='#f2c037', foreground='black', font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Yellow.TButton", background=[('pressed', '#d3a92b'), ('active', '#f5c94c')])


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

    # Create the new scripting tab, passing the shared_gui_refs
    scripting_widgets = create_scripting_tab(notebook, command_funcs, shared_gui_refs)
    shared_gui_refs.update(scripting_widgets)

    peer_comms_widgets = create_peer_comms_tab(notebook, command_funcs)
    shared_gui_refs.update(peer_comms_widgets)

    notebook.pack(expand=True, fill="both", padx=10, pady=5)
    shared_widgets['shared_bottom_frame'].pack(fill=tk.X, expand=False, padx=10, pady=(0, 10))

    def update_torque_bar(canvas, bar_item, text_item, torque_val, max_height=100):
        if not all([canvas, bar_item, text_item]): return

        # Ensure max_height is a positive number to avoid division by zero or negative values
        if max_height <= 20: max_height = 100

        clamped_torque = max(0, min(100, torque_val))
        bar_height = (clamped_torque / 100.0) * (max_height - 20)  # Use the canvas height
        y0 = max_height - 10 - bar_height
        y1 = max_height - 10

        canvas.coords(bar_item, 10, y0, 30, y1)
        canvas.itemconfig(text_item, text=f"{torque_val:.1f}%")

        # Determine color based on torque
        if torque_val > 85:
            color = '#db2828'  # Red
        elif torque_val > 60:
            color = '#f2c037'  # Yellow
        else:
            color = '#21ba45'  # Green
        canvas.itemconfig(bar_item, fill=color)

    def update_injector_torque_bars():
        for i in range(1, 4):
            try:
                torque_str = shared_gui_refs.get(f'torque_value{i}_var').get()
                torque_val = float(torque_str)
                canvas = shared_gui_refs.get(f'injector_torque_canvas{i}')
                update_torque_bar(
                    canvas,
                    shared_gui_refs.get(f'injector_torque_bar{i}'),
                    shared_gui_refs.get(f'injector_torque_text{i}'),
                    torque_val,
                    max_height=canvas.winfo_height()
                )
            except (ValueError, AttributeError, tk.TclError, KeyError):
                continue

    def update_fillhead_torque_bars():
        for i in range(4):
            try:
                torque_str = shared_gui_refs.get(f'fh_torque_m{i}_var').get()
                torque_val = float(torque_str)
                canvas = shared_gui_refs.get(f'fh_torque_canvas{i}')
                update_torque_bar(
                    canvas,
                    shared_gui_refs.get(f'fh_torque_bar{i}'),
                    shared_gui_refs.get(f'fh_torque_text{i}'),
                    torque_val,
                    max_height=canvas.winfo_height()
                )
            except (ValueError, AttributeError, tk.TclError, KeyError):
                continue

    def periodic_gui_updater():
        # This function runs periodically to update dynamic UI elements
        # like the torque bars.
        update_injector_torque_bars()
        update_fillhead_torque_bars()
        root.after(GUI_UPDATE_INTERVAL_MS, periodic_gui_updater)

    def telemetry_requester_loop():
        # This function periodically requests new data from the connected devices.
        if comms.devices["injector"]["connected"]:
            comms.send_to_device("injector", "REQUEST_TELEM", shared_gui_refs)
        if comms.devices["fillhead"]["connected"]:
            comms.send_to_device("fillhead", "REQUEST_TELEM", shared_gui_refs)
        root.after(GUI_UPDATE_INTERVAL_MS, telemetry_requester_loop)

    # Start the network communication threads
    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()

    # Start the periodic GUI update loops
    root.after(250, periodic_gui_updater)
    root.after(1000, telemetry_requester_loop)

    root.mainloop()


if __name__ == "__main__":
    main()
