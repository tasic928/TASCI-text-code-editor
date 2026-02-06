#!/usr/bin/env python3
import os
import tkinter as tk
from tkinter import ttk, colorchooser, filedialog, messagebox


def clamp_color(value):
    return max(0, min(255, int(value)))


def rgb_to_hex(rgb):
    r, g, b = rgb
    return f"#{r:02x}{g:02x}{b:02x}"


def escape_ts(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


class ThemeState:
    def __init__(self):
        self.name = "Midnight Drift"
        self.background = (18, 20, 27)
        self.accent = (44, 107, 255)
        self.editor_text = (230, 231, 235)
        self.keyword = (94, 234, 212)
        self.sidebar = (27, 31, 42)
        self.menu = (15, 23, 42)
        self.status = (11, 18, 32)
        self.editor_bg = (18, 20, 27)
        self.tabs_bg = (13, 17, 23)
        self.tabs_text = (154, 164, 178)
        self.tabs_active_bg = (27, 31, 42)
        self.tabs_active_text = (230, 231, 235)
        self.line_numbers = (122, 135, 153)
        self.selection = (52, 79, 122)
        self.bg_image = ""


class ThemeCreatorApp:
    def __init__(self, root):
        self.root = root
        self.state = ThemeState()
        self.root.title("TASCI Theme Creator")
        self.root.geometry("1200x760")
        self.root.configure(bg="#0b0f16")

        self.style = ttk.Style(self.root)
        self.style.theme_use("clam")
        self._setup_styles()

        self.bg_image_tk = None

        self._build_ui()
        self._sync_state()

    def _setup_styles(self):
        self.style.configure("Panel.TFrame", background="#0d1117")
        self.style.configure("Panel.TLabel", background="#0d1117", foreground="#e6e7eb")
        self.style.configure("Muted.TLabel", background="#0d1117", foreground="#9aa4b2")
        self.style.configure("Title.TLabel", background="#0d1117", foreground="#e6e7eb", font=("Helvetica", 14, "bold"))
        self.style.configure("Section.TLabel", background="#0d1117", foreground="#9aa4b2", font=("Helvetica", 10, "bold"))
        self.style.configure("Panel.TButton", background="#1f2937", foreground="#e6e7eb")

    def _build_ui(self):
        root = self.root

        root.columnconfigure(0, weight=0)
        root.columnconfigure(1, weight=1)
        root.rowconfigure(0, weight=1)

        self.panel = ttk.Frame(root, style="Panel.TFrame", padding=16)
        self.panel.grid(row=0, column=0, sticky="ns")

        title = ttk.Label(self.panel, text="TASCI Theme Creator", style="Title.TLabel")
        title.pack(anchor="w")
        subtitle = ttk.Label(self.panel, text="Design a theme and export a .ts file", style="Muted.TLabel")
        subtitle.pack(anchor="w", pady=(2, 10))

        ttk.Label(self.panel, text="Basics", style="Section.TLabel").pack(anchor="w", pady=(8, 4))
        ttk.Label(self.panel, text="Theme name", style="Muted.TLabel").pack(anchor="w")
        self.name_var = tk.StringVar(value=self.state.name)
        self.name_entry = ttk.Entry(self.panel, textvariable=self.name_var)
        self.name_entry.pack(fill="x", pady=(0, 8))

        ttk.Label(self.panel, text="Background", style="Section.TLabel").pack(anchor="w", pady=(8, 4))
        self._add_color_row("Window background", "background")
        self._add_color_row("Accent color", "accent")

        ttk.Label(self.panel, text="Editor", style="Section.TLabel").pack(anchor="w", pady=(8, 4))
        self._add_color_row("Editor background", "editor_bg")
        self._add_color_row("Editor text", "editor_text")
        self._add_color_row("Keyword", "keyword")
        self._add_color_row("Line numbers", "line_numbers")
        self._add_color_row("Selection", "selection")

        ttk.Label(self.panel, text="Sidebar/Menu", style="Section.TLabel").pack(anchor="w", pady=(8, 4))
        self._add_color_row("Sidebar", "sidebar")
        self._add_color_row("Menu bar", "menu")
        self._add_color_row("Status bar", "status")

        ttk.Label(self.panel, text="Tabs", style="Section.TLabel").pack(anchor="w", pady=(8, 4))
        self._add_color_row("Tabs background", "tabs_bg")
        self._add_color_row("Tabs text", "tabs_text")
        self._add_color_row("Active tab", "tabs_active_bg")
        self._add_color_row("Active tab text", "tabs_active_text")

        ttk.Label(self.panel, text="Preview Image", style="Section.TLabel").pack(anchor="w", pady=(8, 4))
        ttk.Label(self.panel, text="Code preview image", style="Muted.TLabel").pack(anchor="w")
        media_row = ttk.Frame(self.panel, style="Panel.TFrame")
        media_row.pack(fill="x", pady=(2, 6))
        ttk.Button(media_row, text="Select image", command=self._select_image, style="Panel.TButton").pack(side="left")
        ttk.Button(media_row, text="Clear", command=self._clear_media, style="Panel.TButton").pack(side="left", padx=(8, 0))
        ttk.Button(media_row, text="Export Theme", command=self._export_theme, style="Panel.TButton").pack(side="left", padx=(8, 0))
        self.media_label = ttk.Label(self.panel, text="No media selected", style="Muted.TLabel", wraplength=300)
        self.media_label.pack(anchor="w", pady=(0, 8))

        ttk.Label(self.panel, text="Export", style="Section.TLabel").pack(anchor="w", pady=(8, 4))
        ttk.Button(self.panel, text="Export Theme", command=self._export_theme, style="Panel.TButton").pack(anchor="w", pady=(0, 4))

        self.preview_frame = tk.Frame(root)
        self.preview_frame.grid(row=0, column=1, sticky="nsew")
        self.preview_frame.rowconfigure(1, weight=1)
        self.preview_frame.columnconfigure(0, weight=1)

        self.menu_line = tk.Text(
            self.preview_frame,
            height=1,
            wrap="none",
            borderwidth=0,
            highlightthickness=0,
            font=("DejaVu Sans Mono", 11),
        )
        self.menu_line.grid(row=0, column=0, sticky="ew")
        self.menu_line.configure(state="disabled")

        self.preview_text = tk.Text(
            self.preview_frame,
            wrap="none",
            borderwidth=0,
            highlightthickness=0,
            font=("DejaVu Sans Mono", 11),
        )
        self.preview_text.grid(row=1, column=0, sticky="nsew")
        self.preview_text.configure(state="disabled")

        self.name_var.trace_add("write", lambda *_: self._sync_state())

    def _add_color_row(self, label, key):
        row = ttk.Frame(self.panel, style="Panel.TFrame")
        row.pack(fill="x", pady=(2, 4))
        ttk.Label(row, text=label, style="Muted.TLabel").pack(side="left")
        swatch = tk.Label(row, text=" ", width=8, relief="solid", bd=1)
        swatch.pack(side="right", padx=(8, 4))
        btn = ttk.Button(row, text="Color wheel", command=lambda k=key: self._pick_color(k), style="Panel.TButton")
        btn.pack(side="right")
        setattr(self, f"swatch_{key}", swatch)

    def _pick_color(self, key):
        current = rgb_to_hex(getattr(self.state, key))
        color = colorchooser.askcolor(color=current, parent=self.root)
        if color and color[0]:
            r, g, b = [clamp_color(v) for v in color[0]]
            setattr(self.state, key, (r, g, b))
            self._sync_state()

    def _select_image(self):
        filename = filedialog.askopenfilename(
            title="Select image",
            filetypes=[
                ("Images", "*.png *.gif *.ppm *.pgm"),
                ("All files", "*.*"),
            ],
        )
        if filename:
            self.state.bg_image = filename
            self._sync_state()

    def _clear_media(self):
        self.state.bg_image = ""
        self._sync_state()

    def _sync_state(self):
        self.state.name = self.name_var.get() or "Untitled Theme"
        self._update_preview_text()
        self._update_colors()
        self._update_media_label()

    def _update_preview_text(self):
        menu_text = (
            "Edit      View      Find      Help      File      Save      "
            "Save As      Open Folder      About"
        )
        self.menu_line.configure(state="normal")
        self.menu_line.delete("1.0", "end")
        self.menu_line.insert("1.0", menu_text)
        self.menu_line.configure(state="disabled")

        left_items = [
            ".ssh/",
            ".pki/",
            "snap/",
            ".bash_history",
            "Desktop/",
            ".profile",
            ".bash_logout",
            ".codex/",
            ".dotnet/",
            ".config/",
            "Videos/",
            "Public/",
            "Templates/",
            ".sudo_as_admin_successful",
            "./",
            "Documents/",
            ".bashrc",
            ".vscode/",
            ".cache/",
            "Music/",
            ".gitconfig",
            "Pictures/",
            ".local/",
            "../",
            "Downloads/",
        ]

        left_width = 28
        right_width = 110
        rows = max(len(left_items), 24)
        left_items += [""] * (rows - len(left_items))
        right_items = [" 1"] + [""] * (rows - 1)

        lines = []
        top = "┌" + ("─" * left_width) + "┐" + "┌" + ("─" * right_width) + "┐"
        bottom = "└" + ("─" * left_width) + "┘" + "└" + ("─" * right_width) + "┘"
        lines.append(top)
        for i in range(rows):
            left = left_items[i][:left_width].ljust(left_width)
            right = right_items[i][:right_width].ljust(right_width)
            lines.append(f"│{left}││{right}│")
        lines.append(bottom)

        self.preview_text.configure(state="normal")
        self.preview_text.delete("1.0", "end")
        self.preview_text.insert("1.0", "\n".join(lines))

        self._apply_preview_image(left_width)

        # Sidebar + editor region tags for each line
        for i in range(1, len(lines) + 1):
            self.preview_text.tag_add("sidebar", f"{i}.0", f"{i}.{left_width + 2}")
            self.preview_text.tag_add("editor", f"{i}.{left_width + 2}", f"{i}.end")

        # Line number tag for editor column
        self.preview_text.tag_add(
            "line_number",
            f"2.{left_width + 3}",
            f"2.{left_width + 5}",
        )
        # Selection mock
        self.preview_text.tag_add(
            "selection",
            f"2.{left_width + 6}",
            f"2.{left_width + 18}",
        )

        self.preview_text.configure(state="disabled")

    def _update_colors(self):
        bg = rgb_to_hex(self.state.background)
        accent = rgb_to_hex(self.state.accent)
        editor_bg = rgb_to_hex(self.state.editor_bg)
        editor_text = rgb_to_hex(self.state.editor_text)
        keyword = rgb_to_hex(self.state.keyword)
        sidebar = rgb_to_hex(self.state.sidebar)
        menu = rgb_to_hex(self.state.menu)
        status = rgb_to_hex(self.state.status)
        tabs_bg = rgb_to_hex(self.state.tabs_bg)
        tabs_text = rgb_to_hex(self.state.tabs_text)
        tabs_active_bg = rgb_to_hex(self.state.tabs_active_bg)
        tabs_active_text = rgb_to_hex(self.state.tabs_active_text)
        line_numbers = rgb_to_hex(self.state.line_numbers)

        self.preview_frame.configure(bg=bg)
        self.menu_line.configure(bg=menu, fg=editor_text, insertbackground=editor_text)
        self.preview_text.configure(bg=bg, fg=editor_text, insertbackground=editor_text)

        self.preview_text.tag_configure("sidebar", background=sidebar, foreground=editor_text)
        self.preview_text.tag_configure("editor", background=editor_bg, foreground=editor_text)
        self.preview_text.tag_configure("line_number", foreground=line_numbers, background=editor_bg)
        self.preview_text.tag_configure("selection", background=rgb_to_hex(self.state.selection))

        for key in [
            "background",
            "accent",
            "editor_bg",
            "editor_text",
            "keyword",
            "sidebar",
            "menu",
            "status",
            "tabs_bg",
            "tabs_text",
            "tabs_active_bg",
            "tabs_active_text",
            "line_numbers",
            "selection",
        ]:
            swatch = getattr(self, f"swatch_{key}")
            swatch.configure(bg=rgb_to_hex(getattr(self.state, key)))

    def _update_media_label(self):
        if self.state.bg_image:
            self.media_label.configure(text=f"Image: {os.path.basename(self.state.bg_image)}")
        else:
            self.media_label.configure(text="No media selected")

    def _apply_preview_image(self, left_width):
        self.bg_image_tk = None
        if not self.state.bg_image:
            return
        try:
            img = tk.PhotoImage(file=self.state.bg_image)
        except tk.TclError:
            self.media_label.configure(text="Image not supported by Tk (use PNG/GIF/PPM/PGM)")
            return

        max_width = 320
        max_height = 160
        w = img.width()
        h = img.height()
        scale = max(1, max(w // max_width, h // max_height))
        if scale > 1:
            img = img.subsample(scale, scale)

        self.bg_image_tk = img
        row = 4
        col = left_width + 4
        self.preview_text.image_create(f"{row}.{col}", image=self.bg_image_tk)

    def _export_theme(self):
        filename = filedialog.asksaveasfilename(
            title="Export Theme",
            defaultextension=".tasci",
            filetypes=[("TASCI Theme (*.tasci)", "*.tasci"), ("All files", "*.*")],
            initialfile="tasci-theme.tasci",
        )
        if not filename:
            return

        background = rgb_to_hex(self.state.background)
        accent = rgb_to_hex(self.state.accent)
        editor_bg = rgb_to_hex(self.state.editor_bg)
        editor = rgb_to_hex(self.state.editor_text)
        keyword = rgb_to_hex(self.state.keyword)
        sidebar = rgb_to_hex(self.state.sidebar)
        menu = rgb_to_hex(self.state.menu)
        status = rgb_to_hex(self.state.status)
        tabs_bg = rgb_to_hex(self.state.tabs_bg)
        tabs_text = rgb_to_hex(self.state.tabs_text)
        tabs_active_bg = rgb_to_hex(self.state.tabs_active_bg)
        tabs_active_text = rgb_to_hex(self.state.tabs_active_text)
        line_numbers = rgb_to_hex(self.state.line_numbers)
        selection = rgb_to_hex(self.state.selection)

        safe_name = escape_ts(self.state.name)
        safe_img = escape_ts(self.state.bg_image)
        try:
            with open(filename, "w", encoding="utf-8") as fp:
                fp.write("# TASCI Theme\n")
                fp.write(f"name: {safe_name}\n")
                fp.write(f"background: {background}\n")
                fp.write(f"accent: {accent}\n")
                fp.write(f"editorBackground: {editor_bg}\n")
                fp.write(f"editorText: {editor}\n")
                fp.write(f"keyword: {keyword}\n")
                fp.write(f"sidebar: {sidebar}\n")
                fp.write(f"menu: {menu}\n")
                fp.write(f"status: {status}\n")
                fp.write(f"tabsBackground: {tabs_bg}\n")
                fp.write(f"tabsText: {tabs_text}\n")
                fp.write(f"tabsActiveBackground: {tabs_active_bg}\n")
                fp.write(f"tabsActiveText: {tabs_active_text}\n")
                fp.write(f"lineNumbers: {line_numbers}\n")
                fp.write(f"selection: {selection}\n")
                if safe_img:
                    fp.write(f"backgroundImage: {safe_img}\n")
        except OSError as exc:
            messagebox.showerror("Export failed", str(exc), parent=self.root)


def main():
    root = tk.Tk()
    ThemeCreatorApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
