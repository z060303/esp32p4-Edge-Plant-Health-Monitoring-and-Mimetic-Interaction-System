"""Show a low-resolution live preview from the ESP32-P4 MIPI camera."""

from __future__ import annotations

import argparse
import tempfile
import tkinter as tk
from pathlib import Path

from capture_mipi_photo import (
    choose_port,
    read_photo,
    require_serial,
    rgb565_to_rgb888,
    write_png,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port, for example COM8.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument("--timeout", type=float, default=60.0, help="Seconds per preview read.")
    parser.add_argument(
        "--delay",
        type=int,
        default=100,
        help="Delay in milliseconds before requesting the next preview frame.",
    )
    return parser.parse_args()


class PreviewApp:
    def __init__(self, root: tk.Tk, ser, timeout: float, delay_ms: int) -> None:
        self.root = root
        self.ser = ser
        self.timeout = timeout
        self.delay_ms = delay_ms
        self.frame_count = 0
        self.tmp_path = Path(tempfile.gettempdir()) / "plant_ai_mipi_preview.png"

        self.root.title("ESP32-P4 MIPI Camera Preview")
        self.status = tk.StringVar(value="Waiting for preview frame...")
        self.image_label = tk.Label(root, bg="black")
        self.image_label.pack(padx=8, pady=8)
        tk.Label(root, textvariable=self.status, anchor="w").pack(fill="x", padx=8, pady=(0, 8))
        self.photo = None

        self.root.after(50, self.capture_once)

    def capture_once(self) -> None:
        try:
            meta, raw = read_photo(self.ser, self.timeout, 0.4, None)
            expected = int(meta["bytes"])
            if len(raw) != expected:
                self.status.set(f"Incomplete frame: expected {expected}, got {len(raw)}")
            else:
                width = int(meta["width"])
                height = int(meta["height"])
                rgb = rgb565_to_rgb888(raw, width, height)
                write_png(self.tmp_path, width, height, rgb)
                self.photo = tk.PhotoImage(file=self.tmp_path)
                self.image_label.configure(image=self.photo)
                self.frame_count += 1
                self.status.set(f"{width}x{height} preview frame {self.frame_count}")
        except Exception as exc:
            self.status.set(f"Preview error: {exc}")

        self.root.after(self.delay_ms, self.capture_once)


def main() -> int:
    serial, list_ports = require_serial()
    args = parse_args()
    port = choose_port(list_ports, args.port)

    ser = serial.Serial(port, args.baud, timeout=0.2, write_timeout=2)
    ser.dtr = False
    ser.rts = False

    root = tk.Tk()
    PreviewApp(root, ser, args.timeout, args.delay)

    try:
        root.mainloop()
    finally:
        ser.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
