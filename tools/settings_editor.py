# settings_editor.py
#
# ToyGame/Settings/System/ 配下の実行環境ごとの設定ファイル
# (Application_Settings.json / Renderer_Settings.json / InputConfig.json)
# を生成・編集するための開発者向け簡易ツール。
#
# Application / Renderer タブはフィールド単位のフォーム編集。
# InputConfig タブは項目が多く構造も複雑なので、引き続き生JSONテキスト編集。
#
# 各タブ共通の操作:
# - Load    : System 側があれば読み込み、無ければデフォルト値をテンプレートとして表示
# - Save    : System 側のパスに書き出す（無ければ生成）
# - デフォルトを表示 : デフォルト値を読み込み直す（保存はしない）
#
# 依存は標準ライブラリのみ（tkinter / json）。
import json
import os
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def get_path(data, path, default=None):
    cur = data
    for key in path:
        if not isinstance(cur, dict) or key not in cur:
            return default
        cur = cur[key]
    return cur


def set_path(data, path, value):
    cur = data
    for key in path[:-1]:
        cur = cur.setdefault(key, {})
    cur[path[-1]] = value


class ScrollableFrame(ttk.Frame):
    """マウスホイール対応の縦スクロール可能な入れ物。中身は self.inner に配置する。"""

    def __init__(self, parent):
        super().__init__(parent)
        self.canvas = tk.Canvas(self, highlightthickness=0)
        scrollbar = ttk.Scrollbar(self, orient="vertical", command=self.canvas.yview)
        self.inner = ttk.Frame(self.canvas)

        self.inner.bind(
            "<Configure>",
            lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all")),
        )

        self.canvas.create_window((0, 0), window=self.inner, anchor="nw")
        self.canvas.configure(yscrollcommand=scrollbar.set)

        self.canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        self.canvas.bind("<Enter>", lambda e: self._bind_mousewheel())
        self.canvas.bind("<Leave>", lambda e: self._unbind_mousewheel())

    def _bind_mousewheel(self):
        self.canvas.bind_all("<MouseWheel>", self._on_mousewheel)
        self.canvas.bind_all("<Button-4>", self._on_mousewheel)
        self.canvas.bind_all("<Button-5>", self._on_mousewheel)

    def _unbind_mousewheel(self):
        self.canvas.unbind_all("<MouseWheel>")
        self.canvas.unbind_all("<Button-4>")
        self.canvas.unbind_all("<Button-5>")

    def _on_mousewheel(self, event):
        if event.num == 4:
            self.canvas.yview_scroll(-1, "units")
        elif event.num == 5:
            self.canvas.yview_scroll(1, "units")
        else:
            self.canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")


