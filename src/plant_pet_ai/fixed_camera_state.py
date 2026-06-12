"""Fixed-camera visual state stabilization for slow plant growth changes."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from typing import Any, Iterable

from .plant_status_yolo import plant_status_to_leaf_inputs


STATUS_SEVERITY = {
    "healthy_no_disease": 0,
    "overall_healthy_abnormal_leaf": 1,
    "widespread_severe_abnormal": 2,
}
DEFAULT_STABLE_STATUS = "healthy_no_disease"


@dataclass(frozen=True)
class FixedCameraConfig:
    regular_interval_minutes: int = 60
    observing_interval_minutes: int = 30
    min_confidence: float = 0.65
    high_confidence: float = 0.90
    recent_window: int = 5
    local_confirmations: int = 2
    severe_confirmations: int = 2
    recovery_confirmations: int = 3
    low_light_threshold: float = 80


@dataclass(frozen=True)
class VisionObservation:
    plant_status: str
    confidence: float
    timestamp: str | None = None
    image_path: str | None = None


@dataclass(frozen=True)
class StableVisionState:
    stable_plant_status: str
    leaf_status: str
    leaf_scope: str
    mode: str
    confidence: float
    evidence_count: int
    next_check_minutes: int
    should_alert: bool
    explanation: str
    observations_used: int

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


DEFAULT_CAMERA_CONFIG = FixedCameraConfig()


def normalize_plant_status(value: str | None) -> str:
    if value in STATUS_SEVERITY:
        return value
    return DEFAULT_STABLE_STATUS


def parse_observation(raw: MappingLike) -> VisionObservation:
    return VisionObservation(
        plant_status=normalize_plant_status(str(raw.get("plant_status", ""))),
        confidence=float(raw.get("confidence", 0.0)),
        timestamp=raw.get("timestamp"),
        image_path=raw.get("image_path"),
    )


def should_run_visual_check(
    *,
    last_checked_at: str | None = None,
    now: datetime | None = None,
    stable_plant_status: str = DEFAULT_STABLE_STATUS,
    mode: str = "stable",
    light: float | None = None,
    config: FixedCameraConfig = DEFAULT_CAMERA_CONFIG,
) -> tuple[bool, str, int]:
    """Decide whether the fixed camera should run YOLO now."""

    if light is not None and light < config.low_light_threshold:
        return False, "skip_low_light", config.regular_interval_minutes

    interval = (
        config.observing_interval_minutes
        if mode.startswith("watching") or stable_plant_status != DEFAULT_STABLE_STATUS
        else config.regular_interval_minutes
    )
    if not last_checked_at:
        return True, "no_previous_check", interval

    checked_at = _parse_timestamp(last_checked_at)
    if checked_at is None:
        return True, "invalid_previous_check", interval

    now = now or datetime.now(timezone.utc)
    elapsed_minutes = (now - checked_at).total_seconds() / 60
    if elapsed_minutes >= interval:
        return True, "interval_elapsed", interval
    return False, "interval_not_elapsed", interval


def stabilize_vision_status(
    observations: Iterable[VisionObservation | MappingLike],
    *,
    previous_stable_status: str = DEFAULT_STABLE_STATUS,
    config: FixedCameraConfig = DEFAULT_CAMERA_CONFIG,
) -> StableVisionState:
    """Turn low-frequency YOLO observations into a stable plant status.

    The rules intentionally follow plant biology: visual deterioration should be
    confirmed across more than one check, and recovery is slower than worsening.
    """

    parsed = [
        item if isinstance(item, VisionObservation) else parse_observation(item)
        for item in observations
    ]
    recent = parsed[-config.recent_window :]
    confident = [obs for obs in recent if obs.confidence >= config.min_confidence]
    previous = normalize_plant_status(previous_stable_status)

    if not recent:
        leaf_status, leaf_scope = plant_status_to_leaf_inputs(previous)
        return StableVisionState(
            stable_plant_status=previous,
            leaf_status=leaf_status,
            leaf_scope=leaf_scope,
            mode="stable",
            confidence=0.0,
            evidence_count=0,
            next_check_minutes=config.regular_interval_minutes,
            should_alert=False,
            explanation="No visual observations yet; keeping the previous stable state.",
            observations_used=0,
        )

    if not confident:
        leaf_status, leaf_scope = plant_status_to_leaf_inputs(previous)
        return StableVisionState(
            stable_plant_status=previous,
            leaf_status=leaf_status,
            leaf_scope=leaf_scope,
            mode="watching_low_confidence",
            confidence=max((obs.confidence for obs in recent), default=0.0),
            evidence_count=0,
            next_check_minutes=config.observing_interval_minutes,
            should_alert=False,
            explanation="Recent YOLO results are below confidence threshold; keeping the previous stable state.",
            observations_used=len(recent),
        )

    counts = {status: 0 for status in STATUS_SEVERITY}
    confidences = {status: [] for status in STATUS_SEVERITY}
    for obs in confident:
        counts[obs.plant_status] += 1
        confidences[obs.plant_status].append(obs.confidence)

    latest = confident[-1]
    severe_count = counts["widespread_severe_abnormal"]
    local_count = counts["overall_healthy_abnormal_leaf"]
    healthy_count = counts["healthy_no_disease"]

    next_status = previous
    mode = "stable"
    explanation = "Visual state is stable."

    if severe_count >= config.severe_confirmations:
        next_status = "widespread_severe_abnormal"
        mode = "stable_severe"
        explanation = "Severe abnormality has been confirmed across repeated fixed-camera checks."
    elif latest.plant_status == "widespread_severe_abnormal" and latest.confidence >= config.high_confidence:
        next_status = previous
        mode = "watching_severe"
        explanation = "A high-confidence severe result appeared; waiting for the next scheduled check before locking it in."
    elif local_count >= config.local_confirmations:
        if previous == "widespread_severe_abnormal":
            next_status = previous
            mode = "recovering"
            explanation = "The image looks less severe, but recovery needs more repeated evidence than deterioration."
        else:
            next_status = "overall_healthy_abnormal_leaf"
            mode = "stable_local_issue"
            explanation = "Localized abnormality has repeated, so it is accepted as the stable visual state."
    elif healthy_count >= config.recovery_confirmations:
        next_status = "healthy_no_disease"
        mode = "stable_recovered" if previous != "healthy_no_disease" else "stable"
        explanation = "Repeated healthy checks support recovery."
    elif healthy_count > 0 and previous != "healthy_no_disease":
        next_status = previous
        mode = "recovering"
        explanation = "Healthy evidence appeared, but plant recovery should be confirmed over several checks."
    elif latest.plant_status != previous:
        next_status = previous
        mode = "watching_change"
        explanation = "A new visual class appeared once; keeping the previous stable state until it repeats."

    leaf_status, leaf_scope = plant_status_to_leaf_inputs(next_status)
    status_confidences = confidences.get(next_status) or [latest.confidence]
    confidence = sum(status_confidences) / len(status_confidences)
    evidence_count = counts.get(next_status, 0)
    if mode.startswith("watching"):
        evidence_count = counts.get(latest.plant_status, 0)
        confidence = latest.confidence

    return StableVisionState(
        stable_plant_status=next_status,
        leaf_status=leaf_status,
        leaf_scope=leaf_scope,
        mode=mode,
        confidence=confidence,
        evidence_count=evidence_count,
        next_check_minutes=(
            config.observing_interval_minutes
            if mode.startswith("watching") or next_status != DEFAULT_STABLE_STATUS
            else config.regular_interval_minutes
        ),
        should_alert=next_status == "widespread_severe_abnormal" and mode == "stable_severe",
        explanation=explanation,
        observations_used=len(recent),
    )


def _parse_timestamp(value: str) -> datetime | None:
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=timezone.utc)
    return parsed


MappingLike = dict[str, Any]
