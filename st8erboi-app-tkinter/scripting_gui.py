import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
import queue
import os
import json
from functools import partial
import re

from script_validator import COMMANDS, validate_single_line
from script_processor import ScriptRunner
import theme

# --- Helper Class for Placeholder Text ---
class EntryWithPlaceholder(ttk.Entry):
    def __init__(self, master=None, placeholder="PLACEHOLDER", **kwargs):
        super().__init__(master, **kwargs)
        self.placeholder = placeholder
        self.placeholder_color = theme.SECONDARY_ACCENT
        self.default_fg_color = theme.FG_COLOR
        self.is_placeholder = True

        self.bind("<FocusIn>", self._clear_placeholder)
        self.bind("<FocusOut>", self._add_placeholder)
        self.put_placeholder()

    def put_placeholder(self):
        self.is_placeholder = True
        self.delete(0, tk.END)
        self.insert(0, self.placeholder)
        self.config(foreground=self.placeholder_color)

    def _clear_placeholder(self, event=None):
        if self.is_placeholder:
            self.is_placeholder = False
            self.delete(0, tk.END)
            self.config(foreground=self.default_fg_color)

    def _add_placeholder(self, event=None):
        if not self.get():
            self.put_placeholder()

    # Override get() to return empty string if it's a placeholder
    def get(self):
        if self.is_placeholder:
            return ""
        return super().get()

# --- Constants for Recent Files ---
RECENT_FILES_CONFIG = "recent_files.json"
MAX_RECENT_FILES = 5


# --- Recent Files Management ---
def load_recent_files():
    """Loads the list of recent file paths from the config file."""
    try:
        with open(RECENT_FILES_CONFIG, 'r') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return []


def save_recent_files(filepaths):
    """Saves the list of recent file paths to the config file."""
    with open(RECENT_FILES_CONFIG, 'w') as f:
        json.dump(filepaths, f)


def add_to_recent_files(filepath):
    """Adds a new filepath to the top of the recent files list."""
    filepaths = load_recent_files()
    if filepath in filepaths:
        filepaths.remove(filepath)
    filepaths.insert(0, filepath)
    save_recent_files(filepaths[:MAX_RECENT_FILES])

# --- Custom Text Widget and Line Numbers (Restored and Themed) ---

class CustomText(tk.Text):
    """A text widget that notifies of changes"""
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.config(
            bg=theme.WIDGET_BG,
            fg=theme.FG_COLOR,
            insertbackground=theme.PRIMARY_ACCENT, # Cursor color
            borderwidth=0,
            highlightthickness=0, # Remove the border
            font=theme.FONT_NORMAL,
            selectbackground=theme.SELECTION_BG,
            selectforeground=theme.SELECTION_FG,
            inactiveselectbackground=theme.SELECTION_BG, # Keep selection color when widget loses focus
            undo=True,
            wrap=tk.WORD,
            spacing3=3 # Add 3 pixels of spacing after each line
        )

        # Create a proxy for the underlying widget
        self._orig = self._w + "_orig"
        self.tk.call("rename", self._w, self._orig)
        self.tk.createcommand(self._w, self._proxy)

    def _proxy(self, *args):
        cmd = (self._orig,) + args
        try:
            result = self.tk.call(cmd)
        except tk.TclError:
            return None # Can happen with Ctrl-C

        # Generate virtual events for changes
        if (args[0] in ("insert", "delete", "replace") or
            args[0:3] == ("mark", "set", "insert") or
            args[0:2] in (("xview", "moveto"), ("xview", "scroll"),
                         ("yview", "moveto"), ("yview", "scroll"))):
            self.event_generate("<<Change>>", when="tail")
        
        if args[0] in ("insert", "delete", "replace"):
            self.event_generate("<<Modified>>", when="tail")
        
        return result