class RawJsonTab(ttk.Frame):
    """生JSONテキストとして編集するタブ（InputConfigなど構造が複雑なもの向け）。"""

    def __init__(self, parent, label, default_rel_path, user_rel_path):
        super().__init__(parent, padding=8)
        self.label = label
        self.default_path = os.path.join(REPO_ROOT, default_rel_path)
        self.user_path = os.path.join(REPO_ROOT, user_rel_path)
        self.user_rel_path = user_rel_path

        self.status_var = tk.StringVar()

        button_row = ttk.Frame(self)
        button_row.pack(fill=tk.X, pady=(0, 6))
        ttk.Button(button_row, text="Load", command=self.load).pack(side=tk.LEFT)
        ttk.Button(button_row, text="Save", command=self.save).pack(side=tk.LEFT, padx=6)
        ttk.Button(button_row, text="デフォルトを表示", command=self.show_default).pack(side=tk.LEFT)

        self.text = tk.Text(self, wrap="none", undo=True, font=("Menlo", 12))
        self.text.pack(fill=tk.BOTH, expand=True)

        ttk.Label(self, textvariable=self.status_var, foreground="#555").pack(
            fill=tk.X, pady=(4, 0), anchor="w"
        )

        self.load()

    def _set_text(self, data):
        self.text.delete("1.0", tk.END)
        self.text.insert("1.0", json.dumps(data, indent=2, ensure_ascii=False))

    def _read_json_file(self, path):
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)

    def load(self):
        try:
            if os.path.exists(self.user_path):
                data = self._read_json_file(self.user_path)
                self._set_text(data)
                self.status_var.set(f"読み込み: {self.user_rel_path}")
            else:
                data = self._read_json_file(self.default_path)
                self._set_text(data)
                self.status_var.set(
                    f"System設定が無いのでデフォルト値を表示中（未保存）: {self.user_rel_path}"
                )
        except Exception as e:
            messagebox.showerror(self.label, f"読み込みに失敗しました:\n{e}")
            self.status_var.set("読み込みエラー")

    def show_default(self):
        try:
            data = self._read_json_file(self.default_path)
            self._set_text(data)
            self.status_var.set("デフォルト値を表示中（未保存）")
        except Exception as e:
            messagebox.showerror(self.label, f"デフォルト設定の読み込みに失敗しました:\n{e}")

    def save(self):
        text = self.text.get("1.0", tk.END)
        try:
            data = json.loads(text)
        except json.JSONDecodeError as e:
            messagebox.showerror(self.label, f"JSONとして解釈できません:\n{e}")
            return

        try:
            os.makedirs(os.path.dirname(self.user_path), exist_ok=True)
            with open(self.user_path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            self.status_var.set(f"保存しました: {self.user_rel_path}")
        except Exception as e:
            messagebox.showerror(self.label, f"保存に失敗しました:\n{e}")


class FormTab(ttk.Frame):
    """フィールド単位のフォームとして編集するタブ。

    field_groups: [(見出し文字列 or None, [(json_path_tuple, ラベル, 種別, extra_dict), ...]), ...]
    種別: "str" / "int" / "float" / "bool" / "choice" (extra["choices"]) / "vec3" / "color3"
    """

    def __init__(self, parent, label, default_rel_path, user_rel_path, field_groups):
        super().__init__(parent, padding=8)
        self.label = label
        self.default_path = os.path.join(REPO_ROOT, default_rel_path)
        self.user_path = os.path.join(REPO_ROOT, user_rel_path)
        self.user_rel_path = user_rel_path
        self.field_groups = field_groups
        self.data = {}
        self.field_readers = []  # [(json_path_tuple, () -> value)]

        button_row = ttk.Frame(self)
        button_row.pack(fill=tk.X, pady=(0, 6))
        ttk.Button(button_row, text="Load", command=self.load).pack(side=tk.LEFT)
        ttk.Button(button_row, text="Save", command=self.save).pack(side=tk.LEFT, padx=6)
        ttk.Button(button_row, text="デフォルトを表示", command=self.show_default).pack(side=tk.LEFT)

        self.status_var = tk.StringVar()
        ttk.Label(self, textvariable=self.status_var, foreground="#555").pack(
            fill=tk.X, pady=(0, 6), anchor="w"
        )

        self.scroll = ScrollableFrame(self)
        self.scroll.pack(fill=tk.BOTH, expand=True)

        self.load()

    def _read_json_file(self, path):
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)

    def load(self):
        try:
            if os.path.exists(self.user_path):
                self.data = self._read_json_file(self.user_path)
                self.status_var.set(f"読み込み: {self.user_rel_path}")
            else:
                self.data = self._read_json_file(self.default_path)
                self.status_var.set(
                    f"System設定が無いのでデフォルト値を表示中（未保存）: {self.user_rel_path}"
                )
        except Exception as e:
            messagebox.showerror(self.label, f"読み込みに失敗しました:\n{e}")
            self.data = {}
        self._rebuild_form()

    def show_default(self):
        try:
            self.data = self._read_json_file(self.default_path)
            self.status_var.set("デフォルト値を表示中（未保存）")
        except Exception as e:
            messagebox.showerror(self.label, f"デフォルト設定の読み込みに失敗しました:\n{e}")
            return
        self._rebuild_form()

    def save(self):
        try:
            for path, reader in self.field_readers:
                set_path(self.data, path, reader())
        except ValueError as e:
            messagebox.showerror(self.label, f"入力値が不正です:\n{e}")
            return

        try:
            os.makedirs(os.path.dirname(self.user_path), exist_ok=True)
            with open(self.user_path, "w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=2, ensure_ascii=False)
            self.status_var.set(f"保存しました: {self.user_rel_path}")
        except Exception as e:
            messagebox.showerror(self.label, f"保存に失敗しました:\n{e}")

    def _rebuild_form(self):
        for child in self.scroll.inner.winfo_children():
            child.destroy()
        self.field_readers = []

        for group_title, fields in self.field_groups:
            if group_title:
                container = ttk.LabelFrame(self.scroll.inner, text=group_title)
            else:
                container = ttk.Frame(self.scroll.inner)
            container.pack(fill=tk.X, padx=4, pady=4, anchor="w")

            for row, (path, label, kind, extra) in enumerate(fields):
                self._build_field(container, row, path, label, kind, extra)

    def _build_field(self, parent, row, path, label, kind, extra):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(4, 8), pady=2)
        value = get_path(self.data, path)

        if kind == "str":
            var = tk.StringVar(value="" if value is None else str(value))
            ttk.Entry(parent, textvariable=var, width=40).grid(row=row, column=1, sticky="w", pady=2)
            self.field_readers.append((path, lambda v=var: v.get()))

        elif kind == "int":
            var = tk.StringVar(value="0" if value is None else str(value))
            ttk.Entry(parent, textvariable=var, width=12).grid(row=row, column=1, sticky="w", pady=2)
            self.field_readers.append((path, lambda v=var: int(v.get())))

        elif kind == "float":
            var = tk.StringVar(value="0.0" if value is None else str(value))
            ttk.Entry(parent, textvariable=var, width=12).grid(row=row, column=1, sticky="w", pady=2)
            self.field_readers.append((path, lambda v=var: float(v.get())))

        elif kind == "bool":
            var = tk.BooleanVar(value=bool(value))
            ttk.Checkbutton(parent, variable=var).grid(row=row, column=1, sticky="w", pady=2)
            self.field_readers.append((path, lambda v=var: bool(v.get())))

        elif kind == "choice":
            var = tk.StringVar(value="" if value is None else str(value))
            ttk.Combobox(
                parent, textvariable=var, values=extra["choices"], state="readonly", width=10
            ).grid(row=row, column=1, sticky="w", pady=2)
            self.field_readers.append((path, lambda v=var: v.get()))

        elif kind == "vec3":
            vec = value if isinstance(value, list) and len(value) == 3 else [0.0, 0.0, 0.0]
            vars3 = [tk.StringVar(value=str(vec[i])) for i in range(3)]
            frame = ttk.Frame(parent)
            frame.grid(row=row, column=1, sticky="w", pady=2)
            for i in range(3):
                ttk.Entry(frame, textvariable=vars3[i], width=8).pack(side=tk.LEFT, padx=2)
            self.field_readers.append((path, lambda vs=vars3: [float(v.get()) for v in vs]))

        elif kind == "color3":
            vec = value if isinstance(value, list) and len(value) == 3 else [1.0, 1.0, 1.0]
            vars3 = [tk.StringVar(value=str(vec[i])) for i in range(3)]
            frame = ttk.Frame(parent)
            frame.grid(row=row, column=1, sticky="w", pady=2)
            for i in range(3):
                ttk.Entry(frame, textvariable=vars3[i], width=6).pack(side=tk.LEFT, padx=2)
            swatch = tk.Label(frame, text="  ", relief="sunken", width=3)
            swatch.pack(side=tk.LEFT, padx=(6, 2))

            def update_swatch():
                try:
                    rgb = [max(0.0, min(1.0, float(v.get()))) for v in vars3]
                    swatch.configure(
                        bg="#%02x%02x%02x" % tuple(int(c * 255) for c in rgb)
                    )
                except ValueError:
                    pass

            def pick(vs=vars3):
                update_swatch()
                result = colorchooser.askcolor(color=swatch.cget("bg"), parent=self)
                if result and result[0]:
                    r, g, b = result[0]
                    vs[0].set(f"{r / 255:.3f}")
                    vs[1].set(f"{g / 255:.3f}")
                    vs[2].set(f"{b / 255:.3f}")
                    update_swatch()

            ttk.Button(frame, text="選択...", command=pick).pack(side=tk.LEFT, padx=(6, 0))
            update_swatch()
            self.field_readers.append((path, lambda vs=vars3: [float(v.get()) for v in vs]))

        else:
            raise ValueError(f"unknown field kind: {kind}")


