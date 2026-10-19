# Battery-free DS3231 standalone test

Environment `rtc_test`, source `src/rtc_test.cpp`. Framework Wire only;
no RTC library download. This firmware does not run medication logic, read
lids, schedule doses or write EEPROM. Existing four LEDs are explicitly off.
Other firmware sources, tests and the medication core are unchanged.

## Module inspection and limits

User photos show a DS3231-labelled module with six header labels:
32K, SQW, SCL, SDA, VCC, GND. The user reads the small chip as
ATMCL432 / 24C32N / SU27 / D, consistent with an Atmel AT24C32 EEPROM.
Its exact voltage-grade ordering code is not independently established.
The proposed 3.3 V test is consistent with the reported SU27 marking.
The larger RTC chip's full marking is not legible in the supplied photo.

A glass diode and 102-marked (1 kOhm) resistor formed the mapped external
battery path; the initial 201 reading was corrected by later close-ups.
The CR2032 is non-rechargeable. Initial tests below were battery-free.
The user subsequently removed the resistor and passed the backup test;
see the dated modification record below. Do not bridge or restore that resistor.
Merely identifying the EEPROM does not identify the charging circuit.

## Measurements before full connection

1. Disconnect USB, all other power, and all wires from the RTC. Remove battery.
2. In resistance mode, measure RTC SDA-to-VCC and SCL-to-VCC. A stable value
   of a few kOhms (often around 4.7 kOhms) is consistent with onboard pull-ups.
   Semiconductor paths can affect readings, so this is a screening check, not
   a complete circuit identification. Report open-circuit, near-zero or unclear
   readings before continuing. Do not add extra pull-ups blindly.
3. Return the meter to DC voltage mode. With power off connect ONLY RTC VCC
   to the verified 3.3 V rail and RTC GND to common GND. Leave SDA/SCL detached
   from ESP32. Leave the expansion jumper at 3.3 V.
4. Power via ESP32 USB-C only. Measure at the RTC header: VCC-to-GND,
   SDA-to-GND and SCL-to-GND. Expect approximately 3.3 V on all three with
   SDA/SCL idle and module pull-ups fitted. Keep probes from bridging pins.
   Stop and report unexpected voltages or a low/stuck line.
5. Disconnect USB again before connecting the two signal wires below.

Use resistance/continuity mode only when the circuit is unpowered and battery
removed. Never place a meter in current mode across power or the battery holder.
The battery holder need not be probed for this initial battery-free test.

## Wiring after measurements pass

| RTC header label | Connection |
| --- | --- |
| VCC | Verified 3.3 V rail |
| GND | Common GND |
| SDA | D21 / GPIO21 signal |
| SCL | D22 / GPIO22 signal |
| SQW | Unconnected |
| 32K | Unconnected |

Use the printed RTC pin labels, not a remembered physical order. On the
expansion board use the S signal pins for D21/D22; V is a power rail, not data.
Do not assume the separate header marked VCC has the selected rail without
measuring it. No 5 V signal pull-ups. Keep jumpers short and OLED/other I2C
modules disconnected. The four reed/LED circuits may remain connected.
Only use the ESP32 USB-C supply, not simultaneous expansion-board power inputs.

## Build, upload and monitor