class TextLineNumbers(tk.Canvas):
    """A canvas that displays line numbers for a text widget."""
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.textwidget = None

    def attach(self, text_widget):
        self.textwidget = text_widget

    def redraw(self, *args):
        self.delete("all")
        i = self.textwidget.index("@0,0")
        while True:
            dline = self.textwidget.dlineinfo(i)
            if dline is None: break
            y = dline[1]
            linenum = str(i).split(".")[0]
            self.create_text(2, y, anchor="nw", text=linenum, 
                             font=theme.FONT_NORMAL, fill=theme.SECONDARY_ACCENT)
            i = self.textwidget.index(f"{i}+1line")

# --- Syntax Highlighter ---
class SyntaxHighlighter:
    def __init__(self, text_widget, keywords):
        self.text = text_widget
        self.keywords = keywords
        self.tags = {
            'command': {'foreground': theme.COMMAND_COLOR, 'font': theme.FONT_BOLD},
            'parameter': {'foreground': theme.PARAMETER_COLOR},
            'comment': {'foreground': theme.COMMENT_COLOR},
        }
        self._configure_tags()
        # Use a flag to prevent re-highlighting while highlighting is in progress
        self._highlighting = False
        # Bind to the custom <<Modified>> event, which fires on any text change.
        self.text.bind('<<Modified>>', self.highlight)

    def _configure_tags(self):
        for tag_name, tag_config in self.tags.items():
            self.text.tag_configure(tag_name, **tag_config)

    def highlight(self, event=None):
        # Use 'end-1c' to avoid highlighting the final newline which can cause issues
        content = self.text.get("1.0", "end-1c")
        
        # A small delay to prevent rapid, sequential updates from fighting each other.
        self.text.after(10, self._apply_highlight, content)

    def _apply_highlight(self, content):
        # Remove all tags first to prevent stacking
        for tag in self.tags.keys():
            self.text.tag_remove(tag, "1.0", "end")

        # Highlight keywords
        keyword_pattern = r'\b(' + '|'.join(self.keywords) + r')\b'
        for match in re.finditer(keyword_pattern, content, re.IGNORECASE):
            start, end = match.span()
            self.text.tag_add("command", f"1.0+{start}c", f"1.0+{end}c")

        # Highlight numbers (integers and floats)
        for match in re.finditer(r'\b-?\d+(\.\d+)?\b', content):
            start, end = match.span()
            self.text.tag_add("parameter", f"1.0+{start}c", f"1.0+{end}c")

        # Highlight comments
        for match in re.finditer(r'#.*', content):
            start, end = match.span()
            self.text.tag_add("comment", f"1.0+{start}c", f"1.0+{end}c")


# --- Themed Script Editor (Rebuilt with Line Numbers) ---

