# Sun-Valley-ttk-theme v2.4.2 (Self-Contained and Corrected)
#
# This version fixes an issue in the underlying Tcl script to ensure
# the theme is created only once, resolving the traceback error.

import tkinter as tk
from tkinter import ttk
import os

_initialized = False
_mode = "light"

# --- Embedded Theme Files ---

_SV_TCL = r"""
package require Tk

namespace eval sv_ttk {
    variable colors
    variable version 2.4.2

    # This proc loads a color file and applies the settings to the theme
    proc set_theme {theme} {
        if {$theme eq "light"} {
            source [file join $::sv_ttk::theme_dir theme light.tcl]
        } else {
            source [file join $::sv_ttk::theme_dir theme dark.tcl]
        }

        # Configure the existing 'sv' theme with the new colors
        ttk::style theme settings sv {
            ttk::style configure . \
                -background $colors(-bg) \
                -darkcolor  $colors(-dark) \
                -foreground $colors(-fg) \
                -lightcolor $colors(-light) \
                -troughcolor $colors(-trough) \
                -borderwidth 1 \
                -focuscolor $colors(-focus) \
                -selectbackground $colors(-select-bg) \
                -selectforeground $colors(-select-fg) \
                -font "TkDefaultFont"
            ttk::style map . -foreground [list disabled $colors(-disable-fg)]
            ttk::style map . -background [list active $colors(-active)]
            ttk::style layout TButton {
                Button.button -children {
                    Button.focus -children {
                        Button.padding -children {
                            Button.label -side left -expand 1
                        }
                    }
                }
            }
            ttk::style configure TButton -padding {10 5} -anchor center
            ttk::style map TButton \
                -background [list !active $colors(-button-bg) active $colors(-button-active-bg)] \
                -lightcolor [list !active $colors(-button-bg) active $colors(-button-active-bg)] \
                -darkcolor  [list !active $colors(-button-bg) active $colors(-button-active-bg)] \
                -bordercolor $colors(-border)
            ttk::style configure Accent.TButton -foreground $colors(-accent-fg)
            ttk::style map Accent.TButton \
                -background [list !active $colors(-accent) active $colors(-accent-active)] \
                -lightcolor [list !active $colors(-accent) active $colors(-accent-active)] \
                -darkcolor  [list !active $colors(-accent) active $colors(-accent-active)]
            ttk::style configure TMenubutton -background $colors(-bg) -padding {10 5}
            ttk::style map TMenubutton -background [list active $colors(-active)]
            ttk::style configure TEntry \
                -fieldbackground $colors(-entry-bg) \
                -lightcolor $colors(-entry-bg) \
                -darkcolor $colors(-entry-bg) \
                -bordercolor $colors(-border) \
                -insertcolor $colors(-fg) \
                -padding {10 5}
            ttk::style map TEntry -foreground [list disabled $colors(-disable-fg)]
            ttk::style configure TCombobox \
                -fieldbackground $colors(-entry-bg) \
                -lightcolor $colors(-entry-bg) \
                -darkcolor $colors(-entry-bg) \
                -bordercolor $colors(-border) \
                -insertcolor $colors(-fg) \
                -padding {10 5}
            ttk::style map TCombobox \
                -foreground [list disabled $colors(-disable-fg)] \
                -background [list active $colors(-active)]
            ttk::style configure TSpinbox \
                -fieldbackground $colors(-entry-bg) \
                -lightcolor $colors(-entry-bg) \
                -darkcolor $colors(-entry-bg) \
                -bordercolor $colors(-border) \
                -insertcolor $colors(-fg) \
                -padding {10 5}
            ttk::style map TSpinbox \
                -foreground [list disabled $colors(-disable-fg)] \
                -background [list active $colors(-active)]
            ttk::style configure TCheckbutton -indicatorbackground $colors(-entry-bg) -indicatormargin 5
            ttk::style map TCheckbutton \
                -indicatorbackground [list selected $colors(-accent) !selected $colors(-entry-bg)] \
                -indicatorforeground [list selected $colors(-accent-fg) !selected $colors(-entry-bg)]
            ttk::style configure TRadiobutton -indicatorbackground $colors(-entry-bg) -indicatormargin 5
            ttk::style map TRadiobutton \
                -indicatorbackground [list selected $colors(-accent) !selected $colors(-entry-bg)] \
                -indicatorforeground [list selected $colors(-accent-fg) !selected $colors(-entry-bg)]
            ttk::style configure TNotebook -bordercolor $colors(-border)
            ttk::style configure TNotebook.Tab -background $colors(-bg) -foreground $colors(-disable-fg) -padding {10 5}
            ttk::style map TNotebook.Tab \
                -background [list selected $colors(-bg) !selected $colors(-dark)] \
                -foreground [list selected $colors(-fg) !selected $colors(-disable-fg)]
            ttk::style configure Treeview -fieldbackground $colors(-entry-bg)
            ttk::style map Treeview \
                -background [list selected $colors(-select-bg)] \
                -foreground [list selected $colors(-select-fg)]
            ttk::style configure Treeview.Heading -background $colors(-button-bg) -font "TkDefaultFont 9 bold"
            ttk::style configure TProgressbar -background $colors(-accent)
            ttk::style configure TScale -sliderlength 25
            ttk::style map TScale -background [list active $colors(-bg)]
            ttk::style configure TScrollbar -arrowcolor $colors(-fg) -arrowsize 15 -width 15
            ttk::style map TScrollbar -background [list !active $colors(-button-bg) active $colors(-button-active-bg)]
        }

        # Explicitly set the theme to be used
        ttk::style theme use sv
    }

    # Create the theme only ONCE when this file is sourced.
    # It is created with default settings that will be overridden by set_theme.
    ttk::style theme create sv {
        parent clam
    }
}

set ::sv_ttk::theme_dir [file join [file dirname [info script]]]
"""

