"""Bridge ESP32-P4 sensor JSON from serial into the Plant Pet API."""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

import serial


JSON_PREFIX = "PLANT_SENSOR_JSON:"


def post_analyze(api_url: str, payload: dict) -> dict:
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        api_url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=5) as response:
        return json.loads(response.read().decode("utf-8"))


def summarize(api_response: dict) -> str:
    state = api_response.get("state", api_response)
    advice = state.get("advice") or []
    issues = state.get("issues") or []
    first_advice = advice[0] if advice else "no advice"
    first_issue = issues[0] if issues else "no issue"
    return (
        f"{state.get('face', '?')} "
        f"mood={state.get('mood', '?')} "
        f"comfort={state.get('comfort_score', '?')} "
        f"health={state.get('health_score', '?')} "
        f"issue={first_issue} "
        f"advice={first_advice}"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Read ESP32-P4 sensor JSON from serial and POST it to /api/analyze."
    )
    parser.add_argument("--port", required=True, help="Serial port, for example COM8.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument(
        "--api-url",
        default="http://127.0.0.1:8765/api/analyze",
        help="Plant Pet analyze endpoint.",
    )
    parser.add_argument(
        "--once",
        action="store_true",
        help="Exit after the first valid sensor payload is posted.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    print(f"Opening {args.port} at {args.baud} baud...")

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as exc:
        print(f"Could not open serial port: {exc}", file=sys.stderr)
        return 2

    with ser:
        print(f"Waiting for lines prefixed with {JSON_PREFIX}")
        while True:
            raw_line = ser.readline()
            if not raw_line:
                continue

            line = raw_line.decode("utf-8", errors="replace").strip()
            if JSON_PREFIX not in line:
                continue

            json_text = line.split(JSON_PREFIX, 1)[1].strip()
            try:
                sensor_payload = json.loads(json_text)
            except json.JSONDecodeError as exc:
                print(f"Skipping invalid JSON: {json_text} ({exc})")
                continue

            try:
                response = post_analyze(args.api_url, sensor_payload)
            except (urllib.error.URLError, TimeoutError) as exc:
                print(f"API request failed: {exc}", file=sys.stderr)
                time.sleep(1)
                continue

            print(f"sensor={sensor_payload} -> {summarize(response)}")
            if args.once:
                return 0


if __name__ == "__main__":
    raise SystemExit(main())
