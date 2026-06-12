"""Conservative leaf image classifier for the first Plant Pet AI prototype."""

from __future__ import annotations

from functools import lru_cache
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
MODEL_DIR = ROOT / "models" / "leaf_classifier"
DEFAULT_MODEL_PATH = MODEL_DIR / "best_leaf_model.keras"
DEFAULT_LABELS_PATH = MODEL_DIR / "leaf_labels.txt"
DEFAULT_HEALTHY_THRESHOLD = 0.75
IMAGE_SIZE = (224, 224)


@lru_cache(maxsize=2)
def _load_runtime(model_path: str, labels_path: str):
    try:
        import tensorflow as tf
    except ImportError as exc:
        raise RuntimeError(
            "TensorFlow is not installed. Run: pip install -r requirements-train.txt"
        ) from exc

    labels = Path(labels_path).read_text(encoding="utf-8").splitlines()
    model = tf.keras.models.load_model(model_path)
    return tf, model, labels


def predict_leaf_image(
    image_path: str | Path,
    *,
    model_path: str | Path = DEFAULT_MODEL_PATH,
    labels_path: str | Path = DEFAULT_LABELS_PATH,
    healthy_threshold: float = DEFAULT_HEALTHY_THRESHOLD,
) -> dict[str, Any]:
    """Predict leaf status, then map it to a conservative first-version status.

    The current model is a five-class prototype trained from public datasets.
    For the first product loop we only trust confident ``healthy`` predictions.
    Every other raw prediction becomes ``unknown`` so the app does not claim a
    specific disease without enough pothos-specific data.
    """

    image_path = Path(image_path)
    model_path = Path(model_path)
    labels_path = Path(labels_path)

    if not image_path.exists():
        raise FileNotFoundError(f"Image does not exist: {image_path}")
    if not model_path.exists():
        raise FileNotFoundError(f"Model does not exist: {model_path}")
    if not labels_path.exists():
        raise FileNotFoundError(f"Labels file does not exist: {labels_path}")

    tf, model, labels = _load_runtime(str(model_path), str(labels_path))
    image = tf.keras.utils.load_img(image_path, target_size=IMAGE_SIZE)
    batch = tf.keras.utils.img_to_array(image)[None, ...]
    probabilities = model.predict(batch, verbose=0)[0]

    raw_index = int(probabilities.argmax())
    raw_label = labels[raw_index]
    confidence = float(probabilities[raw_index])
    leaf_status = (
        "healthy"
        if raw_label == "healthy" and confidence >= healthy_threshold
        else "unknown"
    )

    return {
        "leaf_status": leaf_status,
        "leaf_scope": "local",
        "raw_prediction": raw_label,
        "confidence": confidence,
        "healthy_threshold": healthy_threshold,
        "probabilities": {
            labels[index]: float(probabilities[index]) for index in range(len(labels))
        },
    }
