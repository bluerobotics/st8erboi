import tkinter as tk
from tkinter import ttk
import threading
import comms
from scripting_gui import create_scripting_interface, create_command_reference # Import both functions
from status_panel import create_status_bar
from terminal import create_terminal_panel
import json
import os
import theme  # Import the new theme file
import tkinter.font as tkfont
import platform
import ctypes

# Import GUI components
from top_menu import create_top_menu

GUI_UPDATE_INTERVAL_MS = 100

class CollapsiblePanel(ttk.Frame):
    """A collapsible panel with a side trigger bar."""
    def __init__(self, parent, text="Controls", width=350, **kwargs):
        super().__init__(parent, style='TFrame', **kwargs)
        self.text = text
        self.width = width
        self.is_collapsed = True  # start collapsed by default

        self.trigger_canvas = tk.Canvas(self, width=30, bg=theme.SECONDARY_ACCENT, highlightthickness=0)
        self.trigger_canvas.pack(side=tk.LEFT, fill=tk.Y)

        self.content_panel = ttk.Frame(self, width=self.width, style='TFrame')
        # Start collapsed: don't pack content
        self.content_panel.pack_propagate(False)
        self.configure(width=int(self.trigger_canvas.cget('width')))

        self.draw_trigger_text()
        self.trigger_canvas.bind("<Button-1>", self.toggle_panel)
        self.trigger_canvas.bind("<Enter>", lambda e: self.trigger_canvas.config(bg=theme.PRIMARY_ACCENT))
        self.trigger_canvas.bind("<Leave>", lambda e: self.trigger_canvas.config(bg=theme.SECONDARY_ACCENT))

    def get_content_frame(self):
        return self.content_panel

    def draw_trigger_text(self):
        self.trigger_canvas.delete("all")
        display_text = "Show " + self.text if self.is_collapsed else "Hide " + self.text
        self.trigger_canvas.create_text(15, 150, text=display_text, angle=90, font=theme.FONT_BOLD, fill=theme.FG_COLOR, anchor="center")

    def _update_parent_sash(self):
        """If this panel is in a Panedwindow, update the sash position."""
        self.update_idletasks()
        try:
            pw = self.master
            if isinstance(pw, ttk.Panedwindow):
                panes = pw.panes()
                if str(self) in panes:
                    sash_index = panes.index(str(self)) - 1
                    if sash_index >= 0:
                        trigger_width = self.trigger_canvas.winfo_width()
                        total_width = pw.winfo_width()
                        
                        if self.is_collapsed:
                            # Move sash to make this pane only trigger_width wide
                            target_pos = total_width - trigger_width
                            # Add a small buffer for the sash itself
                            if target_pos < total_width - 5:
                                target_pos -= 5
                            pw.sashpos(sash_index, target_pos)
                        else:
                            # Move sash to make this pane its full configured width
                            target_pos = total_width - self.width
                            pw.sashpos(sash_index, target_pos)
        except Exception:
            pass # Fail silently

    def toggle_panel(self, event=None):
        if self.is_collapsed:
            # Expand to configured width
            self.content_panel.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            self.configure(width=self.width)
        else:
            # Collapse to trigger width
            self.content_panel.pack_forget()
            self.configure(width=int(self.trigger_canvas.cget('width')))
        self.is_collapsed = not self.is_collapsed
        self.draw_trigger_text()
        self.after(10, self._update_parent_sash) # Let geometry manager update before moving sash

