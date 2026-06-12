# Fixed-Camera YOLO State Engine

This project now treats the YOLO whole-plant classifier as a low-frequency
visual observation source, not as an instant state switch.

## Model

Current model:

```text
models/yolo_plant_status/yolo11n_cls_seed42_e30/weights/best.pt
```

YOLO labels:

| YOLO label | Meaning | Rule-engine mapping |
| --- | --- | --- |
| `healthy_no_disease` | Healthy plant | `leaf_status=healthy`, `leaf_scope=local` |
| `overall_healthy_abnormal_leaf` | Mostly healthy, localized abnormal leaves | `leaf_status=leaf_spot`, `leaf_scope=local` |
| `widespread_severe_abnormal` | Widespread/severe abnormality | `leaf_status=wilted`, `leaf_scope=widespread` |

## Fixed-Camera Rules

The camera is fixed, so re-taking a photo is usually not useful. The engine uses
history and plant-growth timing instead:

- Regular YOLO interval: `60` minutes.
- Watching/abnormal interval: `30` minutes.
- Ignore or skip low-light checks when `light < 80`.
- A single new class does not immediately change the stable state.
- Localized abnormality needs `2` confident observations.
- Widespread/severe abnormality needs `2` confident observations.
- Recovery back to healthy needs `3` confident healthy observations.
- Confidence below `0.65` keeps the previous stable state and enters watching.

This creates hysteresis: deterioration can be confirmed faster than recovery,
because real plants often recover more slowly than they decline.

## API Usage

`/api/analyze` accepts the usual sensor payload plus optional visual inputs:

```json
{
  "soil_moisture": 45,
  "temperature": 26,
  "air_humidity": 55,
  "light": 500,
  "plant_image_path": "data/inference/sample.jpg",
  "stable_plant_status": "healthy_no_disease",
  "vision_history": [
    {
      "plant_status": "overall_healthy_abnormal_leaf",
      "confidence": 0.98,
      "timestamp": "2026-06-05T08:00:00+00:00"
    }
  ]
}
```

The response includes:

- `plant_status_prediction`: the raw YOLO result for the current image.
- `fixed_camera_state`: the stabilized visual state.
- `visual_check`: whether the next scheduled YOLO run should happen.
- `state`: the original rule-engine output after stabilized visual mapping.
- `vision_history`: recent observations to persist and send back next time.

## Command-Line Test

```powershell
python tools/predict_plant_status_yolo.py path\to\plant.jpg
```

Use `--json` for the full structured output.
