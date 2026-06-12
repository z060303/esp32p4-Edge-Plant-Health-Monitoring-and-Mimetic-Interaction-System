"""YOLO whole-plant status inference for the fixed-camera workflow."""

from __future__ import annotations

from functools import lru_cache
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MODEL_PATH = (
    ROOT
    / "models"
    / "yolo_plant_status"
    / "yolo11n_cls_seed42_e30"
    / "weights"
    / "best.pt"
)
IMAGE_SIZE = 224
PLANT_STATUS_LABELS = (
    "healthy_no_disease",
    "overall_healthy_abnormal_leaf",
    "widespread_severe_abnormal",
)


@lru_cache(maxsize=2)
def _load_model(model_path: str):
    try:
        from ultralytics import YOLO
    except ImportError as exc:
        raise RuntimeError(
            "Ultralytics is not installed. Run: pip install -r requirements.txt"
        ) from exc
    return YOLO(model_path)


def predict_plant_status_yolo(
    image_path: str | Path,
    *,
    model_path: str | Path = DEFAULT_MODEL_PATH,
    image_size: int = IMAGE_SIZE,
    device: str = "cpu",
) -> dict[str, Any]:
    """Predict one of the three whole-plant status labels with YOLO."""

    image_path = Path(image_path)
    model_path = Path(model_path)

    if not image_path.exists():
        raise FileNotFoundError(f"Image does not exist: {image_path}")
    if not model_path.exists():
        raise FileNotFoundError(f"YOLO model does not exist: {model_path}")

    model = _load_model(str(model_path))
    result = model.predict(
        source=str(image_path),
        imgsz=image_size,
        device=device,
        verbose=False,
    )[0]
    probabilities = result.probs.data.cpu().numpy()
    names = result.names
    top1_index = int(result.probs.top1)
    raw_label = names[top1_index]

    return {
        "plant_status": raw_label,
        "confidence": float(result.probs.top1conf),
        "model_path": str(model_path),
        "image_path": str(image_path),
        "probabilities": {
            names[index]: float(probabilities[index])
            for index in range(len(probabilities))
        },
    }


def plant_status_to_leaf_inputs(plant_status: str) -> tuple[str, str]:
    """Map the whole-plant YOLO label onto the existing rule-engine inputs."""

    if plant_status == "healthy_no_disease":
        return "healthy", "local"
    if plant_status == "overall_healthy_abnormal_leaf":
        return "leaf_spot", "local"
    if plant_status == "widespread_severe_abnormal":
        return "wilted", "widespread"
    return "unknown", "unknown"
