import tkinter as tk
from tkinter import messagebox, scrolledtext
from tkinter import ttk
from tkinter import Menu
import theme


def create_top_menu(parent, file_commands, autosave_var):
    """
    Creates the main application menu bar.
    """
    menubar = Menu(parent, 
                   bg=theme.WIDGET_BG, 
                   fg=theme.FG_COLOR,
                   activebackground=theme.PRIMARY_ACCENT,
                   activeforeground=theme.FG_COLOR,
                   relief=tk.FLAT,
                   bd=0)
    
    # File Menu
    file_menu = Menu(menubar, tearoff=0, 
                     bg=theme.WIDGET_BG, 
                     fg=theme.FG_COLOR,
                     activebackground=theme.PRIMARY_ACCENT,
                     activeforeground=theme.FG_COLOR)
    file_menu.add_command(label="New Script", command=file_commands['new'])
    file_menu.add_command(label="Open Script...", command=file_commands['open'])
    file_menu.add_command(label="Save", command=file_commands['save'])
    file_menu.add_command(label="Save As...", command=file_commands['save_as'])
    file_menu.add_separator(background=theme.WIDGET_BORDER)
    file_menu.add_checkbutton(label="Autosave", onvalue=True, offvalue=False, variable=autosave_var,
                              selectcolor=theme.PRIMARY_ACCENT) # Color for checkmark
    file_menu.add_separator(background=theme.WIDGET_BORDER)
    
    # Recent files submenu
    recent_files_menu = Menu(file_menu, tearoff=0, 
                             bg=theme.WIDGET_BG, 
                             fg=theme.FG_COLOR,
                             activebackground=theme.PRIMARY_ACCENT,
                             activeforeground=theme.FG_COLOR)
    file_menu.add_cascade(label="Recent Files", menu=recent_files_menu)
    file_menu.add_separator(background=theme.WIDGET_BORDER)
    file_menu.add_command(label="Exit", command=parent.quit)
    
    menubar.add_cascade(label="File", menu=file_menu)

    parent.config(menu=menubar)

    return menubar, recent_files_menu


def show_documentation_window(parent):
    """Displays the application's UI structure documentation in a new window."""
    doc_window = tk.Toplevel(parent)
    doc_window.title("Application Documentation")
    doc_window.geometry("800x600")
    doc_window.configure(bg="#2a2d3b")

    text_area = scrolledtext.ScrolledText(doc_window, wrap=tk.WORD, bg="#1b1e2b", fg="white", font=("Consolas", 10))
    text_area.pack(expand=True, fill="both", padx=10, pady=10)

    documentation_content = """
# Application UI Structure (Revised)

This document outlines the hierarchical structure of the Tkinter UI elements in your application, reflecting the latest refactoring for improved modularity and clarity.

---

### `main.py` - The Root and UI Orchestrator

This file initializes the main application window and is now responsible for creating and placing all major UI components.

- **`root` (`tk.Tk`)**: The main window of the application.
  - **`Top Menu Bar`** (Created by `create_top_menu` from `top_menu.py`)
  - **`left_bar_frame` (`tk.Frame`)**: A vertical container on the left side.
    - **Status Panel** (Created by `create_status_bar` from `status_panel.py`)
    - **Manual Controls Display** (Created by `create_manual_controls_display` from `manual_controls.py`)
    - **Global Abort Button** (`tk.Button`)
  - **`terminal_frame` (`tk.Frame`)**: A container at the bottom of the window.
    - (Contents created by `create_terminal_panel` from `terminal.py`)
  - **`main_content_frame` (`tk.Frame`)**: The central area that fills the remaining space.
    - **Scripting Interface** (Created by `create_scripting_interface` from `scripting_gui.py`)

---

### `top_menu.py` - The Main Menu Bar

This file creates the application's main menu bar (File, Help, etc.).

- **`create_top_menu()`**:
  - `menubar` (`tk.Menu`)
    - `file_menu` (`tk.Menu`): Contains New, Load, Save, Exit commands.
    - `help_menu` (`tk.Menu`): Contains Documentation and About commands.

---

### `status_panel.py` - Left-Side Status Displays

This file's role is unchanged. It creates the informational panels displayed at the top of the `left_bar_frame`.

- **`create_status_bar()`**:
  - `status_bar_container` (`tk.Frame`)
    - `conn_frame` (`tk.LabelFrame`): Connection status.
    - `fh_frame` (`tk.LabelFrame`): Fillhead position readouts.
    - `sys_status_frame` (`tk.LabelFrame`): Consolidated system status.

---

### `manual_controls.py` - Consolidated Manual Controls

This file consolidates all manual control widgets for both devices into a single, cohesive module.

- **`create_manual_controls_display()`**:
  - `container` (`tk.Frame`)
    - `toggle_button` (`ttk.Button`): Shows/hides the controls.
    - `manual_panel` (`tk.Frame`): The collapsible, scrollable area.
      - **Motor Power Controls** (`create_motor_power_controls()`)
      - **Injector Ancillary Controls** (`create_injector_ancillary_controls()`)
      - **Fillhead Ancillary Controls** (`create_fillhead_ancillary_controls()`)

---

### `scripting_gui.py` - Dedicated Scripting Interface

This file's responsibility has been focused. It now exclusively creates the scripting editor and the command reference panel.

- **`create_scripting_interface()`**:
  - `scripting_area` (`tk.Frame`)
    - `paned_window` (`ttk.PanedWindow`): Splits the area into two resizable panes.
      - **`left_pane` (`tk.Frame`)**: Contains the script editor and its control buttons.
      - **`right_pane` (`tk.Frame`)**: Contains the command reference tree view.

---

### `terminal.py` - The Bottom Terminal Panel

This file's role is unchanged. It defines the terminal output window and its associated options.

- **`create_terminal_panel()`**:
  - `bottom_frame` (`tk.Frame`): The main container for the terminal area.
    - `terminal` (`tk.Text`) and `scrollbar` (`ttk.Scrollbar`).
    - `options_frame` (`tk.Frame`) with `Checkbuttons`.
    """
    text_area.insert(tk.END, documentation_content)
    text_area.config(state=tk.DISABLED)
    doc_window.transient(parent)
    doc_window.grab_set()


def show_about_window(parent):
    """Displays the 'About' window with version and author information."""
    messagebox.showinfo(
        "About Multi-Device Controller",
        "Multi-Device Controller\n\n"
        "Version: 1.1.0\n"
        "Release Date: July 18, 2025\n\n"
        "Author: Gemini\n"
        "Location: Victoria, BC, Canada",
        parent=parent
    )
