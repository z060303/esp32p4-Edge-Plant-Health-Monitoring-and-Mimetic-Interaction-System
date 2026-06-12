# Screen prep for first hardware display version

## Recommended screen

Buy an official ALIENTEK LCD module that is supported by the existing
`camera_mipi_headless` BSP. Prefer the RGB 7-inch `800x480` module:

- Recommended: `ATK-MD0700R-800480`, RGB LCD, ID `0x7084`.
- Also supported: `ATK-MD0430R-480272`, RGB LCD, ID `0x4342`.
- Also supported: `ATK-MD0700R-1024600`, RGB LCD, ID `0x7016`.
- Larger options supported by code: `ATK-MD1018R-1280800`, RGB LCD, ID `0x1018`.

Do not buy a random SPI ST7789 screen for the first version unless the goal is
only small text output. The current camera firmware already contains ALIENTEK
RGB/MIPI LCD support, so the official matching panel avoids a driver port.

## Why the 7-inch 800x480 RGB panel

- Large enough to show camera preview plus health status.
- Already detected in `components/BSP/LCD/lcd.c` as ID `0x7084`.
- Already handled in `main/APP/MIPI_CAM/mipi_cam.c`.
- Easier to demo than a small SPI screen.

## Firmware preparation already done

- Added optional `CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY`.
- Added `main/APP/PLANT_DISPLAY`.
- Added `main/APP/PLANT_ENGINE` for ESP32-P4-side rule fusion.
- Firmware can parse serial commands like:

```text
RESULT status=HEALTHY conf=87 mood=HAPPY health=100 advice=KEEP
VISION status=HEALTHY conf=87 soil=45 temp=26 hum=55 light=500
ANALYZE soil=45 temp=26 hum=55 light=500 vision=HEALTHY conf=87
```

When LCD display is enabled, the board shows:

- Plant AI title
- Status
- Confidence
- Mood
- Health score
- Short advice

## On arrival

1. Power off the board.
2. Connect the RGB LCD cable to the board LCD connector.
3. Check cable orientation and lock the FPC connector.
4. Keep the OV5645 MIPI camera connected.
5. Enable LCD display in `camera_mipi_headless/sdkconfig`:

```text
CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY=y
```

6. Build from an ASCII-only path to avoid Windows Chinese-path toolchain issues:

```powershell
Copy-Item camera_mipi_headless C:\codex_build\plant_ai_camera_mipi_headless -Recurse -Force
cd C:\codex_build\plant_ai_camera_mipi_headless
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. 'C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1'
idf.py build
idf.py -p COM8 flash
```

7. Run the PC-side bridge:

```powershell
python tools\plant_status_lcd_bridge.py --port COM8 --command PREVIEW
```

## First-version data path

```text
OV5645 camera on ESP32-P4
-> serial RGB565 image export
-> PC YOLO plant status model
-> VISION serial command
-> ESP32-P4 C rule engine
-> ESP32-P4 LCD display/log
```

## Acceptance test

- Camera log contains `Detected Camera sensor PID=0x5645`.
- LCD backlight turns on.
- PC bridge saves a PNG image.
- PC bridge prints a prediction.
- LCD changes from `BOOTING` to `HEALTHY`, `LOCAL_ISSUE`, or `SEVERE`.
