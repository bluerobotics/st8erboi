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
from peer_comms_gui import create_peer_comms_tab

# --- CONFIGURATION CONSTANT ---
GUI_UPDATE_INTERVAL_MS = 100


# --- UPDATED: More robust style configuration ---
def configure_styles():
    """Defines custom colors and styles for ttk widgets."""
    style = ttk.Style()

    # Use the 'clam' theme, which is known to be more customizable for colors.
    try:
        style.theme_use('clam')
    except tk.TclError:
        print("Warning: 'clam' theme not available. Using default.")

    # A helper to remove the dotted focus outline on buttons
    style.layout('TButton',
                 [('Button.button', {'children':
                                         [('Button.focus', {'children':
                                                                [('Button.padding', {'children':
                                                                                         [('Button.label',
                                                                                           {'side': 'left',
                                                                                            'expand': 1})]
                                                                                     })]
                                                            })]
                                     })]
                 )

    # Style for a button that is currently "ON" or "ACTIVE"
    style.configure("Green.TButton",
                    background='#21ba45',
                    foreground='white',
                    font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Green.TButton",
              background=[('pressed', '#198734'), ('active', '#25cc4c')])

    # Style for a button that indicates the "OFF" state
    style.configure("Red.TButton",
                    background='#db2828',
                    foreground='white',
                    font=('Segoe UI', 9, 'bold'),
                    borderwidth=0)
    style.map("Red.TButton",
              background=[('pressed', '#b32424'), ('active', 'red')])

    # Style for a neutral, unselected button in a group
    style.configure("Neutral.TButton",
                    background='#495057',
                    foreground='#f8f9fa',
                    borderwidth=0)
    style.map("Neutral.TButton",
              background=[('pressed', '#343a40'), ('active', '#5a6268')])


# --- Main Application ---
def main():
    root = tk.Tk()
    root.title("Multi-Device Controller")
    root.configure(bg="#21232b")

    configure_styles()

    shared_gui_refs = {
        "injector_torque_times": [], "injector_torque_history1": [], "injector_torque_history2": [],
        "injector_torque_history3": [],
        "fillhead_torque_times": [], "fillhead_torque_history0": [], "fillhead_torque_history1": [],
        "fillhead_torque_history2": [], "fillhead_torque_history3": [],
    }

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

    def update_injector_torque_plot():
        ax = shared_gui_refs.get('injector_torque_plot_ax')
        canvas = shared_gui_refs.get('injector_torque_plot_canvas')
        lines = shared_gui_refs.get('injector_torque_plot_lines')
        if not all([ax, canvas, lines]): return
        ts = shared_gui_refs.get('injector_torque_times', [])
        histories = [
            shared_gui_refs.get("injector_torque_history1", []),
            shared_gui_refs.get("injector_torque_history2", []),
            shared_gui_refs.get("injector_torque_history3", [])
        ]
        if not ts: return
        try:
            latest_time = ts[-1]
            ax.set_xlim(latest_time - 10, latest_time)
            for i in range(3):
                if len(ts) == len(histories[i]):
                    lines[i].set_data(ts, histories[i])
            all_data = [item for sublist in histories for item in sublist if sublist]
            if not all_data: return
            min_y_data, max_y_data = min(all_data), max(all_data)
            nmin, nmax = max(min_y_data - 10, -10), min(max_y_data + 10, 110)
            ax.set_ylim(nmin, nmax)
            canvas.draw_idle()
        except (IndexError, ValueError):
            pass

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
        update_injector_torque_plot()
        update_fillhead_torque_plot()
        update_fillhead_position_plot()
        root.after(GUI_UPDATE_INTERVAL_MS, periodic_gui_updater)

    def telemetry_requester_loop():
        if comms.devices["injector"]["connected"]:
            comms.send_to_device("injector", "REQUEST_TELEM", shared_gui_refs)
        if comms.devices["fillhead"]["connected"]:
            comms.send_to_device("fillhead", "REQUEST_TELEM", shared_gui_refs)
        root.after(GUI_UPDATE_INTERVAL_MS, telemetry_requester_loop)

    threading.Thread(target=comms.recv_loop, args=(shared_gui_refs,), daemon=True).start()
    threading.Thread(target=comms.monitor_connections, args=(shared_gui_refs,), daemon=True).start()
    root.after(250, periodic_gui_updater)
    root.after(1000, telemetry_requester_loop)
    root.mainloop()


if __name__ == "__main__":
    main()