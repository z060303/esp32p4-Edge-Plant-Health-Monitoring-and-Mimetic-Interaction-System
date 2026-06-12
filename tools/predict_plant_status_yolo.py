"""Run the latest YOLO whole-plant status model and fixed-camera logic."""

from __future__ import annotations

import json
import sys
from argparse import ArgumentParser
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from src.plant_pet_ai.fixed_camera_state import stabilize_vision_status  # noqa: E402
from src.plant_pet_ai.plant_status_yolo import predict_plant_status_yolo  # noqa: E402
from src.plant_pet_ai.state_engine import analyze_plant  # noqa: E402


STATUS_ZH = {
    "healthy_no_disease": "健康",
    "overall_healthy_abnormal_leaf": "局部异常",
    "widespread_severe_abnormal": "大规模异常",
}


def parse_args():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="Plant image to analyze.")
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--previous-status", default="healthy_no_disease")
    parser.add_argument(
        "--history",
        type=Path,
        help="Optional JSON file containing previous vision observations.",
    )
    parser.add_argument("--soil-moisture", default=45, type=float)
    parser.add_argument("--temperature", default=26, type=float)
    parser.add_argument("--air-humidity", default=55, type=float)
    parser.add_argument("--light", default=500, type=float)
    parser.add_argument("--json", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    history = []
    if args.history and args.history.exists():
        history = json.loads(args.history.read_text(encoding="utf-8"))

    prediction = predict_plant_status_yolo(args.image, device=args.device)
    history.append(
        {
            "plant_status": prediction["plant_status"],
            "confidence": prediction["confidence"],
            "image_path": prediction["image_path"],
        }
    )
    fixed_camera_state = stabilize_vision_status(
        history,
        previous_stable_status=args.previous_status,
    )
    state = analyze_plant(
        soil_moisture=args.soil_moisture,
        temperature=args.temperature,
        air_humidity=args.air_humidity,
        light=args.light,
        leaf_status=fixed_camera_state.leaf_status,
        leaf_scope=fixed_camera_state.leaf_scope,
    )
    result = {
        "prediction": prediction,
        "fixed_camera_state": fixed_camera_state.to_dict(),
        "state": {
            "env_status": state.env_status,
            "leaf_status": state.leaf_status,
            "leaf_scope": state.leaf_scope,
            "comfort_score": state.comfort_score,
            "health_score": state.health_score,
            "mood": state.mood,
            "face": state.face,
            "summary": state.summary,
            "issues": list(state.issues),
            "advice": list(state.advice),
            "care_notes": list(state.care_notes),
        },
    }

    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return

    plant_status = prediction["plant_status"]
    stable_status = fixed_camera_state.stable_plant_status
    print(f"YOLO: {plant_status} ({STATUS_ZH.get(plant_status, plant_status)})")
    print(f"confidence: {prediction['confidence']:.4f}")
    print(
        "stable_status: "
        f"{stable_status} ({STATUS_ZH.get(stable_status, stable_status)})"
    )
    print(f"mode: {fixed_camera_state.mode}")
    print(f"next_check_minutes: {fixed_camera_state.next_check_minutes}")
    print(f"engine_leaf: {state.leaf_status} / {state.leaf_scope}")
    print(f"health_score: {state.health_score}")
    print(f"mood: {state.mood} {state.face}")
    print(f"summary: {state.summary}")
    for item in state.issues:
        print(f"issue: {item}")
    for item in state.advice:
        print(f"advice: {item}")


if __name__ == "__main__":
    main()