APPLICATION_FIELDS = [
    (None, [
        (("title",), "タイトル", "str", {}),
        (("asset_path",), "アセットパス", "str", {}),
        (("renderer_backend",), "レンダラー", "choice", {"choices": ["VK", "GL"]}),
        (("target_fps",), "ターゲットFPS (0=無制限)", "int", {}),
    ]),
    ("画面", [
        (("screen", "screen_width"), "幅", "int", {}),
        (("screen", "screen_height"), "高さ", "int", {}),
        (("screen", "fullscreen"), "フルスクリーン", "bool", {}),
        (("screen", "fullscreen_use_setting_resolution"), "フルスクリーン時もこの解像度を使う", "bool", {}),
    ]),
    ("デバッグ", [
        (("debug", "enabled"), "デバッグ機能を有効化", "bool", {}),
    ]),
]

RENDERER_FIELDS = [
    (None, [
        (("shader_path",), "シェーダーパス", "str", {}),
        (("vsync",), "VSync", "bool", {}),
        (("perspectiveFOV",), "視野角(FOV)", "float", {}),
    ]),
    ("仮想解像度", [
        (("screen", "virtual_with"), "幅", "float", {}),
        (("screen", "virtual_height"), "高さ", "float", {}),
    ]),
    ("カメラ", [
        (("camera", "position"), "初期位置 (x, y, z)", "vec3", {}),
    ]),
    ("カラー", [
        (("clearColor",), "背景色", "color3", {}),
        (("wireColor",), "ワイヤーフレーム色", "color3", {}),
        (("obbColor",), "OBB色", "color3", {}),
        (("ambient",), "アンビエント", "color3", {}),
        (("specular",), "スペキュラ", "color3", {}),
    ]),
    ("ディレクショナルライト", [
        (("directionalLight", "diffuse"), "拡散色", "color3", {}),
        (("directionalLight", "position"), "位置 (x, y, z)", "vec3", {}),
        (("directionalLight", "target"), "注視点 (x, y, z)", "vec3", {}),
    ]),
    ("フォグ", [
        (("fog", "maxDist"), "最大距離", "float", {}),
        (("fog", "minDist"), "最小距離", "float", {}),
        (("fog", "color"), "色", "color3", {}),
    ]),
    ("シャドウ", [
        (("shadow", "enable"), "有効化", "bool", {}),
        (("shadow", "near"), "Near", "float", {}),
        (("shadow", "far"), "Far", "float", {}),
        (("shadow", "bias"), "バイアス", "float", {}),
        (("shadow", "ortho_width"), "Ortho幅", "float", {}),
        (("shadow", "ortho_height"), "Ortho高さ", "float", {}),
        (("shadow", "resolution_width"), "解像度(幅)", "int", {}),
        (("shadow", "resolution_height"), "解像度(高さ)", "int", {}),
    ]),
    ("Validation Layer (VK専用)", [
        (("validation", "enable"), "有効化", "bool", {}),
        (("validation", "gpu_assisted"), "GPU-Assisted Validation", "bool", {}),
    ]),
]


def main():
    root = tk.Tk()
    root.title("ToyLibGame Settings Editor")
    root.geometry("900x700")

    notebook = ttk.Notebook(root)
    notebook.pack(fill=tk.BOTH, expand=True)

    notebook.add(
        FormTab(notebook, "Application",
                "ToyLib/Settings/Application_Settings.json",
                "ToyGame/Settings/System/Application_Settings.json",
                APPLICATION_FIELDS),
        text="Application",
    )
    notebook.add(
        FormTab(notebook, "Renderer",
                "ToyLib/Settings/Renderer_Settings.json",
                "ToyGame/Settings/System/Renderer_Settings.json",
                RENDERER_FIELDS),
        text="Renderer",
    )
    notebook.add(
        RawJsonTab(notebook, "InputConfig",
                   "ToyLib/Settings/InputConfig.json",
                   "ToyGame/Settings/System/InputConfig.json"),
        text="InputConfig",
    )

    root.mainloop()


if __name__ == "__main__":
    main()
