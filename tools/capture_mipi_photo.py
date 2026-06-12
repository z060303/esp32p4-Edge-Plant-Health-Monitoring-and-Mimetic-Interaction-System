"""Capture one ESP32-P4 OV5645 frame over serial and save it as PNG.

The matching camera firmware prints a base64 wrapped RGB565 payload between
PHOTO_BEGIN and PHOTO_END. This script sends SNAP repeatedly until that payload
arrives, then converts the RGB565 frame to an ordinary PNG on the computer.
"""

from __future__ import annotations

import argparse
import base64
import binascii
import re
import struct
import sys
import time
import zlib
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT_DIR = ROOT / "data" / "captures"
PHOTO_BEGIN_RE = re.compile(r"PHOTO_BEGIN\s+(?P<meta>.+)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port, for example COM8.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument(
        "--output",
        type=Path,
        help="Output PNG path. Defaults to data/captures/mipi_photo_<timestamp>.png.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=120.0,
        help="Seconds to wait for one complete photo.",
    )
    parser.add_argument(
        "--snap-interval",
        type=float,
        default=1.5,
        help="Seconds between SNAP commands while waiting.",
    )
    parser.add_argument(
        "--command",
        choices=("SNAP", "PREVIEW"),
        default="SNAP",
        help="Camera command to send. SNAP returns 320x240, PREVIEW returns 160x120.",
    )
    parser.add_argument(
        "--raw-output",
        type=Path,
        help="Optional path to also save the received RGB565 payload.",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=3,
        help="Number of photo transfer attempts when the serial payload is incomplete.",
    )
    return parser.parse_args()


def require_serial():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError as exc:
        raise SystemExit(
            "Missing dependency: pyserial. Install it with: pip install pyserial"
        ) from exc
    return serial, list_ports


def choose_port(list_ports, requested: str | None) -> str:
    if requested:
        return requested

    ports = list(list_ports.comports())
    if not ports:
        raise SystemExit("No serial ports found. Pass --port COMx after plugging in the board.")
    if len(ports) == 1:
        return ports[0].device

    preferred_tokens = ("usb serial", "usb jtag", "jtag", "cp210", "ch340", "esp")
    for port in ports:
        text = f"{port.description} {port.manufacturer or ''}".lower()
        if any(token in text for token in preferred_tokens):
            return port.device

    available = ", ".join(port.device for port in ports)
    raise SystemExit(f"Multiple serial ports found ({available}). Pass --port explicitly.")


def parse_photo_meta(line: str) -> dict[str, str]:
    match = PHOTO_BEGIN_RE.search(line)
    if not match:
        return {}

    meta: dict[str, str] = {}
    for item in match.group("meta").split():
        if "=" in item:
            key, value = item.split("=", 1)
            meta[key] = value
    return meta


def read_photo(
    ser,
    timeout: float,
    snap_interval: float,
    command: str | None = "SNAP",
) -> tuple[dict[str, str], bytes]:
    deadline = time.monotonic() + timeout
    last_snap = 0.0
    meta: dict[str, str] | None = None
    chunks: list[str] = []
    command_bytes = f"{command}\n".encode("ascii") if command else None

    while time.monotonic() < deadline:
        now = time.monotonic()
        if command_bytes is not None and now - last_snap >= snap_interval:
            ser.write(command_bytes)
            ser.flush()
            last_snap = now

        raw_line = ser.readline()
        if not raw_line:
            continue

        line = raw_line.decode("ascii", errors="ignore").strip()
        if not line:
            continue

        if meta is None:
            candidate = parse_photo_meta(line)
            if candidate:
                meta = candidate
                chunks.clear()
                print(f"Receiving photo: {candidate}", flush=True)
            else:
                print(line, flush=True)
            continue

        if "PHOTO_END" in line:
            payload = "".join(chunks)
            try:
                return meta, base64.b64decode(payload, validate=True)
            except binascii.Error as exc:
                raise RuntimeError("Received photo payload is not valid base64.") from exc

        if set(line) <= set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="):
            chunks.append(line)

    raise TimeoutError(f"Timed out after {timeout:.1f}s waiting for PHOTO_BEGIN/PHOTO_END.")


def rgb565_to_rgb888(raw: bytes, width: int, height: int) -> bytes:
    expected = width * height * 2
    if len(raw) != expected:
        raise ValueError(f"Expected {expected} RGB565 bytes, got {len(raw)}.")

    rgb = bytearray(width * height * 3)
    out = 0
    for index in range(0, len(raw), 2):
        value = raw[index] | (raw[index + 1] << 8)
        r5 = (value >> 11) & 0x1F
        g6 = (value >> 5) & 0x3F
        b5 = value & 0x1F
        rgb[out] = (r5 * 255) // 31
        rgb[out + 1] = (g6 * 255) // 63
        rgb[out + 2] = (b5 * 255) // 31
        out += 3
    return bytes(rgb)


def png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + chunk_type
        + data
        + struct.pack(">I", binascii.crc32(chunk_type + data) & 0xFFFFFFFF)
    )


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    row_len = width * 3
    scanlines = bytearray()
    for row in range(height):
        scanlines.append(0)
        start = row * row_len
        scanlines.extend(rgb[start : start + row_len])

    data = b"".join(
        [
            b"\x89PNG\r\n\x1a\n",
            png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)),
            png_chunk(b"IDAT", zlib.compress(bytes(scanlines), level=6)),
            png_chunk(b"IEND", b""),
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def default_output_path() -> Path:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return DEFAULT_OUTPUT_DIR / f"mipi_photo_{timestamp}.png"


def main() -> int:
    args = parse_args()
    serial, list_ports = require_serial()
    port = choose_port(list_ports, args.port)
    output = args.output or default_output_path()

    print(f"Opening {port} at {args.baud} baud. Waiting for photo...")
    with serial.Serial(port, args.baud, timeout=0.2, write_timeout=2) as ser:
        ser.dtr = False
        ser.rts = False
        time.sleep(1.0)
        ser.reset_input_buffer()
        last_error: Exception | None = None
        for attempt in range(1, args.retries + 1):
            try:
                meta, raw = read_photo(ser, args.timeout, args.snap_interval, args.command)
            except (RuntimeError, TimeoutError) as exc:
                last_error = exc
                print(
                    f"Photo transfer failed on attempt {attempt}/{args.retries}: {exc}",
                    flush=True,
                )
                continue

            expected_bytes = int(meta["bytes"])
            if len(raw) == expected_bytes:
                break
            print(
                f"Incomplete payload on attempt {attempt}/{args.retries}: "
                f"expected {expected_bytes}, received {len(raw)}. Retrying...",
                flush=True,
            )
        else:
            if last_error is not None:
                raise RuntimeError(
                    f"Photo transfer failed after {args.retries} attempts."
                ) from last_error
            raise RuntimeError(
                f"Expected {expected_bytes} bytes, received {len(raw)} bytes "
                f"after {args.retries} attempts."
            )

    width = int(meta["width"])
    height = int(meta["height"])

    if args.raw_output:
        args.raw_output.parent.mkdir(parents=True, exist_ok=True)
        args.raw_output.write_bytes(raw)

    rgb = rgb565_to_rgb888(raw, width, height)
    write_png(output, width, height, rgb)
    print(f"Saved {width}x{height} PNG: {output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
