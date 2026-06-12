# Vendor MIPI Camera Firmware Test Report

Date: 2026-05-31

## Device

- Board serial port: `COM8`
- Chip: `ESP32-P4 revision v1.0`
- MAC: `30:ed:a0:e2:10:38`
- Flash: `16MB`

## Vendor Firmware

Tested file:

`350846021928_mipicamera.bin`

The file is a complete ESP32-P4 flash image with:

- bootloader at `0x2000`
- partition table at `0x8000`
- application at `0x10000`

It was written at flash offset `0x0`. `esptool` completed successfully and
verified the written data hash.

The firmware starts as project `28_mipicamera`, enters `app_main()`, and then
stalls in the `main` task. The task watchdog fires repeatedly every five
seconds. The reported `MEPC` remains `0x4802180e`.

Evidence:

`vendor_350846021928_mipicamera_serial.log`

## Independent OV5645 Probe

After testing the vendor firmware, the previous flash backup was restored and
the standalone SCCB probe was run again.

On the ATK shared I2C mapping, SDA `GPIO33` and SCL `GPIO32`:

- SCCB address `0x3C` acknowledges with `ESP_OK`
- OV5645 product ID registers return `0x5645`
- repeated-start, STOP-separated, raw SCCB, and GPIO bit-banged reads succeed
- five additional software resets reproduce successful detection five out of
  five times

Evidence:

`camera_csi_probe_serial_after_vendor_test.log`

`camera_csi_probe_repeat_resets_2026-05-31.log`

## Conclusion

The vendor-provided `28_mipicamera` example does not run successfully on this
board: it stalls in `app_main()` and repeatedly triggers the task watchdog.

The current evidence does **not** show an unrecognized camera. The independent
probe shows that the connected sensor is currently recognized as an OV5645 on
the ATK board mapping. This proves the SCCB control path only; it does not yet
prove successful MIPI CSI frame capture. The remaining problem is compatibility
or initialization behavior in the vendor example, not absence of the OV5645
SCCB device.

## Redetection After Power Cycle

After a subsequent board power cycle, the same standalone SCCB probe returned
to the no-ACK state. Address `0x3C` reports `ESP_ERR_NOT_FOUND`; all PID reads
return `0xFFFF`; and GPIO bit-banged transactions receive no ACK. Five
additional software resets reproduced the failure five out of five times.

Evidence:

`camera_csi_probe_redetect_2026-05-31.log`

`camera_csi_probe_redetect_repeat_2026-05-31.log`

The camera control path is therefore intermittent across board power cycles.
The same probe firmware can enter a stable recognized state and a stable
unrecognized state. This points to a contact, cable, power, or board-path
problem rather than only a software configuration issue.

## Second Vendor Flash Cycle

Starting from the stable no-ACK state, five more software resets remained
failed. The vendor image was flashed again and the exact same 16 MiB probe
backup was restored.

After restoration, OV5645 detection returned immediately and remained
successful across five additional software resets:

- address probe `0x3C`: `ESP_OK`
- repeated-start PID read: `0x5645`
- GPIO bit-banged PID read: `0x5645`, with write and read ACK

Evidence:

`camera_csi_probe_retry_before_vendor_cycle_2026-05-31.log`

`vendor_350846021928_cycle2_serial.log`

`camera_csi_probe_after_vendor_cycle2_2026-05-31.log`

The vendor image panicked in the second-stage bootloader with
`Store/AMO access fault` during this cycle, before reaching application camera
initialization. The camera state transition is therefore more likely related
to the flash/reset sequence, elapsed powered time, or an intermittent
electrical path than successful camera initialization by the vendor example.