class MainApplication:
    def __init__(self, root):
        self.root = root
        self.root.title("st8erboi controller")
        self.root.configure(bg=theme.BG_COLOR)

        # Configure ttk styles to match the theme
        self.style = ttk.Style()
        self.style.theme_use('clam') # Use a theme that allows full color customization

        # General widget styling
        self.style.configure('.', background=theme.BG_COLOR, foreground=theme.FG_COLOR, font=theme.FONT_NORMAL)
        self.style.map('.', background=[('active', theme.SECONDARY_ACCENT)])

        # Frame styling
        self.style.configure('TFrame', background=theme.BG_COLOR)

        # Label styling
        self.style.configure('TLabel', background=theme.BG_COLOR, foreground=theme.FG_COLOR, font=theme.FONT_NORMAL)
        self.style.configure('Header.TLabel', font=theme.FONT_BOLD)

        # Button styling
        self.style.configure('TButton', background=theme.WIDGET_BG, foreground=theme.FG_COLOR, borderwidth=1, focusthickness=3, focuscolor=theme.PRIMARY_ACCENT)
        self.style.map('TButton',
            background=[('active', theme.PRIMARY_ACCENT)],
            foreground=[('active', theme.FG_COLOR)]
        )

        # Entry styling
        self.style.configure('TEntry', fieldbackground=theme.WIDGET_BG, foreground=theme.FG_COLOR, insertcolor=theme.FG_COLOR)
        
        # --- Custom Button Styles ---
        self.style.configure('Green.TButton', background=theme.SUCCESS_GREEN, foreground='black', font=theme.FONT_BOLD, borderwidth=1, bordercolor=theme.SUCCESS_GREEN)
        self.style.map('Green.TButton', 
            background=[('pressed', theme.PRESSED_GREEN), ('active', theme.ACTIVE_GREEN)],
            foreground=[('pressed', 'black'), ('active', 'black')],
            relief=[('pressed', 'sunken'), ('active', 'raised')],
            bordercolor=[('active', theme.FG_COLOR), ('!active', theme.SUCCESS_GREEN)]
        )
        # New style for when the script is actively running (button is disabled)
        self.style.configure('Running.Green.TButton', font=theme.FONT_BOLD, borderwidth=1, bordercolor=theme.RUNNING_GREEN)
        self.style.map('Running.Green.TButton', 
            foreground=[('disabled', 'white')],
            background=[('disabled', theme.RUNNING_GREEN)]
        )

        self.style.configure('Red.TButton', background=theme.ERROR_RED, foreground='black', font=theme.FONT_BOLD, borderwidth=1, bordercolor=theme.ERROR_RED)
        self.style.map('Red.TButton', 
            background=[('pressed', theme.PRESSED_RED), ('active', theme.ACTIVE_RED)],
            foreground=[('pressed', 'black'), ('active', 'black')],
            relief=[('pressed', 'sunken'), ('active', 'raised')],
            bordercolor=[('active', theme.FG_COLOR), ('!active', theme.ERROR_RED)]
        )
        # New style for when the script is held
        self.style.configure('Holding.Red.TButton', background=theme.HOLDING_RED, foreground='white', font=theme.FONT_BOLD, borderwidth=1, bordercolor=theme.HOLDING_RED)
        self.style.map('Holding.Red.TButton', 
            background=[('pressed', theme.PRESSED_HOLDING_RED), ('active', theme.ACTIVE_HOLDING_RED)],
            foreground=[('pressed', 'white'), ('active', 'white')],
            relief=[('pressed', 'sunken'), ('active', 'raised')],
            bordercolor=[('active', theme.FG_COLOR), ('!active', theme.HOLDING_RED)]
        )
        
        self.style.configure('Small.TButton', font=theme.FONT_NORMAL)

        # Custom style for the toggle-like Checkbutton
        self.style.configure('OrangeToggle.TButton', font=theme.FONT_BOLD)
        self.style.map('OrangeToggle.TButton',
            foreground=[('selected', 'black'), ('!selected', theme.FG_COLOR)],
            background=[
                ('selected', 'pressed', theme.PRESSED_ORANGE),
                ('selected', 'active', theme.ACTIVE_ORANGE),
                ('selected', theme.PRESSED_ORANGE), # Make the "on" state darker
                ('!selected', 'pressed', theme.PRESSED_GRAY),
                ('!selected', 'active', theme.SECONDARY_ACCENT),
                ('!selected', theme.WIDGET_BG)
            ],
            relief=[('pressed', 'sunken'), ('active', 'raised')]
        )

        # Blue accent button used for utility actions
        self.style.configure('Blue.TButton', background=theme.PRIMARY_ACCENT, foreground='black', font=theme.FONT_BOLD, borderwidth=1, bordercolor=theme.PRIMARY_ACCENT)
        self.style.map('Blue.TButton', 
            background=[('pressed', theme.PRESSED_BLUE), ('active', theme.ACTIVE_BLUE)],
            foreground=[('pressed', 'black'), ('active', 'black')],
            relief=[('pressed', 'sunken'), ('active', 'raised')],
            bordercolor=[('active', theme.FG_COLOR), ('!active', theme.PRIMARY_ACCENT)]
        )

        # Ghost/neutral button for low-emphasis actions
        self.style.configure('Ghost.TButton', background=theme.WIDGET_BG, foreground=theme.FG_COLOR, borderwidth=1)
        self.style.map('Ghost.TButton', background=[('active', theme.SECONDARY_ACCENT)])

        # Additional accent buttons for differentiation
        self.style.configure('Yellow.TButton', background=theme.WARNING_YELLOW, foreground='black', font=theme.FONT_BOLD)
        self.style.map('Yellow.TButton', background=[('active', '#F0CD8C')])
        self.style.configure('Gray.TButton', background=theme.SECONDARY_ACCENT, foreground=theme.FG_COLOR, font=theme.FONT_BOLD)
        self.style.map('Gray.TButton', background=[('active', '#505868')])

        # Card-like containers and subtle labels
        self.style.configure('Card.TLabelframe', background=theme.CARD_BG, foreground=theme.FG_COLOR)
        self.style.configure('Card.TLabelframe.Label', background=theme.CARD_BG, foreground=theme.FG_COLOR, font=theme.FONT_BOLD)
        self.style.configure('Card.TFrame', background=theme.CARD_BG)
        self.style.configure('Subtle.TLabel', background=theme.CARD_BG, foreground=theme.COMMENT_COLOR, font=theme.FONT_NORMAL)
        self.style.configure('CardBorder.TFrame', background=theme.SECONDARY_ACCENT)

        # Custom Progress Bar (for torque meters)
        self.style.configure('Card.Vertical.TProgressbar', background=theme.PRIMARY_ACCENT, troughcolor=theme.CARD_BG)

        # Treeview (used in command reference)
        self.style.configure("Treeview",
            background=theme.WIDGET_BG,
            foreground=theme.FG_COLOR,
            fieldbackground=theme.WIDGET_BG,
            borderwidth=1
        )
        self.style.map("Treeview",
            background=[('selected', theme.SELECTION_BG)],
            foreground=[('selected', theme.SELECTION_FG)]
        )
        self.style.map("Treeview.Heading",
            background=[('active', theme.PRIMARY_ACCENT), ('!active', theme.SECONDARY_ACCENT)],
            foreground=[('active', theme.FG_COLOR), ('!active', theme.FG_COLOR)]
        )

        # Paned Window Separator
        self.style.configure('TPanedwindow', background=theme.BG_COLOR)
        self.style.configure('TPanedwindow.Sash', background=theme.SECONDARY_ACCENT, sashthickness=6)

        self.root.state('zoomed') # Start maximized

        self.autosave_var = tk.BooleanVar(value=True)
        
        # Initialize the status variable first, as other components need it.
        self.status_var = tk.StringVar(value="Initializing...")

        self.shared_gui_refs = self.initialize_shared_variables()
        
        # Command functions are now initialized and added within initialize_shared_variables
        self.command_funcs = self.shared_gui_refs['command_funcs']
        
        self.create_widgets()
        self.load_last_script()

    def initialize_shared_variables(self):
        """
        Initializes the shared state and functions for the application.
        Now also handles the creation of command functions to ensure correct scope.
        """
        # --- Shared State and Functions ---
        shared_gui_refs = {
            # --- Connection and Global State ---
            'status_var_fillhead': tk.StringVar(value='🔌 Fillhead Disconnected'),
            'status_var_gantry': tk.StringVar(value='🔌 Gantry Disconnected'),
            'main_state_var': tk.StringVar(value='---'),
            'feed_state_var': tk.StringVar(value='---'),
            'error_state_var': tk.StringVar(value='No Error'),

            # --- Fillhead Enabled States ---
            'enabled_state1_var': tk.StringVar(value='Disabled'),
            'enabled_state2_var': tk.StringVar(value='Disabled'),
            'enabled_state3_var': tk.StringVar(value='Disabled'),

            # --- Fillhead Position, Homing, and Dispensing ---
            'pos_mm0_var': tk.StringVar(value='---'),
            'pos_mm1_var': tk.StringVar(value='---'),
            'homed0_var': tk.StringVar(value='Not Homed'),
            'homed1_var': tk.StringVar(value='Not Homed'),
            'machine_steps_var': tk.StringVar(value='---'),
            'cartridge_steps_var': tk.StringVar(value='---'),
            'heater_mode_var': tk.StringVar(value='---'),
            'vacuum_state_var': tk.StringVar(value='---'),
            'inject_cumulative_ml_var': tk.StringVar(value='---'),
            'inject_active_ml_var': tk.StringVar(value='---'),
            'total_dispensed_var': tk.StringVar(value='--- ml'),
            'cycle_dispensed_var': tk.StringVar(value='--- / --- ml'),
            'injection_target_ml_var': tk.StringVar(value='---'),

            # --- Fillhead Component States ---
            'fillhead_injector_state_var': tk.StringVar(value='---'),
            'fillhead_inj_valve_state_var': tk.StringVar(value='---'),
            'fillhead_vac_valve_state_var': tk.StringVar(value='---'),
            'fillhead_heater_state_var': tk.StringVar(value='---'),
            'fillhead_vacuum_state_var': tk.StringVar(value='---'),

            # --- Fillhead Torque Values ---
            'torque0_var': tk.DoubleVar(value=0.0),
            'torque1_var': tk.DoubleVar(value=0.0),
            'torque2_var': tk.DoubleVar(value=0.0),
            'torque3_var': tk.DoubleVar(value=0.0),

            # --- Gantry Enabled States ---
            'fh_enabled_m0_var': tk.StringVar(value='Disabled'),
            'fh_enabled_m1_var': tk.StringVar(value='Disabled'),
            'fh_enabled_m2_var': tk.StringVar(value='Disabled'),
            'fh_enabled_m3_var': tk.StringVar(value='Disabled'),

            # --- Gantry Position and Homing ---
            'fh_pos_m0_var': tk.StringVar(value='---'),
            'fh_pos_m1_var': tk.StringVar(value='---'),
            'fh_pos_m2_var': tk.StringVar(value='---'),
            'fh_pos_m3_var': tk.StringVar(value='---'),
            'fh_homed_m0_var': tk.StringVar(value='Not Homed'),
            'fh_homed_m1_var': tk.StringVar(value='Not Homed'),
            'fh_homed_m2_var': tk.StringVar(value='Not Homed'),
            'fh_homed_m3_var': tk.StringVar(value='Not Homed'),

            # --- Gantry Axis States ---
            'fh_state_x_var': tk.StringVar(value='---'),
            'fh_state_y_var': tk.StringVar(value='---'),
            'fh_state_z_var': tk.StringVar(value='---'),
            'fh_state_var': tk.StringVar(value='---'), # Used by status_panel

            # --- Gantry Torque Values ---
            'fh_torque_m0_var': tk.DoubleVar(value=0.0),
            'fh_torque_m1_var': tk.DoubleVar(value=0.0),
            'fh_torque_m2_var': tk.DoubleVar(value=0.0),
            'fh_torque_m3_var': tk.DoubleVar(value=0.0),

            # --- Fillhead Pinch Valve Variables ---
            'inj_valve_pos_var': tk.StringVar(value='---'),
            'inj_valve_homed_var': tk.StringVar(value='Not Homed'),
            'vac_valve_pos_var': tk.StringVar(value='---'),
            'vac_valve_homed_var': tk.StringVar(value='Not Homed'),

            # --- System Status Variables (Heater, Vacuum, etc.) ---
            'vacuum_psig_var': tk.StringVar(value='0.00 PSIG'),
            'temp_c_var': tk.StringVar(value='0.0 °C'),
            'pid_setpoint_var': tk.StringVar(value='25.0'), # Default setpoint
            'heater_display_var': tk.StringVar(value='--- / --- °C'),

            # Heater variables
            'heater_setpoint_var': tk.StringVar(value='70.0'),
            'heater_kp_var': tk.StringVar(value='60.0'),
            'heater_ki_var': tk.StringVar(value='2.5'),
            'heater_kd_var': tk.StringVar(value='40.0'),

            # Vacuum variables
            'vac_target_var': tk.StringVar(value='-14.0'),
            'vac_timeout_var': tk.StringVar(value='30'),
            'vac_leak_delta_var': tk.StringVar(value='0.1'),
            'vac_leak_duration_var': tk.StringVar(value='10')
        }

        # --- System Status Variables (Heater, Vacuum, etc.) ---
        shared_gui_refs.update({
            "reset_and_hide_panel": self.reset_and_hide_panel,
            "show_panel": self.show_panel
        })

        # Define the send functions first, using the dictionary that will be passed around.
        # This makes the reference live, so additions to it (like terminal_cb) are seen.
        send_fillhead_cmd = lambda msg: comms.send_to_device("fillhead", msg, shared_gui_refs)
        send_gantry_cmd = lambda msg: comms.send_to_device("gantry", msg, shared_gui_refs)

        def send_global_abort():
            if 'terminal_cb' in shared_gui_refs:
                comms.log_to_terminal("--- GLOBAL ABORT TRIGGERED ---", shared_gui_refs.get('terminal_cb'))
            send_fillhead_cmd("ABORT")
            send_gantry_cmd("ABORT")

        # Create the command functions and add them directly to the dictionary.
        command_funcs = {
            "abort": send_global_abort,
            "clear_errors": lambda: send_fillhead_cmd("CLEAR_ERRORS"),
            "send_fillhead": send_fillhead_cmd,
            "send_gantry": send_gantry_cmd
        }
        shared_gui_refs['command_funcs'] = command_funcs
        
        # --- NEW: Combined Heater Display Logic ---
        def update_heater_display(*args):
            try:
                temp_val = shared_gui_refs['temp_c_var'].get().split()[0]
                
                heater_state = shared_gui_refs['fillhead_heater_state_var'].get().upper()
                is_on = "ON" in heater_state or "ACTIVE" in heater_state

                if is_on:
                    setpoint_val = shared_gui_refs['pid_setpoint_var'].get()
                else:
                    setpoint_val = "---"

                shared_gui_refs['heater_display_var'].set(f"{temp_val} / {setpoint_val} °C")
            except (IndexError, ValueError, tk.TclError):
                # Handle cases where vars are not yet valid floats or are empty
                shared_gui_refs['heater_display_var'].set("--- / --- °C")

        shared_gui_refs['temp_c_var'].trace_add("write", update_heater_display)
        shared_gui_refs['pid_setpoint_var'].trace_add("write", update_heater_display)
        shared_gui_refs['fillhead_heater_state_var'].trace_add("write", update_heater_display)

        def update_total_dispensed(*args):
            try:
                total_val = shared_gui_refs['inject_cumulative_ml_var'].get().split()[0]
                shared_gui_refs['total_dispensed_var'].set(f"{total_val} ml")
            except (IndexError, ValueError, tk.TclError):
                shared_gui_refs['total_dispensed_var'].set("--- ml")

        shared_gui_refs['inject_cumulative_ml_var'].trace_add("write", update_total_dispensed)

        def update_cycle_dispensed(*args):
            try:
                active_val = shared_gui_refs['inject_active_ml_var'].get().split()[0]
                target_val = shared_gui_refs['injection_target_ml_var'].get()
                if target_val == '---':
                    shared_gui_refs['cycle_dispensed_var'].set(f"{active_val} / --- ml")
                else:
                    shared_gui_refs['cycle_dispensed_var'].set(f"{active_val} / {float(target_val):.2f} ml")
            except (IndexError, ValueError, tk.TclError):
                shared_gui_refs['cycle_dispensed_var'].set("--- / --- ml")

        shared_gui_refs['inject_active_ml_var'].trace_add("write", update_cycle_dispensed)
        shared_gui_refs['injection_target_ml_var'].trace_add("write", update_cycle_dispensed)

        return shared_gui_refs

    def reset_and_hide_panel(self, device_key):
        """Resets all associated variables for a device and hides its panel."""
        # Hide the panel first
        panel = self.shared_gui_refs.get(f'{device_key}_panel')
        if panel:
            panel.pack_forget()

        # Find all variables associated with the device based on a prefix
        # This is a bit of a simplification. A more robust solution might
        # have a predefined map of which variables belong to which device.
        prefix_map = {
            "fillhead": ["status_var_fillhead", "main_state_var", "feed_state_var", "error_state_var",
                         "enabled_state1_var", "enabled_state2_var", "enabled_state3_var", "pos_mm0_var",
                         "pos_mm1_var", "homed0_var", "homed1_var", "machine_steps_var", "cartridge_steps_var",
                         "heater_mode_var", "vacuum_state_var", "inject_cumulative_ml_var", "inject_active_ml_var",
                         "total_dispensed_var", "cycle_dispensed_var", "injection_target_ml_var",
                         "fillhead_injector_state_var", "fillhead_inj_valve_state_var",
                         "fillhead_vac_valve_state_var", "fillhead_heater_state_var", "fillhead_vacuum_state_var",
                         "torque0_var", "torque1_var", "torque2_var", "torque3_var", "inj_valve_pos_var",
                         "inj_valve_homed_var", "vac_valve_pos_var", "vac_valve_homed_var", "vacuum_psig_var",
                         "temp_c_var", "pid_setpoint_var", "heater_display_var"],
            "gantry": ["status_var_gantry", "fh_enabled_m0_var", "fh_enabled_m1_var", "fh_enabled_m2_var",
                       "fh_enabled_m3_var", "fh_pos_m0_var", "fh_pos_m1_var", "fh_pos_m2_var", "fh_pos_m3_var",
                       "fh_homed_m0_var", "fh_homed_m1_var", "fh_homed_m2_var", "fh_homed_m3_var", "fh_state_x_var",
                       "fh_state_y_var", "fh_state_z_var", "fh_state_var", "fh_torque_m0_var", "fh_torque_m1_var",
                       "fh_torque_m2_var", "fh_torque_m3_var"]
        }

        vars_to_reset = prefix_map.get(device_key, [])
        for var_name in vars_to_reset:
            var = self.shared_gui_refs.get(var_name)
            if var:
                if isinstance(var, tk.StringVar):
                    # Reset based on type
                    if "homed" in var_name:
                        var.set("Not Homed")
                    elif "enabled" in var_name:
                        var.set("Disabled")
                    elif "psig" in var_name:
                        var.set("0.00 PSIG")
                    elif "°C" in var_name:
                        var.set("0.0 °C")
                    elif "ml" in var_name:
                        var.set("--- ml")
                    elif "status_var" in var_name:
                        var.set(f"🔌 {device_key.capitalize()} Disconnected")
                    else:
                        var.set("---")
                elif isinstance(var, tk.DoubleVar):
                    var.set(0.0)

    def show_panel(self, device_key):
        """Makes a device's status panel visible."""
        panel = self.shared_gui_refs.get(f'{device_key}_panel')
        if panel:
            panel.pack(side=tk.TOP, fill="x", expand=True, pady=(0, 8))

    def initialize_command_functions(self):
        # This method is now obsolete and can be removed.
        # Its logic has been merged into initialize_shared_variables.
        pass

    def setup_styles(self):
        """
        Configures the application's styles.
        """
        # The styles are now configured in __init__
        pass

    def create_widgets(self):
        """
        Creates the main UI layout and populates it with components.
        """
        # --- Main Layout Frames ---
        main_frame = ttk.Frame(self.root, style='TFrame')
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Create a horizontal splitter for center content and commands (resizable)
        splitter = ttk.Panedwindow(main_frame, orient=tk.HORIZONTAL, style='TPanedwindow')
        splitter.pack(fill=tk.BOTH, expand=True, pady=10, padx=10)

        # Central container for the main content (left pane of splitter)
        center_container = ttk.Frame(splitter, style='TFrame')
        splitter.add(center_container, weight=1)
        
        # Left-side container for the status bar
        left_bar_frame = ttk.Frame(center_container, width=350, style='TFrame')
        left_bar_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(10, 0), pady=10)
        left_bar_frame.pack_propagate(False)

        # Main content area (scripting, console)
        main_content_frame = ttk.Frame(center_container, style='TFrame')
        main_content_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        terminal_widgets = create_terminal_panel(main_content_frame, self.shared_gui_refs)
        self.shared_gui_refs.update(terminal_widgets)
        terminal_widgets['terminal_frame'].pack(side=tk.BOTTOM, fill=tk.X, expand=False, pady=(0, 10))


        # --- Populate UI Components ---
        # Commands panel lives in the splitter (right pane), resizable
        cmd_ref_collapsible = CollapsiblePanel(splitter, text="Commands", width=560)
        splitter.add(cmd_ref_collapsible) # Add the pane
        cmd_ref_collapsible.get_content_frame().pack_propagate(True)

        def set_initial_sash_pos(event=None):
            # This function runs once after the window is drawn to set the sash.
            # We unbind immediately to ensure it's not called again on resize.
            splitter.unbind("<Configure>")

            def position_sash():
                """Calculates and sets the sash position."""
                trigger_width = int(cmd_ref_collapsible.trigger_canvas.cget('width'))
                splitter_width = splitter.winfo_width()
                # Position sash so the right pane is only trigger_width wide,
                # leaving a few pixels for the sash handle itself.
                target_pos = splitter_width - (trigger_width + 5)
                if target_pos > 0:
                    splitter.sashpos(0, target_pos)

            # Schedule this to run after a short delay. This allows the Tkinter
            # event loop to process all initial geometry calculations, ensuring
            # winfo_width() returns a correct, stable value.
            splitter.after(10, position_sash)

        # Bind to the splitter's configure event, which fires when it's first sized.
        splitter.bind("<Configure>", set_initial_sash_pos, add="+")

        # Populate the collapsible panels' content frames
        cmd_ref_content = cmd_ref_collapsible.get_content_frame()

        # Create scripting GUI in the main content area
        self.scripting_gui_refs = create_scripting_interface(
            main_content_frame, 
            self.command_funcs, 
            self.shared_gui_refs, 
            self.autosave_var
        )
        
        # Now that the script editor exists, populate the command reference
        command_ref_widget = create_command_reference(cmd_ref_content, self.scripting_gui_refs['script_editor'])
        command_ref_widget.pack(fill=tk.BOTH, expand=True)

        # Populate the left status bar
        status_widgets_dict = create_status_bar(left_bar_frame, self.shared_gui_refs)
        self.shared_gui_refs.update(status_widgets_dict)

        # Hide panels by default
        self.shared_gui_refs['gantry_panel'].pack_forget()
        self.shared_gui_refs['fillhead_panel'].pack_forget()

        # Create Top Menu (and pass it the file commands from the scripting GUI)
        file_commands = self.scripting_gui_refs['file_commands']
        edit_commands = self.scripting_gui_refs['edit_commands']
        self.menubar, self.recent_files_menu = create_top_menu(self.root, file_commands, edit_commands, self.autosave_var)

        # Pass the recent files menu reference to the scripting gui
        self.scripting_gui_refs['update_recent_menu_callback'](self.recent_files_menu)

    def setup_menu(self):
        """
        Sets up the top menu bar.
        """
        pass # This method is not fully implemented in the original file, so it's left empty.

    def on_closing(self):
        """
        Handles the window close event, checking for unsaved changes before exiting.
        """
        # Ask the scripting GUI to check for unsaved changes before closing
        if self.scripting_gui_refs['check_unsaved']():
            self.root.destroy()

    def load_last_script(self):
        """
        Loads the most recently opened script on startup if one exists.
        """
        try:
            with open("recent_files.json", 'r') as f:
                recent_files = json.load(f)
            if recent_files:
                last_script_path = recent_files[0]
                if os.path.exists(last_script_path):
                    # We need to call this after the main loop has started processing events
                    self.root.after(100, lambda: self.scripting_gui_refs['load_specific_script'](last_script_path))
        except (FileNotFoundError, json.JSONDecodeError, IndexError):
            # No recent files, or file is corrupt. Do nothing.
            pass

    def run(self):
        """
        Starts the communication threads and the main event loop.
        """
        # Start the communication threads, calling functions from the comms module
        threading.Thread(target=comms.recv_loop, args=(self.shared_gui_refs,), daemon=True).start()
        threading.Thread(target=comms.monitor_connections, args=(self.shared_gui_refs,), daemon=True).start()
        threading.Thread(target=comms.discovery_loop, args=(self.shared_gui_refs,), daemon=True).start()
        self.root.mainloop()


def main():
    """
    Initializes the main application window, creates the primary UI layout,
    and starts the communication threads.
    """
    root = tk.Tk()
    
    # --- Set Application Icon ---
    try:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        
        # Set the top-left window icon (uses .png)
        png_path = os.path.join(script_dir, 'icon.png')
        if os.path.exists(png_path):
            img = tk.PhotoImage(file=png_path)
            root.tk.call('wm', 'iconphoto', root._w, img)
        else:
             print(f"Could not find icon.png at '{png_path}'.")

        # Set the taskbar icon (requires .ico on Windows)
        if platform.system() == "Windows":
            ico_path = os.path.join(script_dir, 'icon.ico')
            if os.path.exists(ico_path):
                # This is the most reliable way to set the taskbar icon
                root.iconbitmap(ico_path)
                
                # Force Windows to associate the icon with the app
                myappid = u'tekbic.st8erboi.st8erboi-controller.1.0' 
                ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(myappid)
            else:
                print("NOTE: To set the taskbar icon on Windows, 'icon.ico' must exist in the same folder as the script.")

    except Exception as e:
        print(f"An error occurred while setting the icon: {e}")
        
    app = MainApplication(root)
    app.run()


if __name__ == "__main__":
    main()