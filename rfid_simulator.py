import tkinter as tk
from tkinter import ttk
from datetime import datetime
import time
import json
import threading
import os
from urllib import parse, request, error


class RFIDSimulatorApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Simulador Caixa RFID")
        self.root.resizable(False, False)
        self.root.configure(bg="#2d2d2d")

        self.case_color = "#78e26b"
        self.case_shadow = "#5ac150"
        self.button_face = "#6ad85e"
        self.button_shadow = "#4aab42"

        self.last_pressed = -1
        self.last_rfid_code = "----"
        self.device_ip = "192.168.1.120"
        self.cached_battery = 50
        self.cached_rssi = -58
        self.wifi_connected = True

        self.screen_mode = "default"
        self.temp_line1 = ""
        self.temp_line2 = ""
        self.temp_until_ms = 0

        self.sample_barco_ordem = "OF-01010-2026"
        self.sample_operador = "Joao Ferreira"

        self._load_dotenv_file()
        self.supabase_url = os.getenv("SUPABASE_URL", "").strip()
        self.supabase_key = os.getenv("SUPABASE_KEY", "").strip()
        self.supabase_configured = bool(self.supabase_url and self.supabase_key)

        self.pressed_button = None
        self.button_areas = {
            1: (627, 273, 727, 373),
            2: (507, 273, 607, 373),
            3: (747, 273, 847, 373),
        }
        self.display_area = (485, 58, 844, 206)
        self.rfid_touch_area = (128, 84, 330, 286)
        self.manual_input_area = (64, 320, 402, 378)
        self.background_image = None
        self.manual_code_var = tk.StringVar(value="")
        self.rfid_dot_visible = True
        self.rfid_dot_color = "#1ec15c"
        self.rfid_dot_area = (268, 95, 282, 109)

        self.oled_x = self.display_area[0] + 8
        self.oled_y = self.display_area[1] + 8
        self.oled_w = self.display_area[2] - self.display_area[0] - 16
        self.oled_h = self.display_area[3] - self.display_area[1] - 16
        self.scale_x = self.oled_w / 128.0
        self.scale_y = self.oled_h / 64.0

        self.canvas = tk.Canvas(
            self.root,
            width=920,
            height=460,
            bg="#071126",
            highlightthickness=0,
        )
        self.canvas.pack(padx=20, pady=16)

        self.canvas.bind("<Button-1>", self._on_canvas_click)
        self.root.bind("<Up>", lambda _e: self._press_button(2))
        self.root.bind("<Return>", self._on_global_return)
        self.root.bind("<Down>", lambda _e: self._press_button(3))

        self.manual_entry = ttk.Entry(
            self.canvas,
            textvariable=self.manual_code_var,
            font=("Consolas", 19, "bold"),
            justify="center",
        )
        self.manual_entry.bind("<Return>", self._on_manual_entry_return)

        self._start_periodic_refresh()
        self._draw_case()

    def _start_periodic_refresh(self) -> None:
        self._tick()

    def _tick(self) -> None:
        now_ms = int(time.monotonic() * 1000)
        if self.temp_until_ms and now_ms > self.temp_until_ms:
            self.screen_mode = "default"
            self.temp_until_ms = 0

        self._draw_case()
        self.root.after(300, self._tick)

    def _to_px(self, x: int, y: int) -> tuple[float, float]:
        return self.oled_x + x * self.scale_x, self.oled_y + y * self.scale_y

    def _draw_case(self) -> None:
        self.canvas.delete("all")

        has_background = self._try_draw_background_image()

        if not has_background:
            self.canvas.create_rectangle(8, 8, 912, 452, fill="#0a1329", outline="")
            self.canvas.create_rectangle(26, 30, 890, 430, fill="#9ea9bc", outline="#8594ad", width=2)
            self.canvas.create_rectangle(24, 28, 892, 432, outline="#b8c2d3", width=14)
            self.canvas.create_line(446, 36, 446, 424, fill="#aeb8c7", width=4)
            self.canvas.create_oval(34, 34, 48, 48, fill="#7f8fa6", outline="#72839a")
            self.canvas.create_oval(864, 34, 878, 48, fill="#7f8fa6", outline="#72839a")
            self.canvas.create_oval(34, 392, 48, 406, fill="#7f8fa6", outline="#72839a")
            self.canvas.create_oval(864, 392, 878, 406, fill="#7f8fa6", outline="#72839a")

        for button_id, area in self.button_areas.items():
            self._draw_button(button_id, area)

        if not has_background:
            self._draw_rfid_emblem()
        self._draw_display_cutout()
        self._draw_manual_input_hint()
        self._draw_rfid_status_dot()

    def _try_draw_background_image(self) -> None:
        if self.background_image is not None:
            self.canvas.create_image(0, 0, image=self.background_image, anchor="nw")
            return True

        for image_path in ("image-1772109347678.png", "ui_reference.png", "mockup.png", "layout.png"):
            if not os.path.exists(image_path):
                continue
            try:
                self.background_image = tk.PhotoImage(file=image_path)
                self.canvas.create_image(0, 0, image=self.background_image, anchor="nw")
                return True
            except tk.TclError:
                continue

        return False

    def _draw_button(self, button_id: int, area: tuple[int, int, int, int]) -> None:
        x1, y1, x2, y2 = area
        pressed = self.pressed_button == button_id
        offset = 7 if pressed else 0

        cx = (x1 + x2) // 2
        cy = (y1 + y2) // 2 + offset

        if button_id == 1:
            face_color = "#3d7ce3"
            shadow_color = "#2056b7"
        else:
            face_color = "#f23d63"
            shadow_color = "#c51f45"

        self.canvas.create_oval(x1, y1 + 9, x2, y2 + 9, fill=shadow_color, outline=shadow_color)
        self.canvas.create_oval(x1, y1 + offset, x2, y2 + offset, fill=face_color, outline=face_color)

        if button_id == 1:
            self.canvas.create_text(
                cx,
                cy,
                text="SEL",
                fill="#e4efff",
                font=("Consolas", 17, "bold"),
            )
        elif button_id == 2:
            self.canvas.create_polygon(
                cx,
                cy - 14,
                cx - 10,
                cy + 8,
                cx + 10,
                cy + 8,
                fill="#fefefe",
                outline="#fefefe",
            )
        elif button_id == 3:
            self.canvas.create_polygon(
                cx,
                cy + 14,
                cx - 10,
                cy - 8,
                cx + 10,
                cy - 8,
                fill="#fefefe",
                outline="#fefefe",
            )

        label = "SELECT" if button_id == 1 else ("UP/FIM" if button_id == 2 else "PAUSAS")
        self.canvas.create_text(cx, y2 + 34, text=label, fill="#5d6f87", font=("Consolas", 12, "bold"))

    def _draw_rfid_emblem(self) -> None:
        self.canvas.create_oval(128, 84, 330, 286, fill="#a5bfeb", outline="#a5bfeb", width=3)
        self.canvas.create_oval(146, 102, 312, 268, fill="#b9c9df", outline="#b9c9df", width=2)

        self.canvas.create_text(
            230,
            234,
            text="RFID SENSOR",
            fill="#163aa8",
            font=("Consolas", 16, "bold"),
        )

        self.canvas.create_text(
            220,
            185,
            text="RFID",
            fill="#2d65d2",
            font=("Consolas", 17, "bold"),
        )

        self.canvas.create_arc(192, 127, 260, 195, start=35, extent=110, style=tk.ARC, outline="#3e7ae4", width=5)
        self.canvas.create_arc(202, 137, 274, 209, start=35, extent=110, style=tk.ARC, outline="#3e7ae4", width=5)
        self.canvas.create_arc(214, 149, 286, 221, start=35, extent=110, style=tk.ARC, outline="#3e7ae4", width=5)

    def _draw_rfid_status_dot(self) -> None:
        if self.rfid_dot_visible:
            x1, y1, x2, y2 = self.rfid_dot_area
            self.canvas.create_oval(x1, y1, x2, y2, fill=self.rfid_dot_color, outline=self.rfid_dot_color)

    def _draw_manual_input_hint(self) -> None:
        x1, y1, x2, y2 = self.manual_input_area
        self.canvas.create_rectangle(x1, y1, x2, y2, fill="#f2f4f8", outline="#3374df", width=4)
        if not self.manual_code_var.get():
            self.canvas.create_text(
                (x1 + x2) // 2,
                (y1 + y2) // 2,
                text="",
                fill="#acb7c8",
                font=("Consolas", 22, "bold"),
            )
        self.canvas.create_window((x1 + x2) // 2, (y1 + y2) // 2, window=self.manual_entry, width=(x2 - x1 - 12))

    def _draw_display_cutout(self) -> None:
        x1, y1, x2, y2 = self.display_area

        self.canvas.create_rectangle(x1 - 10, y1 - 10, x2 + 10, y2 + 10, fill="#3d4f68", outline="#3d4f68")
        self.canvas.create_rectangle(x1, y1, x2, y2, fill="#101f18", outline="#25363f", width=2)
        self._draw_oled_screen()

    def _draw_battery_icon(self, percentage: int) -> None:
        x, y = self._to_px(0, 2)
        w = 14 * self.scale_x
        h = 10 * self.scale_y
        self.canvas.create_rectangle(x, y, x + w, y + h, outline="#c4ffc7", width=1)
        self.canvas.create_rectangle(x + w, y + 2 * self.scale_y, x + w + 2 * self.scale_x, y + 8 * self.scale_y, fill="#c4ffc7", outline="#c4ffc7", width=1)
        if percentage > 0:
            fill = (w - 2 * self.scale_x) * (percentage / 100.0)
            self.canvas.create_rectangle(x + self.scale_x, y + self.scale_y, x + self.scale_x + fill, y + h - self.scale_y, fill="#c4ffc7", outline="")

    def _draw_wifi_icon(self, rssi: int) -> None:
        base_x, base_y = self._to_px(110, 10)
        if not self.wifi_connected:
            self.canvas.create_text(base_x, base_y, text="X", anchor="nw", fill="#c4ffc7", font=("Consolas", 9, "bold"))
            return
        if rssi > -90:
            self.canvas.create_rectangle(base_x, base_y - 2 * self.scale_y, base_x + 2 * self.scale_x, base_y, fill="#c4ffc7", outline="")
        if rssi > -80:
            self.canvas.create_rectangle(base_x + 3 * self.scale_x, base_y - 4 * self.scale_y, base_x + 5 * self.scale_x, base_y, fill="#c4ffc7", outline="")
        if rssi > -70:
            self.canvas.create_rectangle(base_x + 6 * self.scale_x, base_y - 6 * self.scale_y, base_x + 8 * self.scale_x, base_y, fill="#c4ffc7", outline="")
        if rssi > -60:
            self.canvas.create_rectangle(base_x + 9 * self.scale_x, base_y - 8 * self.scale_y, base_x + 11 * self.scale_x, base_y, fill="#c4ffc7", outline="")

    def _draw_oled_text(self, ox: int, oy: int, text: str, bold: bool = False) -> None:
        px, py = self._to_px(ox, oy)
        self.canvas.create_text(px, py, anchor="nw", fill="#47ef8d", text=text, font=("Consolas", 8, "bold" if bold else "normal"))

    def _draw_oled_screen(self) -> None:
        x1, y1 = self.oled_x, self.oled_y
        x2, y2 = self.oled_x + self.oled_w, self.oled_y + self.oled_h
        self.canvas.create_rectangle(x1, y1, x2, y2, outline="#1e2c24", width=1)

        self._draw_battery_icon(self.cached_battery)
        self._draw_wifi_icon(self.cached_rssi)

        if self.screen_mode == "temp":
            self._draw_oled_text(0, 30, self.temp_line1)
            self._draw_oled_text(0, 50, self.temp_line2)
        else:
            self._draw_oled_text(0, 30, self.device_ip)
            if self.last_pressed != -1:
                self._draw_oled_text(0, 50, f"Ultimo: {self.last_pressed}")
            else:
                self._draw_oled_text(0, 50, "Pronto...")

    def _is_inside(self, area: tuple[int, int, int, int], x: int, y: int) -> bool:
        x1, y1, x2, y2 = area
        return x1 <= x <= x2 and y1 <= y <= y2

    def _on_canvas_click(self, event: tk.Event) -> None:
        x, y = event.x, event.y

        for button_id, area in self.button_areas.items():
            if self._is_inside(area, x, y):
                self._press_button(button_id)
                return

        if self._is_inside(self.rfid_touch_area, x, y) or self._is_inside(self.manual_input_area, x, y):
            self._focus_manual_input()

    def _on_global_return(self, _event: tk.Event) -> str:
        if self.root.focus_get() == self.manual_entry:
            return "break"
        self._execute_button_action(1)
        return "break"

    def _on_manual_entry_return(self, _event: tk.Event) -> str:
        self._submit_manual_code()
        return "break"

    def _focus_manual_input(self) -> None:
        self.manual_entry.focus_set()
        self.manual_entry.selection_range(0, tk.END)

    def _submit_manual_code(self) -> None:
        code = self.manual_code_var.get().strip().upper()
        if not code or not code.isalnum():
            return

        self.last_rfid_code = code
        self.manual_code_var.set("")
        self._blink_rfid_dot(3)
        self._set_temp_screen("NFC:", code, 1500)
        self._draw_case()

    def _blink_rfid_dot(self, times: int) -> None:
        toggles_remaining = times * 2
        self.rfid_dot_visible = True

        def step() -> None:
            nonlocal toggles_remaining
            if toggles_remaining <= 0:
                self.rfid_dot_visible = True
                self.rfid_dot_color = "#1ec15c"
                self._draw_case()
                return

            self.rfid_dot_color = "#e93b4f" if (toggles_remaining % 2 == 0) else "#1ec15c"
            self._draw_case()
            toggles_remaining -= 1
            self.root.after(130, step)

        step()

    def _press_button(self, button_id: int) -> None:
        self.pressed_button = button_id
        self._draw_case()
        self.root.after(140, lambda: self._execute_button_action(button_id))

    def _execute_button_action(self, button_id: int) -> None:
        self.pressed_button = None
        self.last_pressed = button_id

        if button_id == 1:
            self._set_temp_screen("Enviando Tempo...", "", 15000)
            self._run_async(self._button1_action)
        elif button_id == 2:
            self._set_temp_screen("Buscando Barco...", "", 15000)
            self._run_async(self._button2_action)
        elif button_id == 3:
            self._set_temp_screen("Buscando Operador...", "", 15000)
            self._run_async(self._button3_action)

        self._draw_case()

    def _set_temp_screen(self, line1: str, line2: str, duration_ms: int) -> None:
        self.screen_mode = "temp"
        self.temp_line1 = line1
        self.temp_line2 = line2
        self.temp_until_ms = int(time.monotonic() * 1000) + duration_ms

    def _run_async(self, worker) -> None:
        thread = threading.Thread(target=worker, daemon=True)
        thread.start()

    def _load_dotenv_file(self) -> None:
        for env_path in ("Config.env", ".env"):
            if not os.path.exists(env_path):
                continue

            try:
                with open(env_path, "r", encoding="utf-8") as env_file:
                    for raw_line in env_file:
                        line = raw_line.strip()
                        if not line or line.startswith("#") or "=" not in line:
                            continue
                        key, value = line.split("=", 1)
                        key = key.strip()
                        value = value.strip().strip('"').strip("'")
                        if key and key not in os.environ:
                            os.environ[key] = value
            except OSError:
                continue

    def _supabase_generic_lookup(self, table: str, filter_col: str, filter_val: str, target_col: str) -> str:
        if not self.supabase_configured:
            return "Erro: Config ENV"
        if not self.wifi_connected:
            return "Erro: Offline"

        encoded_filter = parse.quote(filter_val, safe="")
        url = f"{self.supabase_url}/rest/v1/{table}?{filter_col}=eq.{encoded_filter}"

        req = request.Request(url, method="GET")
        req.add_header("apikey", self.supabase_key)
        req.add_header("Authorization", f"Bearer {self.supabase_key}")

        try:
            with request.urlopen(req, timeout=8) as response:
                if response.status != 200:
                    return "Nao encontrado"
                payload = response.read().decode("utf-8")
                doc = json.loads(payload)
                if isinstance(doc, list) and len(doc) > 0:
                    value = doc[0].get(target_col)
                    return str(value) if value is not None else "Nao encontrado"
                return "Nao encontrado"
        except (error.URLError, error.HTTPError, TimeoutError, json.JSONDecodeError):
            return "Nao encontrado"

    def _supabase_generic_insert(self, table: str, data: dict) -> bool:
        if not self.supabase_configured:
            return False
        if not self.wifi_connected:
            return False

        url = f"{self.supabase_url}/rest/v1/{table}"
        payload = json.dumps(data).encode("utf-8")

        req = request.Request(url, data=payload, method="POST")
        req.add_header("apikey", self.supabase_key)
        req.add_header("Authorization", f"Bearer {self.supabase_key}")
        req.add_header("Content-Type", "application/json")

        try:
            with request.urlopen(req, timeout=8) as response:
                return response.status == 201
        except (error.URLError, error.HTTPError, TimeoutError):
            return False

    def _button1_action(self) -> None:
        data = {
            "operador": "000747",
            "tempo": datetime.now().strftime("%H:%M %d/%m"),
        }
        ok = self._supabase_generic_insert("tempos", data)
        line2 = "Sucesso!" if ok else "Erro!"
        self.root.after(0, lambda: self._set_temp_screen("Enviando Tempo...", line2, 2000))

    def _button2_action(self) -> None:
        ordem = self._supabase_generic_lookup("barcos", "barco", "01010", "ordem_fabrico")
        self.root.after(0, lambda: self._set_temp_screen("Ordem:", ordem, 3000))

    def _button3_action(self) -> None:
        nome = self._supabase_generic_lookup("operadores", "numero", "000747", "nome")
        self.root.after(0, lambda: self._set_temp_screen("Nome:", nome, 3000))

def main() -> None:
    root = tk.Tk()
    RFIDSimulatorApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()