class ScriptEditor(tk.Frame):
    def __init__(self, parent, keywords, **kwargs):
        super().__init__(parent, **kwargs)
        self.config(bg=theme.WIDGET_BG)

        # Create components
        self.text = CustomText(self)
        self.linenumbers = TextLineNumbers(self, width=40, bg=theme.WIDGET_BG, highlightthickness=0, borderwidth=0)
        self.linenumbers.attach(self.text)
        
        self.vsb = ttk.Scrollbar(self, orient="vertical", command=self.text.yview)
        self.text.configure(yscrollcommand=self.vsb.set)
        
        # Pack components
        self.linenumbers.pack(side="left", fill="y")
        self.vsb.pack(side="right", fill="y")
        self.text.pack(side="right", fill="both", expand=True)

        # Bind events
        self.text.bind("<<Change>>", self._on_change)
        self.text.bind("<Configure>", self._on_change)
        self.text.bind("<KeyRelease>", self._highlight_current_line)
        self.text.bind("<Button-1>", self._on_change)

        self.text.tag_configure("current_line", background=theme.SECONDARY_ACCENT)
        self._highlight_current_line()
        
        # Attach syntax highlighter
        # FIX: Pass self.text, not self, to the highlighter.
        self.highlighter = SyntaxHighlighter(self.text, keywords)
    
    def _highlight_current_line(self, event=None):
        self.text.tag_remove("current_line", "1.0", "end")
        self.text.tag_add("current_line", "insert linestart", "insert lineend+1c")
        self.text.tag_raise("sel") # Raise selection tag to the top of the stacking order

    def _on_change(self, event=None):
        self.linenumbers.redraw()
        self._highlight_current_line()

    # --- Proxy methods to make this class act like a Text widget ---
    def get(self, *args, **kwargs): return self.text.get(*args, **kwargs)
    def insert(self, *args, **kwargs): return self.text.insert(*args, **kwargs)
    def delete(self, *args, **kwargs): return self.text.delete(*args, **kwargs)
    def tag_add(self, *args, **kwargs): return self.text.tag_add(*args, **kwargs)
    def tag_remove(self, *args, **kwargs): return self.text.tag_remove(*args, **kwargs)
    def tag_config(self, *args, **kwargs): return self.text.tag_config(*args, **kwargs)
    def bind(self, *args, **kwargs): self.text.bind(*args, **kwargs)
    def index(self, *args, **kwargs): return self.text.index(*args, **kwargs)
    def edit_modified(self, *args, **kwargs): return self.text.edit_modified(*args, **kwargs)
    def edit_reset(self, *args, **kwargs): return self.text.edit_reset(*args, **kwargs)

# --- Validation Window (Themed) ---
class ValidationResultsWindow(tk.Toplevel):
    def __init__(self, parent, errors):
        super().__init__(parent)
        self.title("Validation Results")
        self.geometry("600x300")
        self.configure(bg=theme.WIDGET_BG)
        self.transient(parent)
        self.grab_set()
        
        text_area = tk.Text(self, wrap=tk.WORD, 
                                bg=theme.WIDGET_BG, 
                                fg=theme.FG_COLOR, 
                                font=theme.FONT_NORMAL,
                                borderwidth=0,
                                highlightthickness=0)
        text_area.pack(expand=True, fill="both", padx=10, pady=10)
        
        if not errors:
            text_area.insert(tk.END, "Validation successful! No errors found.")
        else:
            for error in errors:
                text_area.insert(tk.END, f"Line {error['line']}: {error['error']}\n")
        
        text_area.config(state=tk.DISABLED)
        close_button = ttk.Button(self, text="Close", command=self.destroy)
        close_button.pack(pady=5)


