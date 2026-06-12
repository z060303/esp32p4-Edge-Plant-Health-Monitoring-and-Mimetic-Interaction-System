# ATK-MCOV5645 MIPI Camera Notes

## Archived Vendor Documents

The vendor archive has been extracted to:

`vendor_docs/ATK-MCOV5645`

Useful normalized copies:

- `vendor_docs/ATK-MCOV5645/schematic.pdf`
- `vendor_docs/ATK-MCOV5645/mipi_spec.pdf`

## Confirmed Module Details

- Camera sensor: OV5645
- Interface: 2-lane MIPI CSI
- SCCB/I2C address: `0x3C`
- Module supply: `3.3V`
- The module includes an active oscillator and LDO regulators.
- The 22-pin FPC connector and the 2 x 11-pin header are alternative connection methods.
- When using the FPC cable, the lower pin header does not need additional wiring.

## FPC Signals

The 22-pin FPC connector includes:

- MIPI clock and two MIPI data lanes
- `CSI_CLK`
- `CSI_RST`
- `CSI_PWDN`
- `I2C_SCL`
- `I2C_SDA`
- `VCC_3V3`
- Ground

## Probe Result

The ESP32-P4 camera probe currently scans:

- `GPIO7/GPIO8`: device `0x44` detected
- `GPIO33/GPIO32`: devices `0x10`, `0x1E`, `0x24`, `0x50`, `0x6A`, `0x7E` detected

The expected OV5645 SCCB address `0x3C` is not detected.

## ATK-DNESP32P4 Baseboard Mapping

The ATK-DNESP32P4 hardware reference manual confirms:

- SCCB/I2C SCL: `GPIO32`
- SCCB/I2C SDA: `GPIO33`
- `CSI_RST`: `GPIO34`
- `CSI_PWDN`: `GPIO54`

The baseboard uses level shifters because these camera control signals operate at
`1.8V` on the camera side.

Normalized baseboard document copies:

- `vendor_docs/ATK-DNESP32P4/baseboard_schematic.pdf`
- `vendor_docs/ATK-DNESP32P4/hardware_reference_manual.pdf`
- `vendor_docs/ATK-DNESP32P4/io_pin_assignment.xlsx`

## Dedicated Baseboard Probe Result

The probe firmware was updated to:

- use SCCB/I2C on `GPIO33/GPIO32`
- drive `CSI_PWDN` on `GPIO54` from high to low
- drive `CSI_RST` on `GPIO34` from low to high
- scan the SCCB bus after the explicit power-on reset sequence

The OV5645 address `0x3C` is still not detected. The next diagnostic step is to
measure the camera-module-side `VCC_3V3` rail and verify the FPC contact path.

## Official ATK Example Cross-Check

The official `43_comprehensive_routine` source from the ATK-DNESP32P4 starter
materials was archived under:

`vendor_docs/ATK-DNESP32P4/starter_materials/comprehensive`

The official full-board flow passes the existing shared I2C bus handle into the
camera initialization path and sets camera `reset_pin` and `pwdn_pin` to `-1`.
The probe firmware was changed to match this strategy and reflashed. The
OV5645 address `0x3C` is still not detected.

Photos confirm that the FPC is plugged into the board-side vertical `MIPI-CSI`
socket rather than the nearby horizontal `MIPI-DSI` socket. Both socket latches
appear closed and the cable insertion depth looks reasonable.

## Multi-Path Standalone Diagnostic

The standalone probe was expanded to test the camera without any other external
peripherals connected. It checks the OV5645 product ID registers `0x300A` and
`0x300B`; a valid module must return `0x56` and `0x45`.

Tested bus candidates:

- ATK full-board shared I2C: SDA `GPIO33`, SCL `GPIO32`
- ATK generic CSI configuration candidate: SDA `GPIO31`, SCL `GPIO34`
- ESP-IDF generic CSI example candidate: SDA `GPIO7`, SCL `GPIO8`

Tested SCCB/I2C methods:

- standard address probe for `0x3C`
- repeated-start register read
- STOP-separated SCCB register read
- raw SCCB transaction with ACK checking disabled
- GPIO bit-banged SCCB on the ATK full-board mapping
- nine-clock bus recovery before bit-banged reads
- all four combinations of `CSI_RST` (`GPIO34`) and `CSI_PWDN` (`GPIO54`)

No method returned the OV5645 ID. GPIO bit-banged reads returned `0xFFFF`, and
the camera never acknowledged a transaction.

## Official Acceptance Firmware Cross-Check

The vendor-provided `mini_board_mipi_camera.bin` complete flash image was also
written successfully at offset `0x0`. It panics during early startup before
camera initialization on this board combination, so it cannot validate the
camera module here. The standalone SCCB diagnostic remains the useful test.

