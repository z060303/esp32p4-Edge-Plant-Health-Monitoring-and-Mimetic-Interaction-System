# First version detection runbook

## Current target

First version means:

```text
real plant image -> PC-side AI status detection -> ESP32-P4 C engine -> ESP32-P4 result display
```

The LCD is still shipping, so the board currently receives the same `RESULT`
command but logs it instead of drawing it.

## Hardware now

- ESP32-P4 board
- OV5645 MIPI camera
- USB data cable on COM8

LCD to add later:

- 7-inch RGB LCD, 480x800 / 800x480 class, ALIENTEK-compatible

## One-shot detection

```powershell
python tools\plant_status_lcd_bridge.py --port COM8 --command PREVIEW --retries 3
```

Expected output:

```text
Image: data\captures\...
Prediction: healthy_no_disease (...)
Sent: RESULT status=HEALTHY conf=... mood=... health=... advice=...
```

With the current firmware, the bridge sends a `VISION` command by default and
lets the ESP32-P4 C engine calculate mood, health, and advice:

```text
Sent: VISION status=HEALTHY conf=... soil=45.0 temp=26.0 hum=55.0 light=500.0
```

Use the old PC-side rule-engine mode only when needed:

```powershell
python tools\plant_status_lcd_bridge.py --port COM8 --command PREVIEW --engine-location pc
```

Possible status values:

- `HEALTHY`
- `LOCAL_ISSUE`
- `SEVERE`

## Continuous detection

Run every 5 minutes:

```powershell
python tools\plant_status_lcd_bridge.py --port COM8 --command PREVIEW --retries 3 --loop --interval 300
```

Use `--stabilization stable` after the demo if you want repeated evidence before
the displayed status changes.

## Board-engine smoke tests

After flashing the firmware, test the C engine without camera or PC AI by
sending one serial line:

```text
ANALYZE soil=45 temp=26 hum=55 light=500 vision=HEALTHY conf=90
```

Expected board log or LCD status:

```text
ENGINE status=HEALTHY conf=90 mood=HAPPY comfort=100 health=100 advice=KEEP
```

Try a severe case:

```text
ANALYZE soil=20 temp=34 hum=30 light=1300 vision=SEVERE conf=88
```

Expected result should trend toward low health and urgent advice, for example
`status=SEVERE`, `advice=ALERT`.

## Camera placement

For the current model, a good first-version image should look like this:

- Leaves occupy about 50-70% of the frame.
- Avoid large white table areas taking most of the image.
- Keep the camera fixed once placed.
- Avoid direct glare from the lamp.
- Use normal room light; do not capture in very dim light.
- Keep the pot edge visible only if it helps orientation.

## Sample collection

Before the screen arrives, collect real device samples:

```text
data/captures/healthy/
data/captures/local_issue/
data/captures/severe_or_bad_examples/
```

Take at least:

- 10 normal-light images
- 5 low-light images
- 5 close-up leaf images
- 5 wider whole-plant images

Keep wrong predictions. They are useful for improving the final model.

## Screen arrival steps

1. Connect the 7-inch RGB LCD with power off.
2. Enable `CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY=y`.
3. Build from an ASCII-only path.
4. Flash the board.
5. Run `plant_status_lcd_bridge.py`.
6. Confirm the LCD shows the same status printed by the bridge script.

## Do not add yet

Do not add SHT30/BH1750/soil moisture before the camera + LCD result loop is
stable. They are useful, but they add debugging variables.