Close any other serial monitor. In PowerShell from the project root:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with the currently detected Silicon Labs CP210x port:
& '.\.tools\venv\Scripts\pio.exe' run -e rtc_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e rtc_test --port COMx --echo
```

115200 baud. Click inside the monitor, type a command, then press Enter.
Ctrl+C exits. Opening the monitor can reset ESP32; press EN if startup was
missed. Startup scans the bus and reads time; it never automatically sets time.

## Commands and interpretation

| Command | Effect |
| --- | --- |
| `help` | Command summary |
| `scan` | Probe 7-bit addresses 0x08–0x77 without writing device data |
| `status` | Read calendar registers and oscillator-stop flag |
| `set YYYY-MM-DD HH:MM:SS` | Validate and explicitly write date/time in 24-hour mode; enable oscillator and clear OSF |

Example syntax only: `set 2026-10-18 14:30:00`. Substitute the actual date/time
you choose, rather than copying that example as current time. Input is a
literal bench clock: no timezone, daylight-saving or UTC conversion. Year range
is 2000–2099. Day-of-week is computed (Monday=1); invalid calendar dates,
out-of-range fields, trailing text and overlong commands are rejected.

RTC address should be 0x68. EEPROM may appear at 0x50–0x57 depending on address
straps; record what is actually detected. An ACK is not proof of chip identity.
No EEPROM contents are read or written. Time is read about once per second.
The I2C bus is 100 kHz with bounded transaction timeout; read/write failures
are reported, not replaced with invented time values.

OSF=1 means the oscillator stopped at some point and time must be treated as
untrusted. A plausible date with OSF=0 still needs comparison to a reference;
it is not independent proof the time is correct. The explicit set command
clears OSF only after the time write succeeds. If an I2C write fails, some
registers may already have changed; recheck communication and explicitly set
again. The firmware can read 12-hour format but always sets 24-hour format.

## Physical checks, in order

1. **Measurement checks:** record both pull-up resistance readings and all
   three voltage readings. Continue only after the expected results.
2. **Detection:** upload/open monitor; expect 0x68, possibly an EEPROM address.
   Record any initial OSF warning or invalid-register message. With no backup
   battery, invalid initial time is expected after complete power loss.
3. **Explicit set:** enter your chosen current time using `set ...`. Readback
   should match within a few seconds, with OSF=0. Check seconds advance for at
   least 60 seconds and compare elapsed time against an independent clock.
   This is a basic functional check, not a precision oscillator calibration.
4. **ESP32 reset:** note the RTC time, press EN while leaving USB connected.
   Verify RTC continues from the previous time (allowing elapsed seconds),
   not from the earlier set value. This is NOT a battery-retention test.
5. **Minute/date rollover:** optionally set `2026-12-31 23:59:55`; expect
   `2027-01-01 00:00:00`. Restore the actual current date/time afterward.
6. **Invalid input:** try `set 2026-02-30 12:00:00` and
   `set 2026-10-18 24:00:00`. Both must report errors; the clock should keep
   advancing from its previous time rather than being overwritten.

Do not disconnect/reconnect signal wires while powered to provoke errors.
Do not test USB-disconnected retention yet. If power is removed, no retention
is claimed and the time may need setting again. Keep the CR2032 removed.

## Validation record — 2026-10-18

Module inspection is based on user photos/markings, not an independently
verified schematic. Subsequent charging-path modification and backup test
are recorded below; early battery-free instructions describe the original test.
Measurements, upload and powered RTC checks passed as reported below.
Existing automated behavior tests were not rerun for this standalone addition.
PlatformIO rtc_test compilation/linking passed: 21,888 bytes static RAM,
289,589 bytes flash. The sandbox compiler-execution failure was resolved by
an approved outside-sandbox build retry. Build success does not verify the
physical module, timekeeping or measurements. No upload was performed here.

### User-reported physical results

| Check | Result |
| --- | --- |
| Unpowered SDA-to-VCC and SCL-to-VCC | Approximately 4.7 kOhm each |
| Battery removed, powered at 3.3 V | VCC, SDA and SCL approximately 3.3 V relative to GND |
| rtc_test upload | User reported success |
| RTC detection | ACK at 0x68 |
| Initial status | OSF=1 before explicit setting |
| Explicit setting and advancing time | PASS; OSF=0, time advances normally |
| EN reset while USB remains connected | PASS; time continues without resetting to set value |
| Invalid date/time rejection | PASS as reported |
| Year rollover | PASS as reported |
| Restoration of actual time | User confirmed completed |

Evidence is the user's manual observations, not archived serial traces or
agent-observed hardware. No precision calibration has been performed.
Battery-backed retention was subsequently checked as recorded below.
Recording these results did not change firmware or hardware.

## Next diagnostic: unpowered charging-path identification

Disconnect USB and every wire from the module; keep the battery out. First
identify the holder negative contact by continuity to the GND header; the
other isolated holder contact is battery positive. Do not infer polarity
from the visible holder spring alone. Confirm with holder markings/photo.
Then identify the resistor and glass diode in a close-up photo. Later photos
corrected the earlier reading: this board has a 102-marked, 1 kOhm resistor
beside the diode, not a 201-marked, 200 Ohm resistor.
Use resistance across the resistor, diode mode if available
across the diode in both directions, and continuity to map their end pads to
VCC and battery positive. A silicon diode may show roughly 0.5–0.8 V forward
and OL reverse, but in-circuit paths can alter readings. The suspected chain
is VCC -> resistor/diode in series -> battery positive; component order must
be measured, not assumed. Check diode direction as well as connectivity.
No beep across a diode is not evidence of isolation. Do not use current mode,
power the board, fit a cell, cut traces or remove components during this check.
If probe access is poor or readings ambiguous, stop for photo/measurement
review rather than interpreting the charging path as absent.

### Charging-path modification and backup retention — user report, 2026-10-18

Pad names refer to the close-up with the diode vertical and its adjacent
102 resistor horizontal: R-L/R-R are resistor left/right; D-T/D-B are diode
top/bottom. Unpowered readings mapped VCC -> R-R -> 1 kOhm resistor -> R-L
-> D-B -> diode -> D-T -> battery positive. Battery negative measured 0 Ohm
to GND. No diode-mode measurement was available; its forward behavior was
not measured. Approximately 55–56 kOhm in resistance mode across that path
was not accepted as proof of isolation.

The user removed only the 1 kOhm resistor beside the glass diode (not the
other 102 resistor by the POWER LED). After removal:
- R-R to VCC, R-L to D-B, D-T to battery positive, and battery negative to
  GND each measured approximately 0 Ohm.
- Empty resistor pads measured OL; no solder bridge or damaged pads reported.
- Battery-free powered RTC checks passed again.
- Loose CR2032 measured approximately 3 V before installation.

The user then fitted the CR2032 with correct polarity, set/checked time and
disconnected USB for five minutes. On reconnection the RTC retained correct
time and OSF stayed 0: **PASS for this manual backup-retention check**.
This supports operation after interruption of the mapped external charging
branch; it is not a measured battery-current certification, long-term battery
life test or precision calibration. Do not restore/bridge the removed resistor.
The earlier battery-free instructions remain relevant before any further
resistance measurement or soldering: remove power and battery first.
No firmware or physical hardware changes were performed by the agent when
recording these results. Automatic scheduling/restart recovery remain untested.

## References

- [DS3231 manufacturer datasheet: registers, OSF, power and I2C](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf)
- [AT24C32 manufacturer datasheet](https://ww1.microchip.com/downloads/en/devicedoc/doc0336.pdf)