# --- GUI Creation Functions (Wiring everything up) ---
def create_command_reference(parent, script_editor_widget):
    ref_frame = ttk.Frame(parent, style='TFrame', padding=5)

    # --- Filter Widgets ---
    filter_frame = ttk.Frame(ref_frame, style='TFrame')
    filter_frame.pack(fill=tk.X, pady=(0, 5))
    filter_frame.grid_columnconfigure((0,1,2,3), weight=1) # Make columns resizable

    # --- Treeview ---
    tree_frame = ttk.Frame(ref_frame, style='TFrame')
    tree_frame.pack(fill=tk.BOTH, expand=True)
    tree = ttk.Treeview(tree_frame, columns=('device', 'params', 'desc'), show='tree headings')
    
    all_commands = []
    for cmd, details in sorted(COMMANDS.items(), key=lambda item: (item[1]['device'], item[0])):
        all_commands.append({
            'cmd': cmd,
            'device': details['device'].capitalize(),
            'params': " ".join([p['name'] for p in details['params']]),
            'desc': details['help']
        })

    def populate_tree(commands_to_display):
        for item in tree.get_children():
            tree.delete(item)
        for cmd_data in commands_to_display:
            tree.insert('', 'end', text=cmd_data['cmd'], values=(cmd_data['device'], cmd_data['params'], cmd_data['desc']))

    filter_vars = {}
    def apply_filters(*args):
        filtered_commands = all_commands
        for col, var in filter_vars.items():
            entry = var.get()
            # Don't filter if it's just the placeholder
            if entry and entry != f"Filter {col.capitalize()}...":
                filter_text = entry.lower()
                filtered_commands = [cmd for cmd in filtered_commands if filter_text in cmd[col].lower()]
        populate_tree(filtered_commands)
        # After filtering, re-apply the current sort
        if 'sort_by' in tree.__dict__:
             sort_column(tree.sort_by, tree.sort_rev, True)


    columns = {'#0': 'cmd', 'device': 'device', 'params': 'params', 'desc': 'desc'}
    widths = {'#0': 200, 'device': 60, 'params': 60, 'desc': 300}

    for i, (col_id, col_key) in enumerate(columns.items()):
        tree.heading(col_id, text=col_key.capitalize() if col_id!='#0' else 'Command')
        tree.column(col_id, width=widths[col_id], anchor='w')
        
        var = tk.StringVar()
        var.trace_add("write", apply_filters)
        filter_vars[col_key] = var
        
        # FIX: Remove old, incorrect style from placeholder entry and add textvariable
        entry = EntryWithPlaceholder(filter_frame, 
                                     textvariable=var,
                                     placeholder=f"Filter {col_key.capitalize()}...")
        entry.grid(row=0, column=i, sticky='ew', padx=(0 if i==0 else 2, 0))

    def sort_column(col_id, reverse, preserve=False):
        # The text of the header is stored in tree.heading(col_id, "text")
        header_text = tree.heading(col_id, "text").replace(" ▲", "").replace(" ▼", "")
        
        # Reset all headers
        for cid, ckey in columns.items():
             tree.heading(cid, text=ckey.capitalize() if cid!='#0' else 'Command')
        
        arrow = " ▼" if reverse else " ▲"
        tree.heading(col_id, text=header_text + arrow)

        # Get data to sort
        if col_id == '#0':
             l = [(tree.item(k)['text'], k) for k in tree.get_children('')]
        else:
             l = [(tree.set(k, col_id), k) for k in tree.get_children('')]
        
        l.sort(key=lambda t: t[0].lower(), reverse=reverse)
        for index, (val, k) in enumerate(l):
            tree.move(k, '', index)

        # Update the heading command to toggle sorting
        tree.heading(col_id, command=lambda: sort_column(col_id, not reverse))
        # Store sort state
        tree.sort_by = col_id
        tree.sort_rev = reverse


    for col_id, col_key in columns.items():
        tree.heading(col_id, command=lambda c=col_id: sort_column(c, False))


    vsb = ttk.Scrollbar(tree_frame, orient="vertical", command=tree.yview)
    tree.configure(yscrollcommand=vsb.set)
    tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    vsb.pack(side=tk.RIGHT, fill=tk.Y)

    populate_tree(all_commands)
    # Sort by the 'Command' column by default on startup
    # Using 'after' gives the UI a moment to draw before sorting.
    tree.after(100, lambda: sort_column('#0', False))

    context_menu = tk.Menu(parent, tearoff=0, 
                           bg=theme.WIDGET_BG, 
                           fg=theme.FG_COLOR,
                           activebackground=theme.PRIMARY_ACCENT,
                           activeforeground=theme.FG_COLOR)

    def get_selected_command():
        selected_item = tree.focus()
        if not selected_item: return None
        return tree.item(selected_item, "text")

    def copy_command():
        command = get_selected_command()
        if command: ref_frame.clipboard_clear(); ref_frame.clipboard_append(command)

    def add_to_script():
        command = get_selected_command()
        if command: script_editor_widget.insert(tk.INSERT, f"{command} ")

    context_menu.add_command(label="Copy Command", command=copy_command);
    context_menu.add_command(label="Add to Script", command=add_to_script)

    def show_context_menu(event):
        iid = tree.identify_row(event.y)
        if iid: tree.selection_set(iid); tree.focus(iid); context_menu.post(event.x_root, event.y_root)

    tree.bind("<Button-3>", show_context_menu);
    tree.bind("<Double-1>", lambda e: add_to_script())
    return ref_frame


