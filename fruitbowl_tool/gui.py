"""
Fruitbowl Resource Pack Tool — GUI
tkinter interface with Single Model, Batch Mode, and Manage Pack tabs.
"""

import os
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from .constants import KNOWN_ITEMS
from .settings import load_settings, save_settings
from .core import (
    sanitize_name, check_existing, needs_heading_name, scan_pack_items,
    add_to_pack,
)
from .manage import scan_all_models, delete_model
from .deploy import zip_pack, compute_sha1, update_server_properties, upload_to_github, deploy_full, get_pack_url


class FruitbowlApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Fruitbowl Resource Pack Tool")
        self.root.resizable(False, False)

        self.settings = load_settings()

        # ── Variables ────────────────────────────────────────────────────
        self.bbmodel_path = tk.StringVar()
        self.pack_path = tk.StringVar(value=self.settings.get("pack_path", ""))
        self.model_name = tk.StringVar()
        self.author_name = tk.StringVar()
        self.item_type = tk.StringVar()
        self.available_items: list[str] = list(KNOWN_ITEMS)

        # Batch state
        self.batch_entries: list[dict] = []

        # Manage state
        self.manage_models: list[dict] = []
        self.manage_filter = tk.StringVar()
        self.manage_filter.trace_add("write", lambda *_: self._manage_apply_filter())

        # ── Build UI ─────────────────────────────────────────────────────
        self._build_ui()

        if self.pack_path.get() and os.path.isdir(self.pack_path.get()):
            self._refresh_items()

    # ══════════════════════════════════════════════════════════════════════
    # UI CONSTRUCTION
    # ══════════════════════════════════════════════════════════════════════

    def _build_ui(self):
        pad = {"padx": 8, "pady": 4}
        wide_pad = {"padx": 8, "pady": (12, 4)}

        self.notebook = ttk.Notebook(self.root)
        self.notebook.grid(sticky="nsew", padx=8, pady=8)

        self.single_tab = ttk.Frame(self.notebook, padding=12)
        self.batch_tab = ttk.Frame(self.notebook, padding=12)
        self.manage_tab = ttk.Frame(self.notebook, padding=12)
        self.notebook.add(self.single_tab, text="  Single Model  ")
        self.notebook.add(self.batch_tab, text="  Batch Mode  ")
        self.notebook.add(self.manage_tab, text="  Manage Pack  ")
        self.deploy_tab = ttk.Frame(self.notebook, padding=12)
        self.notebook.add(self.deploy_tab, text="  Deploy  ")

        self._build_single_tab(pad, wide_pad)
        self._build_batch_tab(pad, wide_pad)
        self._build_manage_tab(pad, wide_pad)
        self._build_deploy_tab(pad, wide_pad)

    def _build_single_tab(self, pad, wide_pad):
        main = self.single_tab
        row = 0

        ttk.Label(main, text="Resource Pack Folder", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.pack_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(main, text="Browse…", width=10, command=self._browse_pack).grid(
            row=row, column=2, **pad)

        row += 1
        ttk.Label(main, text="BBModel File", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.bbmodel_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(main, text="Browse…", width=10, command=self._browse_bbmodel).grid(
            row=row, column=2, **pad)

        row += 1
        ttk.Label(main, text="Model Name (leave blank to use filename)", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.model_name, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)

        row += 1
        ttk.Label(main, text="Author (optional)", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(main, textvariable=self.author_name, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)

        row += 1
        ttk.Label(main, text="Minecraft Item", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.item_combo = ttk.Combobox(
            main, textvariable=self.item_type,
            values=self.available_items, width=52, state="normal")
        self.item_combo.grid(row=row, column=0, columnspan=2, sticky="ew", **pad)
        self.item_combo.bind("<KeyRelease>", lambda e: self._filter_combo(self.item_combo))

        row += 1
        btn_frame = ttk.Frame(main)
        btn_frame.grid(row=row, column=0, columnspan=3, pady=(16, 4))
        ttk.Button(btn_frame, text="  Add to Pack  ", command=self._run_single).pack(
            side="left", padx=4)

        row += 1
        ttk.Label(main, text="Output", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.log_text = tk.Text(main, height=10, width=65, state="disabled",
                                bg="#1e1e1e", fg="#cccccc", font=("Consolas", 9),
                                relief="flat", borderwidth=1)
        self.log_text.grid(row=row, column=0, columnspan=3, **pad)
        self._setup_log_tags(self.log_text)

    def _build_batch_tab(self, pad, wide_pad):
        batch = self.batch_tab
        row = 0

        ttk.Label(batch, text="Resource Pack Folder", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(batch, textvariable=self.pack_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(batch, text="Browse…", width=10, command=self._browse_pack).grid(
            row=row, column=2, **pad)

        row += 1
        ttk.Label(batch, text="Minecraft Item (applied to all files in batch)",
                  font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.batch_item_combo = ttk.Combobox(
            batch, textvariable=self.item_type,
            values=self.available_items, width=52, state="normal")
        self.batch_item_combo.grid(row=row, column=0, columnspan=2, sticky="ew", **pad)
        self.batch_item_combo.bind("<KeyRelease>",
                                   lambda e: self._filter_combo(self.batch_item_combo))

        row += 1
        ttk.Label(batch, text="BBModel Files  (double-click Author column to edit)",
                  font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)

        row += 1
        tree_frame = ttk.Frame(batch)
        tree_frame.grid(row=row, column=0, columnspan=2, sticky="nsew", **pad)

        self.batch_tree = ttk.Treeview(
            tree_frame, columns=("name", "author"), show="headings",
            height=9, selectmode="extended")
        self.batch_tree.heading("name", text="File → Model Name")
        self.batch_tree.heading("author", text="Author")
        self.batch_tree.column("name", width=330, minwidth=200)
        self.batch_tree.column("author", width=130, minwidth=80)

        tree_scroll = ttk.Scrollbar(tree_frame, orient="vertical",
                                     command=self.batch_tree.yview)
        self.batch_tree.configure(yscrollcommand=tree_scroll.set)
        self.batch_tree.pack(side="left", fill="both", expand=True)
        tree_scroll.pack(side="right", fill="y")

        self.batch_tree.bind("<Double-1>", self._batch_edit_author)

        btn_col = ttk.Frame(batch)
        btn_col.grid(row=row, column=2, sticky="n", **pad)
        ttk.Button(btn_col, text="Add Files…", width=12,
                   command=self._batch_add_files).pack(pady=2)
        ttk.Button(btn_col, text="Add Folder…", width=12,
                   command=self._batch_add_folder).pack(pady=2)
        ttk.Button(btn_col, text="Set Author…", width=12,
                   command=self._batch_set_author_selected).pack(pady=(10, 2))
        ttk.Button(btn_col, text="Remove", width=12,
                   command=self._batch_remove).pack(pady=(10, 2))
        ttk.Button(btn_col, text="Clear All", width=12,
                   command=self._batch_clear).pack(pady=2)

        row += 1
        self.batch_count_label = ttk.Label(batch, text="0 files loaded", font=("", 8))
        self.batch_count_label.grid(row=row, column=0, columnspan=2, sticky="w", **pad)

        row += 1
        batch_btn_frame = ttk.Frame(batch)
        batch_btn_frame.grid(row=row, column=0, columnspan=3, pady=(12, 4))
        ttk.Button(batch_btn_frame, text="  Add All to Pack  ",
                   command=self._run_batch).pack(side="left", padx=4)

        row += 1
        ttk.Label(batch, text="Output", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.batch_log = tk.Text(batch, height=10, width=65, state="disabled",
                                 bg="#1e1e1e", fg="#cccccc", font=("Consolas", 9),
                                 relief="flat", borderwidth=1)
        self.batch_log.grid(row=row, column=0, columnspan=3, **pad)
        self._setup_log_tags(self.batch_log)

    def _build_manage_tab(self, pad, wide_pad):
        manage = self.manage_tab
        row = 0

        # ── Top bar: pack path + scan ────────────────────────────────────
        ttk.Label(manage, text="Resource Pack Folder", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        ttk.Entry(manage, textvariable=self.pack_path, width=55).grid(
            row=row, column=0, columnspan=2, sticky="ew", **pad)
        ttk.Button(manage, text="Browse…", width=10, command=self._browse_pack).grid(
            row=row, column=2, **pad)

        # ── Search / filter bar ──────────────────────────────────────────
        row += 1
        filter_frame = ttk.Frame(manage)
        filter_frame.grid(row=row, column=0, columnspan=3, sticky="ew", **wide_pad)
        ttk.Label(filter_frame, text="Search:").pack(side="left")
        ttk.Entry(filter_frame, textvariable=self.manage_filter, width=30).pack(
            side="left", padx=(4, 12))
        ttk.Button(filter_frame, text="Scan Pack", command=self._manage_scan).pack(
            side="left", padx=4)
        self.manage_count_label = ttk.Label(filter_frame, text="", font=("", 8))
        self.manage_count_label.pack(side="left", padx=(12, 0))

        # ── Treeview ─────────────────────────────────────────────────────
        row += 1
        tree_frame = ttk.Frame(manage)
        tree_frame.grid(row=row, column=0, columnspan=2, sticky="nsew", **pad)

        cols = ("item_type", "model_name", "threshold", "author", "status")
        self.manage_tree = ttk.Treeview(
            tree_frame, columns=cols, show="headings",
            height=18, selectmode="extended")

        self.manage_tree.heading("item_type", text="Item Type",
                                 command=lambda: self._manage_sort("item_type"))
        self.manage_tree.heading("model_name", text="Model Name",
                                 command=lambda: self._manage_sort("model_name"))
        self.manage_tree.heading("threshold", text="#",
                                 command=lambda: self._manage_sort("threshold"))
        self.manage_tree.heading("author", text="Author",
                                 command=lambda: self._manage_sort("author"))
        self.manage_tree.heading("status", text="Status",
                                 command=lambda: self._manage_sort("status"))

        self.manage_tree.column("item_type", width=130, minwidth=90)
        self.manage_tree.column("model_name", width=180, minwidth=120)
        self.manage_tree.column("threshold", width=40, minwidth=30, anchor="center")
        self.manage_tree.column("author", width=120, minwidth=70)
        self.manage_tree.column("status", width=70, minwidth=50, anchor="center")

        tree_scroll = ttk.Scrollbar(tree_frame, orient="vertical",
                                     command=self.manage_tree.yview)
        self.manage_tree.configure(yscrollcommand=tree_scroll.set)
        self.manage_tree.pack(side="left", fill="both", expand=True)
        tree_scroll.pack(side="right", fill="y")

        # Tag for missing-file rows
        self.manage_tree.tag_configure("missing", foreground="#e8c85a")

        # ── Action buttons ───────────────────────────────────────────────
        btn_col = ttk.Frame(manage)
        btn_col.grid(row=row, column=2, sticky="n", **pad)
        ttk.Button(btn_col, text="Scan Pack", width=12,
                   command=self._manage_scan).pack(pady=2)
        ttk.Button(btn_col, text="Delete", width=12,
                   command=self._manage_delete).pack(pady=(16, 2))
        ttk.Button(btn_col, text="Update…", width=12,
                   command=self._manage_update).pack(pady=2)

        # ── Output log ───────────────────────────────────────────────────
        row += 1
        ttk.Label(manage, text="Output", font=("", 9, "bold")).grid(
            row=row, column=0, columnspan=3, sticky="w", **wide_pad)
        row += 1
        self.manage_log = tk.Text(manage, height=8, width=65, state="disabled",
                                  bg="#1e1e1e", fg="#cccccc", font=("Consolas", 9),
                                  relief="flat", borderwidth=1)
        self.manage_log.grid(row=row, column=0, columnspan=3, **pad)
        self._setup_log_tags(self.manage_log)

        # Sort state
        self._manage_sort_col = "item_type"
        self._manage_sort_reverse = False

    # ══════════════════════════════════════════════════════════════════════
    # SHARED HELPERS
    # ══════════════════════════════════════════════════════════════════════

    def _setup_log_tags(self, widget: tk.Text):
        widget.tag_configure("success", foreground="#6bcf6b")
        widget.tag_configure("warn", foreground="#e8c85a")
        widget.tag_configure("error", foreground="#e85a5a")
        widget.tag_configure("info", foreground="#7ab0df")
        widget.tag_configure("header", foreground="#ffffff",
                             font=("Consolas", 9, "bold"))

    def _log_to(self, widget: tk.Text, msg: str, tag: str = "info"):
        widget.configure(state="normal")
        widget.insert("end", msg + "\n", tag)
        widget.see("end")
        widget.configure(state="disabled")

    def _log_clear(self, widget: tk.Text):
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        widget.configure(state="disabled")

    def _browse_pack(self):
        path = filedialog.askdirectory(title="Select resource pack folder")
        if path:
            if not os.path.exists(os.path.join(path, "pack.mcmeta")):
                messagebox.showwarning(
                    "Not a resource pack",
                    "That folder doesn't contain a pack.mcmeta file.\n"
                    "Make sure you select the root of the resource pack.")
                return
            self.pack_path.set(path)
            self.settings["pack_path"] = path
            save_settings(self.settings)
            self._refresh_items()

    def _browse_bbmodel(self):
        path = filedialog.askopenfilename(
            title="Select .bbmodel file",
            filetypes=[("Blockbench Models", "*.bbmodel"), ("All Files", "*.*")])
        if path:
            self.bbmodel_path.set(path)
            self.model_name.set(sanitize_name(os.path.basename(path)))

    def _refresh_items(self):
        pack = self.pack_path.get()
        if pack and os.path.isdir(pack):
            self.available_items = scan_pack_items(pack)
            self.item_combo["values"] = self.available_items
            self.batch_item_combo["values"] = self.available_items

    def _filter_combo(self, combo: ttk.Combobox):
        typed = combo.get().lower()
        if not typed:
            combo["values"] = self.available_items
            return
        filtered = [item for item in self.available_items if typed in item]
        combo["values"] = filtered
        if filtered:
            combo.event_generate("<Down>")

    def _ask_heading_name(self, item_id: str) -> str | None:
        saved = self.settings.get("custom_headings", {}).get(item_id)
        if saved:
            return saved

        fallback = item_id.replace("_", " ").title()

        dialog = tk.Toplevel(self.root)
        dialog.title("New Item Type")
        dialog.resizable(False, False)
        dialog.grab_set()

        result = {"value": None}

        ttk.Label(dialog, text=f"'{item_id}' is new to the model list.",
                  padding=(12, 12, 12, 0)).pack()
        ttk.Label(dialog, text="Enter the heading name for model list.txt:",
                  padding=(12, 4, 12, 4)).pack()
        ttk.Label(dialog, text=f"(leave blank to use '{fallback}')",
                  font=("", 8), padding=(12, 0, 12, 4)).pack()

        name_var = tk.StringVar()
        entry = ttk.Entry(dialog, textvariable=name_var, width=35)
        entry.pack(padx=12, pady=4)
        entry.focus_set()

        def apply():
            result["value"] = name_var.get().strip()
            dialog.destroy()

        def cancel():
            result["value"] = None
            dialog.destroy()

        entry.bind("<Return>", lambda e: apply())
        entry.bind("<Escape>", lambda e: cancel())

        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(pady=(8, 12))
        ttk.Button(btn_frame, text="OK", command=apply, width=8).pack(
            side="left", padx=4)
        ttk.Button(btn_frame, text="Cancel", command=cancel, width=8).pack(
            side="left", padx=4)

        dialog.wait_window()
        return result["value"]

    # ══════════════════════════════════════════════════════════════════════
    # BATCH TAB — LIST MANAGEMENT
    # ══════════════════════════════════════════════════════════════════════

    def _batch_add_files(self):
        paths = filedialog.askopenfilenames(
            title="Select .bbmodel files",
            filetypes=[("Blockbench Models", "*.bbmodel"), ("All Files", "*.*")])
        if paths:
            existing_paths = {e["path"] for e in self.batch_entries}
            for p in paths:
                if p not in existing_paths:
                    self.batch_entries.append({"path": p, "author": ""})
            self._batch_refresh_tree()

    def _batch_add_folder(self):
        folder = filedialog.askdirectory(
            title="Select folder containing .bbmodel files")
        if folder:
            existing_paths = {e["path"] for e in self.batch_entries}
            for f in sorted(os.listdir(folder)):
                if f.lower().endswith(".bbmodel"):
                    full = os.path.join(folder, f)
                    if full not in existing_paths:
                        self.batch_entries.append({"path": full, "author": ""})
            self._batch_refresh_tree()

    def _batch_remove(self):
        selected = self.batch_tree.selection()
        if not selected:
            return
        indices = sorted([int(iid) for iid in selected], reverse=True)
        for i in indices:
            self.batch_entries.pop(i)
        self._batch_refresh_tree()

    def _batch_clear(self):
        self.batch_entries.clear()
        self._batch_refresh_tree()

    def _batch_set_author_selected(self):
        selected = self.batch_tree.selection()
        if not selected:
            messagebox.showinfo("No selection", "Select one or more rows first.")
            return

        dialog = tk.Toplevel(self.root)
        dialog.title("Set Author")
        dialog.resizable(False, False)
        dialog.grab_set()

        ttk.Label(dialog, text=f"Author for {len(selected)} selected model(s):",
                  padding=(12, 12, 12, 4)).pack()
        author_var = tk.StringVar()
        entry = ttk.Entry(dialog, textvariable=author_var, width=30)
        entry.pack(padx=12, pady=4)
        entry.focus_set()

        def apply():
            author = author_var.get().strip()
            for iid in selected:
                idx = int(iid)
                self.batch_entries[idx]["author"] = author
            self._batch_refresh_tree()
            dialog.destroy()

        entry.bind("<Return>", lambda e: apply())
        btn_frame = ttk.Frame(dialog)
        btn_frame.pack(pady=(8, 12))
        ttk.Button(btn_frame, text="Apply", command=apply).pack(side="left", padx=4)
        ttk.Button(btn_frame, text="Cancel", command=dialog.destroy).pack(
            side="left", padx=4)

    def _batch_edit_author(self, event):
        region = self.batch_tree.identify("region", event.x, event.y)
        if region != "cell":
            return
        column = self.batch_tree.identify_column(event.x)
        if column != "#2":
            return
        iid = self.batch_tree.identify_row(event.y)
        if not iid:
            return

        idx = int(iid)
        bbox = self.batch_tree.bbox(iid, column)
        if not bbox:
            return

        current_val = self.batch_entries[idx]["author"]
        entry_var = tk.StringVar(value=current_val)
        entry = ttk.Entry(self.batch_tree, textvariable=entry_var, width=15)
        entry.place(x=bbox[0], y=bbox[1], width=bbox[2], height=bbox[3])
        entry.focus_set()
        entry.select_range(0, "end")

        def finish(event=None):
            self.batch_entries[idx]["author"] = entry_var.get().strip()
            entry.destroy()
            self._batch_refresh_tree()

        entry.bind("<Return>", finish)
        entry.bind("<FocusOut>", finish)
        entry.bind("<Escape>", lambda e: entry.destroy())

    def _batch_refresh_tree(self):
        self.batch_tree.delete(*self.batch_tree.get_children())
        for i, entry in enumerate(self.batch_entries):
            basename = os.path.basename(entry["path"])
            model_nm = sanitize_name(basename)
            display = f"{basename}  →  {model_nm}"
            self.batch_tree.insert("", "end", iid=str(i),
                                   values=(display, entry["author"]))
        count = len(self.batch_entries)
        self.batch_count_label.configure(
            text=f"{count} file{'s' if count != 1 else ''} loaded")

    # ══════════════════════════════════════════════════════════════════════
    # SINGLE MODE — RUN
    # ══════════════════════════════════════════════════════════════════════

    def _run_single(self):
        self._log_clear(self.log_text)

        bbmodel = self.bbmodel_path.get().strip()
        pack = self.pack_path.get().strip()
        item = self.item_type.get().strip().lower().replace(" ", "_")
        name = self.model_name.get().strip() or None
        author = self.author_name.get().strip()

        if not pack:
            self._log_to(self.log_text, "ERROR: Select a resource pack folder first.", "error")
            return
        if not os.path.isdir(pack):
            self._log_to(self.log_text, f"ERROR: Pack folder not found: {pack}", "error")
            return
        if not bbmodel:
            self._log_to(self.log_text, "ERROR: Select a .bbmodel file first.", "error")
            return
        if not os.path.isfile(bbmodel):
            self._log_to(self.log_text, f"ERROR: File not found: {bbmodel}", "error")
            return
        if not item:
            self._log_to(self.log_text, "ERROR: Select a Minecraft item type.", "error")
            return

        model_nm = sanitize_name(name if name else os.path.basename(bbmodel))
        existing = check_existing(pack, model_nm)
        if existing:
            msg = (
                f"Model '{model_nm}' already exists in the pack.\n"
                f"The following files will be overwritten:\n\n"
                + "\n".join(f"  • {e}" for e in existing)
                + "\n\nContinue?")
            if not messagebox.askyesno("Overwrite existing model?", msg):
                self._log_to(self.log_text, "Cancelled by user.", "warn")
                return

        heading = ""
        if needs_heading_name(pack, item):
            heading = self._ask_heading_name(item)
            if heading is None:
                self._log_to(self.log_text, "Cancelled by user.", "warn")
                return
            if heading:
                self.settings.setdefault("custom_headings", {})[item] = heading
                save_settings(self.settings)

        try:
            messages = add_to_pack(bbmodel, pack, item, name, author, heading)
            for tag, msg in messages:
                self._log_to(self.log_text, msg, tag)
        except Exception as e:
            self._log_to(self.log_text, f"ERROR: {e}", "error")

    # ══════════════════════════════════════════════════════════════════════
    # BATCH MODE — RUN
    # ══════════════════════════════════════════════════════════════════════

    def _run_batch(self):
        self._log_clear(self.batch_log)

        pack = self.pack_path.get().strip()
        item = self.item_type.get().strip().lower().replace(" ", "_")

        if not pack:
            self._log_to(self.batch_log,
                         "ERROR: Select a resource pack folder first.", "error")
            return
        if not os.path.isdir(pack):
            self._log_to(self.batch_log,
                         f"ERROR: Pack folder not found: {pack}", "error")
            return
        if not item:
            self._log_to(self.batch_log,
                         "ERROR: Select a Minecraft item type.", "error")
            return
        if not self.batch_entries:
            self._log_to(self.batch_log,
                         "ERROR: No files loaded. Add some .bbmodel files first.", "error")
            return

        overwrites = []
        for entry in self.batch_entries:
            name = sanitize_name(os.path.basename(entry["path"]))
            existing = check_existing(pack, name)
            if existing:
                overwrites.append(name)

        if overwrites:
            msg = (
                f"{len(overwrites)} model(s) already exist and will be updated:\n\n"
                + "\n".join(f"  • {n}" for n in overwrites)
                + "\n\nContinue?")
            if not messagebox.askyesno("Overwrite existing models?", msg):
                self._log_to(self.batch_log, "Cancelled by user.", "warn")
                return

        heading = ""
        if needs_heading_name(pack, item):
            heading = self._ask_heading_name(item)
            if heading is None:
                self._log_to(self.batch_log, "Cancelled by user.", "warn")
                return
            if heading:
                self.settings.setdefault("custom_headings", {})[item] = heading
                save_settings(self.settings)

        total = len(self.batch_entries)
        success_count = 0
        update_count = 0
        fail_count = 0

        self._log_to(self.batch_log, f"Processing {total} file(s) → {item}", "header")
        self._log_to(self.batch_log, "─" * 50, "info")

        for i, entry in enumerate(self.batch_entries, 1):
            filepath = entry["path"]
            author = entry["author"]
            basename = os.path.basename(filepath)
            name = sanitize_name(basename)
            author_display = f"  by {author}" if author else ""
            self._log_to(self.batch_log,
                         f"[{i}/{total}] {basename} → {name}{author_display}", "header")

            try:
                existing = check_existing(pack, name)
                is_update = bool(existing)
                messages = add_to_pack(filepath, pack, item, name, author, heading)
                for tag, msg in messages:
                    self._log_to(self.batch_log, f"  {msg}", tag)
                if is_update:
                    update_count += 1
                else:
                    success_count += 1
            except Exception as e:
                self._log_to(self.batch_log, f"  ERROR: {e}", "error")
                fail_count += 1

        self._log_to(self.batch_log, "", "info")
        self._log_to(self.batch_log, "─" * 50, "info")
        parts = []
        if success_count:
            parts.append(f"{success_count} added")
        if update_count:
            parts.append(f"{update_count} updated")
        if fail_count:
            parts.append(f"{fail_count} failed")
        summary = f"Done! {', '.join(parts)}."
        tag = "error" if fail_count else "success"
        self._log_to(self.batch_log, summary, tag)
        self._log_to(self.batch_log, "Reload pack in-game with F3+T", "info")

    # ══════════════════════════════════════════════════════════════════════
    # MANAGE TAB
    # ══════════════════════════════════════════════════════════════════════

    def _manage_scan(self):
        self._log_clear(self.manage_log)
        pack = self.pack_path.get().strip()

        if not pack:
            self._log_to(self.manage_log,
                         "ERROR: Select a resource pack folder first.", "error")
            return
        if not os.path.isdir(pack):
            self._log_to(self.manage_log,
                         f"ERROR: Pack folder not found: {pack}", "error")
            return

        self.manage_models = scan_all_models(pack)
        self._manage_populate_tree()

        total = len(self.manage_models)
        missing = sum(1 for m in self.manage_models
                      if not (m["has_texture"] and m["has_model"] and m["has_item_def"]))

        self._log_to(self.manage_log,
                     f"Scanned {total} models across dispatch files.", "success")
        if missing:
            self._log_to(self.manage_log,
                         f"⚠ {missing} model(s) have missing files (highlighted).", "warn")

    def _manage_populate_tree(self):
        """Fill the treeview from self.manage_models, respecting current filter."""
        self.manage_tree.delete(*self.manage_tree.get_children())
        query = self.manage_filter.get().lower().strip()

        shown = 0
        for i, m in enumerate(self.manage_models):
            # Filter
            if query:
                searchable = f"{m['item_type']} {m['model_name']} {m['author']}".lower()
                if query not in searchable:
                    continue

            # Status column
            all_ok = m["has_texture"] and m["has_model"] and m["has_item_def"]
            if all_ok:
                status = "OK"
                tags = ()
            else:
                parts = []
                if not m["has_texture"]:
                    parts.append("tex")
                if not m["has_model"]:
                    parts.append("mdl")
                if not m["has_item_def"]:
                    parts.append("def")
                status = "✗ " + ",".join(parts)
                tags = ("missing",)

            self.manage_tree.insert(
                "", "end", iid=str(i),
                values=(m["item_type"], m["model_name"], m["threshold"],
                        m["author"], status),
                tags=tags)
            shown += 1

        total = len(self.manage_models)
        if query:
            self.manage_count_label.configure(
                text=f"{shown} of {total} models shown")
        else:
            self.manage_count_label.configure(text=f"{total} models")

    def _manage_apply_filter(self):
        if self.manage_models:
            self._manage_populate_tree()

    def _manage_sort(self, col: str):
        """Sort the manage treeview by clicking a column header."""
        if self._manage_sort_col == col:
            self._manage_sort_reverse = not self._manage_sort_reverse
        else:
            self._manage_sort_col = col
            self._manage_sort_reverse = False

        if col == "threshold":
            self.manage_models.sort(
                key=lambda m: m[col], reverse=self._manage_sort_reverse)
        else:
            self.manage_models.sort(
                key=lambda m: m.get(col, "").lower()
                if isinstance(m.get(col), str) else m.get(col, 0),
                reverse=self._manage_sort_reverse)

        self._manage_populate_tree()

    def _manage_delete(self):
        selected = self.manage_tree.selection()
        if not selected:
            messagebox.showinfo("No selection", "Select one or more models to delete.")
            return

        pack = self.pack_path.get().strip()
        if not pack or not os.path.isdir(pack):
            self._log_to(self.manage_log, "ERROR: Invalid pack path.", "error")
            return

        # Build description of what will be deleted
        to_delete = []
        for iid in selected:
            idx = int(iid)
            to_delete.append(self.manage_models[idx])

        if len(to_delete) == 1:
            m = to_delete[0]
            desc = f"Delete '{m['model_name']}' (#{m['threshold']} in {m['item_type']})?"
        else:
            desc = f"Delete {len(to_delete)} models?\n\n"
            for m in to_delete[:10]:
                desc += f"  • {m['model_name']} (#{m['threshold']} in {m['item_type']})\n"
            if len(to_delete) > 10:
                desc += f"  … and {len(to_delete) - 10} more"

        desc += "\n\nThis will remove all associated files (texture, model, item def,\n"
        desc += "dispatch entry, and model list entry). This cannot be undone."

        if not messagebox.askyesno("Confirm Delete", desc):
            return

        self._log_clear(self.manage_log)
        self._log_to(self.manage_log,
                     f"Deleting {len(to_delete)} model(s)…", "header")
        self._log_to(self.manage_log, "─" * 50, "info")

        for m in to_delete:
            self._log_to(self.manage_log,
                         f"  {m['model_name']} (#{m['threshold']} in {m['item_type']})",
                         "header")
            try:
                messages = delete_model(pack, m["item_type"], m["model_name"],
                                        m["threshold"])
                for tag, msg in messages:
                    self._log_to(self.manage_log, f"    {msg}", tag)
            except Exception as e:
                self._log_to(self.manage_log, f"    ERROR: {e}", "error")

        self._log_to(self.manage_log, "─" * 50, "info")
        self._log_to(self.manage_log, "Done. Rescan to refresh the list.", "success")

        # Auto-rescan
        self.manage_models = scan_all_models(pack)
        self._manage_populate_tree()

    def _manage_update(self):
        """Update a model by re-importing from a .bbmodel file."""
        selected = self.manage_tree.selection()
        if not selected:
            messagebox.showinfo("No selection", "Select a model to update.")
            return
        if len(selected) > 1:
            messagebox.showinfo("Single select",
                                "Select one model at a time for update.")
            return

        pack = self.pack_path.get().strip()
        if not pack or not os.path.isdir(pack):
            self._log_to(self.manage_log, "ERROR: Invalid pack path.", "error")
            return

        idx = int(selected[0])
        m = self.manage_models[idx]

        # Ask for the new .bbmodel file
        bbmodel_path = filedialog.askopenfilename(
            title=f"Select .bbmodel to update '{m['model_name']}'",
            filetypes=[("Blockbench Models", "*.bbmodel"), ("All Files", "*.*")])
        if not bbmodel_path:
            return

        self._log_clear(self.manage_log)
        self._log_to(self.manage_log,
                     f"Updating '{m['model_name']}' from {os.path.basename(bbmodel_path)}…",
                     "header")

        try:
            messages = add_to_pack(
                bbmodel_path, pack, m["item_type"],
                model_name=m["model_name"], author=m["author"])
            for tag, msg in messages:
                self._log_to(self.manage_log, msg, tag)
        except Exception as e:
            self._log_to(self.manage_log, f"ERROR: {e}", "error")

        # Auto-rescan
        self.manage_models = scan_all_models(pack)
        self._manage_populate_tree()



    def _build_deploy_tab(self, pad, wide_pad):
        deploy = self.deploy_tab
        row = 0

        # -- Server directory --
        # -- Resource Pack directory --
        ttk.Label(deploy, text="Resource Pack Directory:").grid(
            row=row, column=0, sticky="w", **pad)
        self.deploy_pack_var = tk.StringVar(
            value=self.settings.get("pack_path", ""))
        entry_pack = ttk.Entry(deploy, textvariable=self.deploy_pack_var, width=52)
        entry_pack.grid(row=row, column=1, sticky="ew", **pad)
        ttk.Button(deploy, text="Browse\u2026",
                   command=self._deploy_browse_pack).grid(
            row=row, column=2, **pad)
        row += 1

        ttk.Label(deploy, text="Minecraft Server Directory:").grid(
            row=row, column=0, sticky="w", **pad)
        self.deploy_server_var = tk.StringVar(
            value=self.settings.get("server_dir", ""))
        entry = ttk.Entry(deploy, textvariable=self.deploy_server_var, width=52)
        entry.grid(row=row, column=1, sticky="ew", **pad)
        ttk.Button(deploy, text="Browse…",
                   command=self._deploy_browse_server).grid(
            row=row, column=2, **pad)
        row += 1

        # -- Buttons row --
        btn_frame = ttk.Frame(deploy)
        btn_frame.grid(row=row, column=0, columnspan=3, sticky="ew", pady=(10, 5))

        ttk.Button(btn_frame, text="📦 Zip Pack",
                   command=self._deploy_zip).pack(side="left", padx=(0, 5))
        ttk.Button(btn_frame, text="🚀 Deploy to Server",
                   command=self._deploy_full).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="🔄 Restart Server",
                   command=self._deploy_restart).pack(side="left", padx=5)
        row += 1

        # -- Info labels --
        info_frame = ttk.LabelFrame(deploy, text="Pack Info", padding=8)
        info_frame.grid(row=row, column=0, columnspan=3, sticky="ew", pady=(10, 5))

        ttk.Label(info_frame, text="SHA1:").grid(row=0, column=0, sticky="w")
        self.deploy_sha1_var = tk.StringVar(value="—")
        sha1_label = ttk.Label(info_frame, textvariable=self.deploy_sha1_var,
                               font=("Consolas", 9))
        sha1_label.grid(row=0, column=1, sticky="w", padx=(5, 0))

        ttk.Label(info_frame, text="URL:").grid(row=1, column=0, sticky="w")
        self.deploy_url_var = tk.StringVar(value="—")
        url_label = ttk.Label(info_frame, textvariable=self.deploy_url_var,
                              font=("Consolas", 9))
        url_label.grid(row=1, column=1, sticky="w", padx=(5, 0))

        ttk.Label(info_frame, text="Size:").grid(row=2, column=0, sticky="w")
        self.deploy_size_var = tk.StringVar(value="—")
        ttk.Label(info_frame, textvariable=self.deploy_size_var).grid(
            row=2, column=1, sticky="w", padx=(5, 0))
        row += 1

        # -- Log area --
        self.deploy_log = tk.Text(deploy, height=14, width=80,
                                  state="disabled", wrap="word",
                                  bg="#1e1e1e", fg="#d4d4d4",
                                  font=("Consolas", 9))
        self.deploy_log.grid(row=row, column=0, columnspan=3,
                             sticky="nsew", **pad)
        self.deploy_log.tag_configure("success", foreground="#4ec9b0")
        self.deploy_log.tag_configure("error", foreground="#f44747")
        self.deploy_log.tag_configure("warn", foreground="#dcdcaa")
        self.deploy_log.tag_configure("info", foreground="#9cdcfe")
        self.deploy_log.tag_configure("header", foreground="#569cd6",
                                      font=("Consolas", 9, "bold"))

        deploy.columnconfigure(1, weight=1)
        deploy.rowconfigure(row, weight=1)

    def _deploy_browse_server(self):
        d = filedialog.askdirectory(title="Select Minecraft Server Directory")
        if d:
            self.deploy_server_var.set(d)
            self.settings["server_dir"] = d
            save_settings(self.settings)

    def _deploy_browse_pack(self):
        d = filedialog.askdirectory(title="Select Resource Pack Directory")
        if d:
            self.deploy_pack_var.set(d)
            self.settings["pack_path"] = d
            save_settings(self.settings)

    def _deploy_zip(self):
        pack = self.deploy_pack_var.get() or self.settings.get("pack_path", "")
        if not pack or not os.path.isdir(pack):
            messagebox.showerror("Error", "Set a valid pack path first (Single Model tab).")
            return

        self._log_clear(self.deploy_log)
        self._log_to(self.deploy_log, "Zipping pack…", "header")

        try:
            zip_path = zip_pack(pack)
            sha1 = compute_sha1(zip_path)
            size = os.path.getsize(zip_path)

            self.deploy_sha1_var.set(sha1)
            self.deploy_url_var.set(get_pack_url())
            self.deploy_size_var.set(f"{size / 1024:.0f} KB")

            self._log_to(self.deploy_log, f"✓ Zip created: {zip_path}", "success")
            self._log_to(self.deploy_log, f"✓ SHA1: {sha1}", "success")
            self._log_to(self.deploy_log, f"✓ Size: {size // 1024} KB", "success")
            self._log_to(self.deploy_log, "", "info")
            self._log_to(self.deploy_log,
                         "Click 'Deploy to Server' to upload and apply.",
                         "info")

            # Store for later use
            self._deploy_zip_path = zip_path
            self._deploy_sha1 = sha1

        except Exception as e:
            self._log_to(self.deploy_log, f"ERROR: {e}", "error")

    def _deploy_full(self):
        pack = self.deploy_pack_var.get() or self.settings.get("pack_path", "")
        server_dir = self.deploy_server_var.get()

        if not pack or not os.path.isdir(pack):
            messagebox.showerror("Error", "Set a valid pack path first (Single Model tab).")
            return
        if not server_dir or not os.path.isdir(server_dir):
            messagebox.showerror("Error", "Set a valid Minecraft server directory.")
            return

        props_path = os.path.join(server_dir, "server.properties")
        if not os.path.exists(props_path):
            messagebox.showerror("Error",
                                 f"server.properties not found in:\n{server_dir}")
            return

        self._log_clear(self.deploy_log)
        self._log_to(self.deploy_log, "Deploying to server…", "header")

        try:
            msgs = deploy_full(pack, server_dir)
            for tag, msg in msgs:
                self._log_to(self.deploy_log, msg, tag)

            # Update info labels
            zip_path = os.path.join(os.path.dirname(pack), "fruitbowl-server-pack.zip")
            if os.path.exists(zip_path):
                sha1 = compute_sha1(zip_path)
                size = os.path.getsize(zip_path)
                self.deploy_sha1_var.set(sha1)
                self.deploy_url_var.set(get_pack_url())
                self.deploy_size_var.set(f"{size / 1024:.0f} KB")

        except Exception as e:
            self._log_to(self.deploy_log, f"ERROR: {e}", "error")

    def _deploy_restart(self):
        self._log_clear(self.deploy_log)
        self._log_to(self.deploy_log, "Restarting Minecraft server…", "header")

        try:
            result = subprocess.run(
                ["systemctl", "--user", "restart", "minecraft.service"],
                capture_output=True, text=True, timeout=15)
            if result.returncode == 0:
                self._log_to(self.deploy_log,
                             "✓ Server restart command sent.", "success")
            else:
                self._log_to(self.deploy_log,
                             f"⚠ Restart returned code {result.returncode}", "warn")
                if result.stderr:
                    self._log_to(self.deploy_log, result.stderr.strip(), "error")
        except FileNotFoundError:
            self._log_to(self.deploy_log,
                         "⚠ systemctl not found — restart server manually.", "warn")
        except subprocess.TimeoutExpired:
            self._log_to(self.deploy_log,
                         "✓ Restart command sent (timed out waiting for response).",
                         "success")
        except Exception as e:
            self._log_to(self.deploy_log, f"ERROR: {e}", "error")


def main():
    root = tk.Tk()

    try:
        style = ttk.Style()
        if "vista" in style.theme_names():
            style.theme_use("vista")
        elif "clam" in style.theme_names():
            style.theme_use("clam")
    except Exception:
        pass

    FruitbowlApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
