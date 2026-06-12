# Whole-Plant Touch Feedback Plan

Status: deferred until the main plant-monitoring workflow is complete.

## Goal

Detect a user's touch or gentle stroking on any leaf of the plant and emit one
touch event for the interaction layer. The first implementation should be
inexpensive and easy to prototype. It does not need to identify which leaf was
touched.

## Low-Cost First Version

Use the ESP32-P4 capacitive-touch peripheral. Treat the plant as one coupled
sensing object instead of attaching a sensor to every leaf.

```text
hand touches any leaf
        |
whole-plant capacitance changes
        |
conductive-fabric electrode loosely attached near the base of the main stem
        |
short shielded wire + 1 kOhm series resistor
        |
ESP32-P4 touch channel
```

Add a soil reference channel:

```text
304 stainless-steel soil probe
        |
10 kOhm series resistor
        |
second ESP32-P4 touch channel
```

The soil reference is used to recognize watering and slow moisture drift. The
shielded wire should be kept short. Use the ESP32-P4 shield feature where the
selected board wiring allows it. Final GPIO assignments must be checked against
the development board pinout and other connected peripherals before assembly.

For a plant with multiple independent main stems, attach one small
conductive-fabric electrode to each stem and combine them into the plant sensing
input during prototyping.

## Shopping List

| Item | Quantity | Expected price |
| --- | ---: | ---: |
| Conductive-fabric tape, 10-20 mm wide | 1 roll | CNY 3-8 |
| Thin flexible single-core shielded wire | 2 m | CNY 3-6 |
| 304 stainless-steel probe or thick stainless-steel wire | 1 | CNY 2-5 |
| 1 kOhm and 10 kOhm resistors | several | about CNY 1 |
| Optional copper-foil tape | 1 roll | CNY 3-6 |

Expected prototype cost: under CNY 20 if the ESP32-P4 board is already
available.

## Software Behavior

1. Sample the plant channel continuously and maintain a slow-moving idle
   baseline.
2. Detect a touch from a relative change, not from a fixed absolute reading.
3. Confirm the change for about 100-150 ms before emitting an event.
4. Add a 300-500 ms release cooldown.
5. If the soil channel changes sharply, suppress touch events temporarily and
   rebuild the baseline after watering.
6. Capture raw readings for idle, leaf touch, stroking, pot touch, watering and
   nearby electrical interference before choosing thresholds.

## Upgrade Path

If the low-cost version remains too sensitive to cable length, humidity or
watering, upgrade to an FDC1004 capacitive-to-digital converter with active
shielding:

```text
CIN1  -> stem electrode
CIN2  -> soil reference probe
CIN3  -> optional copper foil on the pot for pot-touch detection
SHLDx -> cable shields
I2C   -> ESP32-P4
```

A piezoelectric disc on the inside of the pot can be added later as supporting
evidence for physical movement. It should not be the primary sensor because it
also reacts to table vibration and pot movement.

## Safety Notes

- Attach conductive fabric gently; do not tighten it around a growing stem.
- Do not pierce the stem or apply DC voltage to plant tissue.
- Keep sensor wiring away from display, motor and speaker-amplifier wiring.

## Reference

- ESP32-P4 capacitive-touch peripheral:
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/cap_touch_sens.html
- FDC1004 product page:
  https://www.ti.com/product/FDC1004