def create_scripting_interface(parent, command_funcs, shared_gui_refs, autosave_var):
    """
    Creates the main scripting area, including the editor and command reference.
    Returns a dictionary containing file commands and a callback to update the recent menu.
    """
    scripting_area = ttk.Frame(parent, style='TFrame')
    scripting_area.pack(fill=tk.BOTH, expand=True)

    # Get a reference to the root window for thread-safe GUI updates
    root = scripting_area.winfo_toplevel()

    paned_window = ttk.PanedWindow(scripting_area, orient=tk.HORIZONTAL, style='TPanedwindow')
    paned_window.pack(fill=tk.BOTH, expand=True, padx=10, pady=(10, 5))
    left_pane = ttk.Frame(paned_window, style='TFrame')
    paned_window.add(left_pane, weight=1) # The script editor will be the only thing here now

    # The right_pane and the command_ref_widget that was created here have been removed,
    # as the command reference is now created in its own collapsible panel in main.py.

    # --- Scripting State Variables ---
    script_runner = None
    last_exec_highlight = -1
    last_selection_highlight = 1
    current_filepath = None
    feed_hold_line = None
    single_block_var = tk.BooleanVar(value=False)

    # --- Message Queue & Terminal ---
    message_queue = queue.Queue()
    original_terminal_cb = shared_gui_refs.get('terminal_cb')
    if 'vacuum_check_status_var' not in shared_gui_refs:
        shared_gui_refs['vacuum_check_status_var'] = tk.StringVar(value='N/A')

    recent_files_menu_ref = None

    def terminal_wrapper(message):
        # --- Intercept ERROR messages to display them prominently ---
        if "_ERROR:" in message:
            # The root.after is crucial to prevent threading issues with Tkinter
            root.after(0, lambda: status_label.config(foreground=theme.ERROR_RED)) # Bright Red
            root.after(0, lambda: status_var.set(message))

        if "DONE:" in message: message_queue.put(message)
        if original_terminal_cb: original_terminal_cb(message)

    shared_gui_refs['terminal_cb'] = terminal_wrapper

    # --- UI Creation ---
    control_frame = ttk.Frame(left_pane, style='TFrame');
    control_frame.pack(fill=tk.X, pady=(0, 0))
    editor_frame = ttk.LabelFrame(left_pane, style='TFrame')
    editor_frame.pack(fill=tk.BOTH, expand=True, pady=5)
    script_editor = ScriptEditor(editor_frame, keywords=list(COMMANDS.keys())) # Use the rebuilt composite widget
    script_editor.pack(fill="both", expand=True)

    # Configure tags for execution and error highlighting
    script_editor.tag_config("exec_highlight", background=theme.WARNING_YELLOW, foreground="black")
    script_editor.tag_config("selection_highlight", background=theme.SECONDARY_ACCENT)
    script_editor.tag_config("error_highlight", background=theme.ERROR_RED, foreground=theme.FG_COLOR)
    
    script_editor.insert(tk.END, "# Example Script\n# Type commands here or load a file.\n")
    script_editor.edit_reset()
    script_editor.edit_modified(False)

    # The command reference is no longer created here.

    def update_window_title():
        filename = "Untitled"
        if current_filepath: filename = os.path.basename(current_filepath)
        modified_star = "*" if script_editor.edit_modified() else "";
        root.title(f"{filename}{modified_star} - Multi-Device Controller")

    def on_text_modified(event):
        """Updates window title and triggers autosave if enabled."""
        update_window_title()
        # --- Autosave Logic ---
        if autosave_var.get() and current_filepath:
            # Call the save_script function but without triggering the dialog
            try:
                with open(current_filepath, 'w') as f:
                    f.write(script_editor.get('1.0', tk.END))
                # Mark as unmodified to prevent the "unsaved" dialog
                script_editor.edit_modified(False)
                update_window_title() # Update title to remove '*'
            except Exception as e:
                status_var.set(f"Autosave Error: {e}")

        # Update the highlight to follow the cursor's current line
        current_line = int(script_editor.index(tk.INSERT).split('.')[0])
        update_selection_highlight(current_line)

    # Add the binding for autosave/title update. The highlighter binds itself.
    script_editor.bind("<<Modified>>", on_text_modified, add='+')

    status_var = tk.StringVar(value="Status: Idle")

    # --- Recent Files Menu Management ---
    def update_recent_files_display():
        if not recent_files_menu_ref: return
        recent_files_menu_ref.delete(0, tk.END)
        filepaths = load_recent_files()
        if not filepaths:
            recent_files_menu_ref.add_command(label="Empty", state=tk.DISABLED)
        else:
            for path in filepaths:
                recent_files_menu_ref.add_command(label=os.path.basename(path),
                                                  command=partial(load_specific_script, path))

    def set_recent_menu_reference(menu_obj):
        nonlocal recent_files_menu_ref
        recent_files_menu_ref = menu_obj
        update_recent_files_display()

    # --- File Operations ---
    def check_unsaved_changes():
        if not script_editor.edit_modified(): return True
        response = messagebox.askyesnocancel("Unsaved Changes", "You have unsaved changes. Do you want to save them?")
        if response is True:
            return save_script()
        elif response is False:
            return True
        else:
            return False

    def new_script():
        nonlocal current_filepath
        if not check_unsaved_changes(): return
        script_editor.delete('1.0', tk.END);
        script_editor.edit_modified(False);
        current_filepath = None;
        update_window_title();
        update_selection_highlight(1)

    def save_script():
        nonlocal current_filepath
        if current_filepath:
            try:
                with open(current_filepath, 'w') as f:
                    f.write(script_editor.get('1.0', tk.END))
                script_editor.edit_modified(False);
                update_window_title();
                status_var.set(f"Saved to {os.path.basename(current_filepath)}");
                add_to_recent_files(current_filepath)
                update_recent_files_display()
                return True
            except Exception as e:
                messagebox.showerror("Save Error", f"Could not save file:\n{e}");
                return False
        else:
            return save_script_as()

    def save_script_as():
        nonlocal current_filepath
        filepath = filedialog.asksaveasfilename(title="Save Script As", defaultextension=".txt",
                                                filetypes=(("Text files", "*.txt"), ("All files", "*.*")))
        if not filepath: return False
        current_filepath = filepath;
        return save_script()

    def load_specific_script(filepath):
        nonlocal current_filepath
        if not check_unsaved_changes(): return
        if not os.path.exists(filepath):
            messagebox.showerror("File Not Found", f"Could not find file:\n{filepath}")
            filepaths = load_recent_files()
            if filepath in filepaths:
                filepaths.remove(filepath)
                save_recent_files(filepaths)
            update_recent_files_display()
            return
        try:
            with open(filepath, 'r') as f:
                script_editor.delete('1.0', tk.END);
                script_editor.insert('1.0', f.read())
            current_filepath = filepath;
            script_editor.edit_modified(False);
            update_window_title();
            update_selection_highlight(1);
            status_var.set(f"Loaded {os.path.basename(current_filepath)}")
            add_to_recent_files(filepath)
            update_recent_files_display()
        except Exception as e:
            messagebox.showerror("Load Error", f"Could not load file:\n{e}")

    def load_script():
        filepath = filedialog.askopenfilename(title="Open Script File",
                                              filetypes=(("Text files", "*.txt"), ("All files", "*.*")))
        if not filepath: return
        load_specific_script(filepath)

    # --- Script Execution Logic ---
    def set_buttons_state(state):
        cycle_start_button.config(state=state)
        is_running = state == tk.DISABLED
        feed_hold_button.config(state=tk.NORMAL if is_running else tk.DISABLED)

    # MODIFIED: This function now safely schedules GUI updates on the main thread.
    def status_callback_handler(message, line_num):
        def update_gui():
            nonlocal last_exec_highlight
            status_var.set(message)
            if last_exec_highlight != line_num:
                if last_exec_highlight != -1:
                    script_editor.tag_remove("exec_highlight", f"{last_exec_highlight}.0", f"{last_exec_highlight}.end")
                if line_num != -1:
                    script_editor.tag_add("exec_highlight", f"{line_num}.0", f"{line_num}.end")
                last_exec_highlight = line_num

        # Schedule the GUI update to run in the main event loop
        root.after(0, update_gui)

    def on_run_finished():
        root.after(0, lambda: set_buttons_state(tk.NORMAL))
        status_callback_handler("Idle", -1)

    def on_step_finished():
        root.after(0, lambda: set_buttons_state(tk.NORMAL))
        current_line_num = int(last_selection_highlight)
        status_var.set(f"Step complete. Next line: {current_line_num + 1}");
        root.after(0, lambda: update_selection_highlight(current_line_num + 1))

    def run_script_from_content(content, line_offset=0, is_step=False):
        nonlocal script_runner
        set_buttons_state(tk.DISABLED)
        completion_callback = on_step_finished if is_step else on_run_finished
        script_runner = ScriptRunner(content, command_funcs, shared_gui_refs, status_callback_handler,
                                     completion_callback, message_queue, line_offset);
        script_runner.start()

    def check_script_validity(show_success=False):
        script_content = script_editor.get("1.0", tk.END);
        errors = validate_script(script_content);
        script_editor.tag_remove("error_highlight", "1.0", tk.END)
        if errors:
            ValidationResultsWindow(scripting_area, errors);
            status_var.set(f"{len(errors)} error(s) found.")
            for error in errors: line_num = error['line']; script_editor.tag_add("error_highlight", f"{line_num}.0",
                                                                                 f"{line_num}.end")
            return False
        else:
            if show_success: messagebox.showinfo("Validation Success", "Script is valid!")
            status_var.set("Validation Successful");
            return True

    def handle_feed_hold():
        nonlocal feed_hold_line
        command_funcs['abort']()

        if script_runner and script_runner.is_running:
            feed_hold_line = last_exec_highlight
            script_runner.stop()
            on_run_finished()
            status_var.set(f"Feed Hold. Halted at line {feed_hold_line}.")
            if feed_hold_line != -1:
                update_selection_highlight(feed_hold_line)
        else:
            status_var.set("All motion stopped.")

    def handle_reset():
        nonlocal script_runner, feed_hold_line
        if script_runner and script_runner.is_running:
            script_runner.stop()
        command_funcs['abort']()

        # Explicitly clear any lingering execution highlight immediately.
        script_editor.tag_remove("exec_highlight", "1.0", tk.END)

        # Send CLEAR_ERRORS to both devices. Gantry will ignore it for now.
        command_funcs['send_fillhead']("CLEAR_ERRORS")
        command_funcs['send_gantry']("CLEAR_ERRORS")

        feed_hold_line = None
        on_run_finished()
        update_selection_highlight(1)
        status_var.set("Script reset. Ready to start from line 1.")

    def handle_cycle_start():
        nonlocal feed_hold_line
        start_line_num = 1
        if feed_hold_line is not None:
            start_line_num = feed_hold_line
            feed_hold_line = None
        else:
            try:
                start_line_num = int(last_selection_highlight)
            except (ValueError, TypeError):
                start_line_num = 1

        update_selection_highlight(start_line_num)
        is_single_block = single_block_var.get()
        all_lines = script_editor.get("1.0", tk.END).splitlines()

        next_valid_line_num = -1
        next_valid_line_content = ""
        for i in range(start_line_num - 1, len(all_lines)):
            line_content = all_lines[i].strip()
            if line_content and not line_content.startswith('#'):
                next_valid_line_num = i + 1
                next_valid_line_content = all_lines[i]
                break

        if next_valid_line_num == -1:
            status_var.set("End of script reached.")
            on_run_finished()
            return

        update_selection_highlight(next_valid_line_num)

        if is_single_block:
            errors = validate_single_line(next_valid_line_content, next_valid_line_num)
            script_editor.tag_remove("error_highlight", "1.0", tk.END)
            if errors:
                ValidationResultsWindow(scripting_area, errors)
                script_editor.tag_add("error_highlight", f"{next_valid_line_num}.0", f"{next_valid_line_num}.end")
                status_var.set(f"Error on line {next_valid_line_num}.")
                return
            run_script_from_content(next_valid_line_content, next_valid_line_num - 1, is_step=True)
        else:
            if not check_script_validity(): return
            content_from_line = "\n".join(all_lines[next_valid_line_num - 1:])
            run_script_from_content(content_from_line, next_valid_line_num - 1, is_step=False)

    def update_selection_highlight(line_num):
        nonlocal last_selection_highlight
        # Remove the highlight from all lines first to ensure only one is ever highlighted.
        script_editor.tag_remove("selection_highlight", "1.0", tk.END)
        if line_num != -1:
            script_editor.tag_add("selection_highlight", f"{line_num}.0", f"{line_num}.end")
            last_selection_highlight = line_num

    def on_line_click(event):
        index = script_editor.index(f"@{event.x},{event.y}")
        line_num = int(index.split('.')[0])
        update_selection_highlight(line_num)

    script_editor.bind("<Button-1>", on_line_click)
    update_selection_highlight(1)

    # --- Control Buttons ---
    btn_container = ttk.Frame(control_frame, style='TFrame');
    btn_container.pack(fill=tk.X, padx=10, pady=(5, 0))

    cycle_start_button = ttk.Button(btn_container, text="Cycle Start", command=handle_cycle_start,
                                    style="Green.TButton")
    cycle_start_button.pack(side=tk.LEFT, padx=(0, 5))

    feed_hold_button = ttk.Button(btn_container, text="Feed Hold", command=handle_feed_hold, style="Red.TButton")
    feed_hold_button.pack(side=tk.LEFT, padx=5)

    reset_button = ttk.Button(btn_container, text="Reset", command=handle_reset, style="Small.TButton")
    reset_button.pack(side=tk.LEFT, padx=5)

    single_block_switch = ttk.Checkbutton(btn_container, text="Single Block", variable=single_block_var,
                                          style="OrangeToggle.TButton")
    single_block_switch.pack(side=tk.LEFT, padx=5)

    feed_hold_button.config(state=tk.DISABLED)

    # --- Status Label ---
    status_label = ttk.Label(control_frame, textvariable=status_var, anchor='w', 
                             font=theme.FONT_LARGE_BOLD, 
                             foreground=theme.PRIMARY_ACCENT,
                             background=theme.BG_COLOR)
    status_label.pack(side=tk.LEFT, padx=10, pady=(5, 5), fill=tk.X, expand=True)
    update_window_title()

    # --- Callback to reset status color ---
    def reset_status_color(*args):
        status_label.config(foreground=theme.PRIMARY_ACCENT)

    status_var.trace_add("write", reset_status_color)


    # --- Return file commands and menu update callback ---
    file_commands = {
        "new": new_script,
        "open": load_script,
        "save": save_script,
        "save_as": save_script_as,
        "validate": lambda: check_script_validity(show_success=True)
    }
    return {
        "file_commands": file_commands,
        "update_recent_menu_callback": set_recent_menu_reference,
        "check_unsaved": check_unsaved_changes,
        "load_specific_script": load_specific_script,
        "script_editor": script_editor # Expose the script editor widget
    }
