# Plant Monitor UI + Touch v2

## Reference Pattern

The old screen followed a desktop-pet layout. The current direction follows
open-source plant-monitor dashboards such as PlantPulse / PlantMonitoringSystem:
a plant visual anchor, clear plant state, sensor tiles and an explicit
interaction signal.

## Screen Layout

Target display: 800 x 480 RGB LCD.

```text
top bar      Plant Health                                  LIVE / TOUCH RECEIVED
left         large potted plant, lifted leaves on touch, issue spots
right top    plant state, care advice, health and vision bars
right mid    touch signal / mood acknowledgement
bottom       soil, temperature, air humidity, light metric tiles
```

The UI now treats the screen as a plant care instrument instead of a pet game.
The main plant drawing is built from native LCD primitives, so no third-party
sprite artwork is required for the new look.

## Touch Behavior

Supported serial inputs:

```text
TOUCH
VISION status=HEALTHY conf=91 touch=1
RESULT status=HEALTHY conf=91 mood=HAPPY health=90 advice=KEEP touch=1
```

Behavior:

- `TOUCH` reuses the latest visual status and current live sensor readings.
- Any command with `touch=1` renders the touch-response state immediately.
- The header changes to `TOUCH RECEIVED`.
- The plant leaves lift slightly, ripple rings appear around the plant, and
  the signal panel changes from `READY` to `ACK`.
- The display log includes `touch=1 layout=plant_monitor`.

## Physical Sensor Hook

When the plant touch sensor is wired, the driver should emit the same logical
event as the serial `TOUCH` command. Keep debounce in the sensor layer:

- confirm touch for 100-150 ms
- suppress repeated events for 300-500 ms
- rebuild baseline after watering or large soil-reference drift