_LIGHT_TCL = r"""
namespace eval sv_ttk {
    variable colors
    array set colors {
        -accent         "#0078d4"
        -accent-active  "#005a9e"
        -accent-fg      "#ffffff"
        -active         "#f2f2f2"
        -bg             "#ffffff"
        -border         "#c6c6c6"
        -button-active-bg "#e5e5e5"
        -button-bg      "#f0f0f0"
        -dark           "#e5e5e5"
        -disable-fg     "#8e8e8e"
        -entry-bg       "#ffffff"
        -fg             "#000000"
        -focus          "#0078d4"
        -light          "#ffffff"
        -select-bg      "#0078d4"
        -select-fg      "#ffffff"
        -trough         "#e5e5e5"
    }
}
"""

_DARK_TCL = r"""
namespace eval sv_ttk {
    variable colors
    array set colors {
        -accent         "#0078d4"
        -accent-active  "#005a9e"
        -accent-fg      "#ffffff"
        -active         "#373737"
        -bg             "#202020"
        -border         "#575757"
        -button-active-bg "#3e3e3e"
        -button-bg      "#323232"
        -dark           "#292929"
        -disable-fg     "#8e8e8e"
        -entry-bg       "#252525"
        -fg             "#ffffff"
        -focus          "#0078d4"
        -light          "#292929"
        -select-bg      "#0078d4"
        -select-fg      "#ffffff"
        -trough         "#2e2e2e"
    }
}
"""


def _get_theme_dir():
    # This is the main directory where the theme assets will be stored.
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "sv_ttk_theme")


def _create_theme_files():
    """Creates the theme directory and TCL files if they don't exist."""
    base_dir = _get_theme_dir()
    # CORRECTED: Create the 'theme' subdirectory that sv.tcl expects.
    theme_dir = os.path.join(base_dir, "theme")
    if not os.path.exists(theme_dir):
        os.makedirs(theme_dir)

    # Write the main sv.tcl file in the base directory
    sv_tcl_path = os.path.join(base_dir, "sv.tcl")
    if not os.path.exists(sv_tcl_path):
        with open(sv_tcl_path, "w") as f:
            f.write(_SV_TCL)

    # Write the color scheme files inside the 'theme' subdirectory
    files_to_create = {
        "light.tcl": _LIGHT_TCL,
        "dark.tcl": _DARK_TCL
    }

    for filename, content in files_to_create.items():
        filepath = os.path.join(theme_dir, filename)
        if not os.path.exists(filepath):
            with open(filepath, "w") as f:
                f.write(content)


def set_theme(theme: str, root: tk.Tk = None):
    """
    Sets the theme.

    :param theme: Theme to set. Can be "light" or "dark".
    :param root: The root window of the application. If not specified, the default root window will be used.
    """
    global _mode, _initialized

    if theme.lower() not in ("light", "dark"):
        raise RuntimeError(f'"{theme}" is not a valid theme.')

    _mode = theme.lower()

    if root is None:
        if tk._default_root:
            root = tk._default_root
        else:
            return

    if not _initialized:
        # Create the theme files before trying to source them
        _create_theme_files()

        # Correctly source the main TCL file from its base directory
        sv_tcl_path = os.path.join(_get_theme_dir(), "sv.tcl").replace("\\", "/")
        root.tk.eval(f'source {{{sv_tcl_path}}}')
        _initialized = True

    root.tk.eval(f'sv_ttk::set_theme "{_mode}"')


def get_theme():
    """
    Returns the current theme.
    """
    return _mode