During one high-speed attempt to write the vendor image, the USB connection
dropped mid-transfer. After restoring the standalone diagnostic, the ATK shared
I2C clock line changed from an idle high level to a low level and the address
probe changed from `ESP_ERR_NOT_FOUND` to `ESP_ERR_TIMEOUT`. This intermittent
electrical state is consistent with a contact, cable, power, or board-path
problem rather than only a camera-driver configuration issue.

## Current Conclusion

The firmware and SCCB timing variants have been exhausted far enough to rule
out the common software causes. The next safe step is a power-off hardware
inspection and a known-good replacement FPC cable or module-side power
measurement. Do not move or reseat the FPC cable while the board is powered.

## 2026-05-31 State Change

A new vendor image, `350846021928_mipicamera.bin`, was flashed successfully at
offset `0x0`. It is different from the previously archived
`mini_board_mipi_camera.bin`. The new image boots as project `28_mipicamera`,
but stalls in `app_main()` near the LCD/MIPI-DSI initialization path and
repeatedly triggers the task watchdog.

After restoring the exact 16 MiB flash backup taken immediately before that
test, the standalone SCCB diagnostic detected the camera on SDA `GPIO33` and
SCL `GPIO32`:

- address probe `0x3C`: `ESP_OK`
- repeated-start PID read: `0x5645`
- STOP-separated PID read: `0x5645`
- raw SCCB PID read: `0x5645`
- GPIO bit-banged PID read before touching control pins: `0x5645`

Five additional software resets reproduced the successful detection five out
of five times. The incorrect bus candidates still fail, which argues against a
floating-bus false positive.

This changes the narrow conclusion: the OV5645 SCCB control path is currently
working. It does not yet prove that MIPI CSI frame capture works. The transition
from the earlier no-ACK state remains unexplained. Given the earlier
`ESP_ERR_NOT_FOUND` / `ESP_ERR_TIMEOUT` changes and I2C clock-line state change,
an intermittent contact, cable, power, or board-path condition remains the
leading explanation. A persistent state left by a prior firmware run is also
possible but not proven.

## 2026-05-31 Redetection After Power Cycle

After the board was power-cycled without changing the standalone SCCB probe
firmware, the camera returned to the earlier no-ACK state:

- address probe `0x3C`: `ESP_ERR_NOT_FOUND`
- repeated-start PID read: `0xFFFF`
- STOP-separated PID read: `0xFFFF`
- raw SCCB PID read: `0xFFFF`
- GPIO bit-banged PID read: `0xFFFF`, with no write or read ACK

Five additional software resets reproduced this failure five out of five times.

This provides stronger evidence for an intermittent hardware-path or
power-state problem. The same firmware and pin mapping can enter a stable
working state and a stable no-ACK state across board power cycles. Software
reset alone does not recover the failed state.

## 2026-05-31 Vendor Flash Cycle Reproduction

Starting from the stable failed state above, five more software resets remained
failed five out of five times. The new vendor image
`350846021928_mipicamera.bin` was then flashed again, followed by restoration of
the exact same 16 MiB standalone-probe backup.

The restored standalone probe immediately detected OV5645 again:

- address probe `0x3C`: `ESP_OK`
- repeated-start PID read: `0x5645`
- GPIO bit-banged PID read: `0x5645`, with write and read ACK

Five additional software resets reproduced successful detection five out of
five times.

During this second cycle, the vendor image repeatedly panicked in the second
stage bootloader with `Store/AMO access fault` before reaching application
camera initialization. Therefore, the state transition cannot be attributed to
successful camera setup by the vendor application. The flash/reset procedure
itself, elapsed powered time, or an intermittent electrical path remains the
more likely trigger.

## 2026-05-31 Stable-Recovery Firmware Cold-Boot Result

A recovery-focused firmware was built and flashed. It removes the generic
`GPIO31/GPIO34` bus candidate because `GPIO34` is the real baseboard
`CSI_RST` signal. It only probes the confirmed shared SCCB bus on
`GPIO33/GPIO32`, keeps the empirically working host-side control state
`RESET=1, PWDN=1`, tries three rounds of long control pulses, and then retries
for another 60 seconds.

In the warm state, five esptool hard resets detected `PID=0x5645` five out of
five times. After a real ten-second power-off cycle, the camera returned to the
stable no-ACK state. The recovery firmware continued to receive
`ESP_ERR_NOT_FOUND` from address `0x3C` for approximately 78 seconds. All
control sequences failed. Three further hard resets and a same-firmware reflash
also failed.

During a later read-only full-flash control experiment, the ESP32-P4
USB-Serial/JTAG connection disappeared after approximately 63.3 seconds and
did not re-enumerate until USB power was unplugged and reconnected. After USB
reconnection, the camera still did not acknowledge address `0x3C`.

This closes the firmware-only recovery path. The evidence is consistent with
an intermittent camera module, FPC cable, camera power/clock path, or baseboard
fault. A replacement-device evidence summary is available at:

`docs/evidence/camera_replacement_evidence_2026-05-31.md`
