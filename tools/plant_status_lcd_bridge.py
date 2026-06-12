"""Capture a plant frame, run PC-side AI, and send the result to the board LCD.

The ESP32-P4 firmware keeps camera capture and LCD display on the board. This
script performs the first-version AI loop on the PC:

    serial camera frame -> YOLO plant status -> rule engine -> RESULT command
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from src.plant_pet_ai.fixed_camera_state import stabilize_vision_status
from src.plant_pet_ai.plant_status_yolo import (
    plant_status_to_leaf_inputs,
    predict_plant_status_yolo,
)
from src.plant_pet_ai.state_engine import analyze_plant
from tools.capture_mipi_photo import (
    default_output_path,
    read_photo,
    require_serial,
    rgb565_to_rgb888,
    write_png,
)

DEFAULT_HISTORY_PATH = ROOT / "data" / "captures" / "plant_status_lcd_history.json"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", type=Path, help="PNG output path.")
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument("--retries", type=int, default=3)
    parser.add_argument("--command", choices=("SNAP", "PREVIEW"), default="PREVIEW")
    parser.add_argument("--soil-moisture", type=float, default=45.0)
    parser.add_argument("--temperature", type=float, default=26.0)
    parser.add_argument("--air-humidity", type=float, default=55.0)
    parser.add_argument("--light", type=float, default=500.0)
    parser.add_argument("--device", default="cpu")
    parser.add_argument(
        "--stabilization",
        choices=("instant", "stable"),
        default="stable",
        help="instant displays the current AI result; stable requires repeated evidence.",
    )
    parser.add_argument(
        "--engine-location",
        choices=("board", "pc"),
        default="board",
        help="board sends VISION for ESP32-P4 C engine; pc sends RESULT from Python engine.",
    )
    parser.add_argument(
        "--board-response-timeout",
        type=float,
        default=2.0,
        help="Seconds to read board ENGINE/RESULT logs after sending the command.",
    )
    parser.add_argument(
        "--send-env",
        action="store_true",
        help="Include PC-provided soil/temp/hum/light in board VISION commands.",
    )
    parser.add_argument("--loop", action="store_true", help="Run continuously.")
    parser.add_argument("--interval", type=float, default=300.0, help="Seconds between loop checks.")
    parser.add_argument(
        "--iterations",
        type=int,
        default=0,
        help="Maximum checks to run. 0 means run until Ctrl+C.",
    )
    parser.add_argument(
        "--history",
        type=Path,
        default=DEFAULT_HISTORY_PATH,
        help="JSON history used by --stabilization stable.",
    )
    parser.add_argument(
        "--max-history",
        type=int,
        default=20,
        help="Maximum observations to keep in the history JSON.",
    )
    parser.add_argument(
        "--previous-stable-status",
        default="healthy_no_disease",
        help="Fallback stable visual status when the history file is empty.",
    )
    return parser.parse_args()


def capture_png_from_serial(ser, args: argparse.Namespace) -> Path:
    output = args.output or default_output_path()

    for attempt in range(1, args.retries + 1):
        meta, raw = read_photo(ser, args.timeout, 1.0, args.command)
        expected_bytes = int(meta["bytes"])
        if len(raw) == expected_bytes:
            break
        print(
            f"Incomplete payload on attempt {attempt}/{args.retries}: "
            f"expected {expected_bytes}, received {len(raw)}. Retrying...",
            flush=True,
        )
    else:
        raise RuntimeError(
            f"Expected {expected_bytes} bytes, received {len(raw)} bytes "
            f"after {args.retries} attempts."
        )

    width = int(meta["width"])
    height = int(meta["height"])
    rgb = rgb565_to_rgb888(raw, width, height)
    write_png(output, width, height, rgb)
    return output


def load_history(path: Path) -> dict:
    if not path.exists():
        return {"stable_plant_status": None, "observations": []}

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"stable_plant_status": None, "observations": []}

    if not isinstance(data, dict):
        return {"stable_plant_status": None, "observations": []}
    if not isinstance(data.get("observations"), list):
        data["observations"] = []
    return data


def save_history(path: Path, history: dict, max_history: int) -> None:
    observations = history.get("observations", [])
    if len(observations) > max_history:
        history["observations"] = observations[-max_history:]

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(history, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def portable_image_path(image_path: str) -> str:
    path = Path(image_path)
    try:
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    except (OSError, ValueError):
        return str(path)


def append_prediction_to_history(history: dict, prediction: dict) -> None:
    history.setdefault("observations", []).append(
        {
            "plant_status": prediction["plant_status"],
            "confidence": prediction["confidence"],
            "image_path": portable_image_path(prediction["image_path"]),
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }
    )


def build_result_command(
    prediction: dict,
    args: argparse.Namespace,
    history: dict,
) -> tuple[str, dict | None]:
    plant_status = prediction["plant_status"]
    confidence = round(prediction["confidence"] * 100)
    stable_info = None

    if args.stabilization == "stable":
        stable = stabilize_vision_status(
            history.get("observations", []),
            previous_stable_status=(
                history.get("stable_plant_status")
                or args.previous_stable_status
            ),
        )
        plant_status = stable.stable_plant_status
        leaf_status = stable.leaf_status
        leaf_scope = stable.leaf_scope
        confidence = round(stable.confidence * 100)
        stable_info = stable.to_dict()
        history["stable_plant_status"] = stable.stable_plant_status
    else:
        leaf_status, leaf_scope = plant_status_to_leaf_inputs(plant_status)

    state = analyze_plant(
        soil_moisture=args.soil_moisture,
        temperature=args.temperature,
        air_humidity=args.air_humidity,
        light=args.light,
        leaf_status=leaf_status,
        leaf_scope=leaf_scope,
    )

    if plant_status == "healthy_no_disease":
        status = "HEALTHY"
        advice = "KEEP"
    elif plant_status == "overall_healthy_abnormal_leaf":
        status = "LOCAL_ISSUE"
        advice = "WATCH"
    else:
        status = "SEVERE"
        advice = "ALERT"

    if args.engine_location == "board":
        if not args.send_env:
            return (
                f"VISION status={status} conf={confidence}\n"
            ), stable_info
        return (
            f"VISION status={status} conf={confidence} "
            f"soil={args.soil_moisture} temp={args.temperature} "
            f"hum={args.air_humidity} light={args.light}\n"
        ), stable_info

    mood = state.mood.upper()
    return (
        f"RESULT status={status} conf={confidence} mood={mood} "
        f"health={state.health_score} advice={advice}\n"
    ), stable_info


def read_board_response(ser, timeout: float) -> list[str]:
    deadline = time.monotonic() + timeout
    text = ""
    while time.monotonic() < deadline:
        raw = ser.read(1024)
        if raw:
            text += raw.decode("utf-8", errors="replace")

    interesting = []
    for line in text.replace("\r", "\n").split("\n"):
        for marker in (" plant_engine:", " plant_display:"):
            marker_index = line.find(marker)
            if marker_index >= 0:
                ansi_index = line.rfind("\x1b[", 0, marker_index)
                start = ansi_index if ansi_index >= 0 else max(0, marker_index - 32)
                interesting.append(line[start:].strip())
                break
    return interesting


def main() -> int:
    args = parse_args()
    serial, _ = require_serial()
    history = load_history(args.history)
    iteration = 0

    with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=2) as ser:
        ser.dtr = False
        ser.rts = False

        while True:
            iteration += 1
            print(
                f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] "
                f"Check {iteration} starting...",
                flush=True,
            )
            ser.dtr = False
            ser.rts = False
            image_path = capture_png_from_serial(ser, args)
            prediction = predict_plant_status_yolo(image_path, device=args.device)
            append_prediction_to_history(history, prediction)
            command, stable_info = build_result_command(prediction, args, history)
            save_history(args.history, history, args.max_history)
            time.sleep(0.2)
            ser.write(command.encode("ascii"))
            ser.flush()
            board_response = read_board_response(ser, args.board_response_timeout)

            print(f"Image: {image_path}")
            print(f"Prediction: {prediction['plant_status']} ({prediction['confidence']:.3f})")
            if stable_info:
                print(
                    "Stable: "
                    f"{stable_info['stable_plant_status']} "
                    f"mode={stable_info['mode']} "
                    f"evidence={stable_info['evidence_count']} "
                    f"used={stable_info['observations_used']}"
                )
            print(f"Sent: {command.strip()}")
            for line in board_response:
                print(f"Board: {line}")

            should_stop = (
                not args.loop
                or (args.iterations > 0 and iteration >= args.iterations)
            )
            if should_stop:
                break

            print(f"Sleeping {args.interval:.1f}s before next check...", flush=True)
            time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
