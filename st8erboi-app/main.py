import tkinter as tk
from tkinter import ttk
import threading
import time
import datetime
import math
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import numpy as np

# Import our communications module
import comms

# Import GUI builder functions from their separate files
from shared_gui import create_shared_widgets
from injector_gui import create_injector_tab
from fillhead_gui import create_fillhead_tab


# --- Main Application ---
def main():
    root = tk.Tk()
    root.title("Multi-Device Controller")
    root.configure(bg="#21232b")

    shared_gui_refs = {
        "injector_torque_times": [], "injector_torque_history1": [], "injector_torque_history2": [],
        "injector_torque_history3": [],
        "fillhead_torque_times": [], "fillhead_torque_history0": [], "fillhead_torque_history1": [],
        "fillhead_torque_history2": [], "fillhead_torque_history3": [],
    }

    def get_terminal_cb():
        return shared_gui_refs.get('terminal_cb')

    send_injector_cmd = lambda msg: comms.send_to_device("injector", msg, shared_gui_refs)
    send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, shared_gui_refs)

    def send_global_abort():
        comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", get_terminal_cb())
        send_injector_cmd("ABORT")
        send_fillhead_cmd("ABORT")

    command_funcs = {"abort": send_global_abort, "clear_errors": lambda: send_injector_cmd("CLEAR_ERRORS")}

    # --- Create all GUI elements ---
    shared_widgets = create_shared_widgets(root, command_funcs)
    shared_gui_refs.update(shared_widgets)

    notebook = ttk.Notebook(root)

    injector_widgets = create_injector_tab(notebook, send_injector_cmd, shared_gui_refs)
    shared_gui_refs.update(injector_widgets)

    fillhead_widgets = create_fillhead_tab(notebook, send_fillhead_cmd)
    shared_gui_refs.update(fillhead_widgets)

    notebook.pack(expand=True, fill="both", padx=10, pady=5)
    shared_widgets['shared_bottom_frame'].pack(fill=tk.X, expand=False, padx=10, pady=(0, 10))

    # --- GUI Update Functions ---
    def update_fillhead_torque_plot():
        ax = shared_gui_refs.get('fillhead_torque_plot_ax')
        canvas = shared_gui_refs.get('fillhead_torque_plot_canvas')
        lines = shared_gui_refs.get('fillhead_torque_plot_lines')
        if not all([ax, canvas, lines]): return

        ts = shared_gui_refs['fillhead_torque_times']
        histories = [shared_gui_refs[f'fillhead_torque_history{i}'] for i in range(4)]

        if not ts: return
        try:
            latest_time = ts[-1]
            ax.set_xlim(latest_time - 10, latest_time)

            for i in range(4):
                if len(ts) == len(histories[i]): lines[i].set_data(ts, histories[i])

            all_data = [item for sublist in histories for item in sublist if sublist]
            if not all_data: return

            min_y_data, max_y_data = min(all_data), max(all_data)
            nmin, nmax = max(min_y_data - 10, -10), min(max_y_data + 10, 110)
            ax.set_ylim(nmin, nmax)
            canvas.draw_idle()
        except (IndexError, ValueError):
            pass

    def periodic_gui_updater():
        """This function runs in the main Tkinter thread to safely update plots."""
        # For now, only the fillhead plot is being updated here, but you can add injector plots
        update_fillhead_torque_plot()
        root.after(80, periodic_gui_updater)  # Refresh plots 10 times per second

    # --- Timed Telemetry Requester ---
    def telemetry_requester_loop():
        """This function runs in the main GUI thread and sends telemetry requests at a defined interval."""
        interval_ms = 80  # Default safe interval

        # Check for injector
        if comms.devices["injector"]["connected"]:
            # To add user-control for injector, add an entry box and variable like below
            comms.send_to_device("injector", "REQUEST_TELEM", shared_gui_refs)

        # Check for fillhead
        if comms.devices["fillhead"]["connected"]:
            try:
                interval_ms = int(shared_gui_refs['fh_telem_interval_var'].get())
                if interval_ms < 50: interval_ms = 50  # Enforce a minimum of 50ms (20Hz) to be safe
                comms.send_to_device("fillhead", "REQUEST_TELEM", shared_gui_refs)
            except (ValueError, KeyError):
                interval_ms = 250  # Use default if GUI value is invalid

        # Schedule the next run using the determined interval
        root.after(interval_ms, telemetry_requester_loop)

    # --- Start Communication Threads ---
    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()

    # --- Start the main application and timed loops ---
    root.after(250, periodic_gui_updater)
    root.after(1000, telemetry_requester_loop)  # Start the first request after 1 second

    root.mainloop()


if __name__ == "__main__":
    main()