# Standalone I2C OLED measurement and test guide

Environment `oled_test`, source `src/oled_test.cpp`. Provisional SSD1306 driver
and 128x64 geometry; photos do not establish the exact controller or electrical
rating. Test at 3.3 V first. Do not substitute 5 V if it fails.
The module has front labels GND, VCC, SCL, SDA. Follow labels, not a memorized
pin order: the order looks reversed when viewed from the back.
Back markings 0x78/0x7A use shifted addresses, corresponding to 7-bit
0x3C/0x3D. Do not alter the address jumper; scan to find the actual address.
An ACK identifies an address, not the controller or panel resolution.

## Before connecting signals

1. Disconnect USB and all other power. Disconnect ALL RTC wires from the
   ESP32/bus for the initial OLED test. The modified RTC may retain its cell
   while stored disconnected; do not measure resistance on the battery-powered
   RTC. Its backup battery will preserve time independently.
2. Leave the OLED completely disconnected. In resistance mode measure:
   SDA-to-VCC and SCL-to-VCC. Record both values. A stable few kOhms suggests
   onboard pull-ups; open/nonlinear readings can reflect different module
   circuitry. Report ambiguous, OL or near-zero readings before proceeding.
   Do not add pull-ups without checking what is already fitted.
3. Return the meter to DC voltage mode. With USB OFF connect only OLED
   GND to common GND and VCC to the verified 3.3 V rail. Keep SDA/SCL detached
   from the ESP32. Keep the expansion-board jumper at 3.3 V. Use ESP32 USB-C
   as the only power source.
4. Power USB. Measure OLED VCC-to-GND, SDA-to-GND and SCL-to-GND. With onboard
   pull-ups to 3.3 V and idle signals, expect approximately 3.3 V on each.
   If signals are low, above 3.3 V, floating or otherwise unexpected, disconnect
   power and report results before proceeding. Avoid bridging pins with probes.
5. Disconnect USB again before adding SDA/SCL connections. Never use resistance
   or continuity mode on a powered circuit. Do not use meter current mode here.

The front display has a protective-film pull tab; avoid pulling the flex cable
or pressing on the glass. Film removal is optional for the test.

## Wiring after measurements pass

| OLED label | ESP32 connection |
| --- | --- |
| GND | Common GND |
| VCC | Verified 3.3 V rail |
| SCL | D22 / GPIO22 signal |
| SDA | D21 / GPIO21 signal |

Use signal pins S on the expansion board, not its V column. Verify any separate
VCC-labelled power header before using it. Keep jumpers short. Four-lid/LED
wiring may stay connected: this firmware holds those LEDs off, does not read
lids, and does not run medication sessions or schedules. Initially leave RTC,
EEPROM and other I2C modules disconnected.

## Build, upload and monitor

The new environment alone pins Adafruit SSD1306 2.5.17, Adafruit GFX 1.13.0,
and Adafruit BusIO 1.17.4. PlatformIO may need internet on its first build to
download these dependencies into its ignored local library directory.
Existing environments and source filters are unchanged.

Close other serial monitors. From the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Substitute the current CP210x port for COMx:
& '.\.tools\venv\Scripts\pio.exe' run -e oled_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e oled_test --port COMx --echo
```

Baud 115200. Click terminal, type a command and press Enter. Ctrl+C exits.
Opening the monitor may reset ESP32. Press EN if startup output was missed.

## Firmware and commands

Startup scans at 100 kHz. If exactly one address among 0x3C/0x3D responds, it
automatically sends provisional SSD1306 128x64 initialization commands and
draws the pattern. If neither/both respond, it leaves initialization to the
explicit init command after wiring/address review. OLED traffic stays at
100 kHz during and after library calls. There is no external reset pin.

| Command | Behavior |
| --- | --- |
| `scan` | Show current 7-bit I2C ACKs; no automatic reinitialization |
| `init 0x3c` / `init 0x3d` | Require ACK, then send initialization to selected address |
| `pattern` | Border at x=0/127 and y=0/63, four corner squares, title, address, uptime counter |
| `text` | Top and bottom text, left/right labels, upper/lower-case alphabet rows |
| `clear` | Send a blank framebuffer |
| `off` / `on` | Turn panel off/on, preserving selected view |
| `all-on` | Command all pixels on for two seconds, then automatically restore pattern |
| `normal` | Restore framebuffer mode, normal polarity, display on and pattern |
| `status` | Initialization/address, most recent address probe, last framebuffer-transfer duration |
| `help` | Command list |

Patterns refresh approximately once per second, with moderate contrast 0x7F.
The timed all-on diagnostic suspends pattern rendering and does not extend
its timer if the command is repeated. `off` cancels it; `normal` ends it early.
The counter is ESP32 uptime, NOT RTC time. The text rows show A through U and
a through u (21 characters each), intentionally fitting the screen width.
Existing external LEDs should stay off. Unsupported/overlong serial commands
do not execute a partial command.

The driver allocates a 1024-byte framebuffer at initialization. Its begin()
success is principally allocation success, not controller identification;
the test separately checks address ACK before initializing/rendering. The
library does not return per-frame I2C transmission status. `frame_us` measures
the blocking framebuffer call, not proof that all bytes arrived or the screen
looks correct. Library code is appropriate for this isolated experiment; a
later medication integration must assess its update rate and blocking latency.
At 100 kHz a full 1024-byte frame takes roughly 0.1 second, so a later integration
cannot simply assume every display refresh preserves the sensor response target.
No RTC time/status or EEPROM contents are written by this test.

## Physical checks, in order

1. **Measurements:** record the two resistances and three voltages above.
2. **Detection:** after upload, confirm ACK at 0x3C or 0x3D and record it.
3. **Pattern:** see OLED TEST, provisional geometry line, I2C address and
   advancing Seconds counter. All four corner squares and the complete thin
   border should be visible, with no horizontal offset, wrap, noise or clipping.
   Readable text alone does not verify all rows/columns.
4. **Text:** send `text`; check top and bottom rows, LEFT/RIGHT labels,
   A–U and a–u rows. Characters should be upright and stable.
5. **Blank and power commands:** `clear` should blank the display; `pattern`
   restores it. `off` blanks it; `on` restores the view/counter.
6. **Stability:** leave pattern running two minutes. Expect stable image,
   changing counter and no repeated reset or no-ACK messages. Send `status`
   and record frame_us. EN should re-scan and restore the pattern.

If it ACKs but stays blank or looks shifted/clipped, report serial output and
a display photo. We may need a different controller driver or geometry; do
not change supply voltage or solder jumpers to compensate.

## Later shared-bus check (only after standalone passes)

Disconnect USB before reconnecting RTC VCC/GND/SDA/SCL in parallel. Preserve
its charging-path modification and battery polarity. Pull-ups from both boards
combine in parallel: R_total = 1/(1/R_OLED + 1/R_RTC). Two 4.7 kOhm sets give
about 2.35 kOhm; different measured values require review before connection.
Never assume extra pull-ups are needed. Keep all signal pull-ups on 3.3 V.
If combined resistance is measured directly, first remove ALL power sources,
including RTC battery, and document its resulting time-validity status later.

With oled_test running, `scan` should show the OLED plus RTC 0x68 and possibly
EEPROM 0x50–0x57. Check OLED stability. This establishes only bus coexistence,
not valid RTC readings or full medication timing. A later integration must
verify RTC reads during rendering and measure lid-response/update latency.

## Validation — 2026-10-22

Module assessment is based on user photos; exact controller/supply rating is
unverified. Electrical measurements, upload, visual tests and shared-bus checks
are pending user observations. No hardware behavior is inferred from a build.
Existing automated behavior tests were not changed.

Executed verification: oled_test compiled and linked successfully with the
pinned dependencies (22,040 bytes static RAM, 301,441 bytes flash; the display
framebuffer is allocated later at runtime). Checksums confirmed the ten checked
existing firmware/core/test source files were unchanged. Existing automated
behavior tests were not rerun for this isolated addition. No upload or physical
measurements were performed by the agent. The sandbox dependency download
failed; an approved outside-sandbox retry installed the libraries. Its first
build encountered missing generated output directories; a serial (-j 1) retry
completed successfully.

The previously deferred scheduler checks (overlap/invalid command rejection,
no replay, reset, empty arm, plus optional physical midnight rollover) remain
pending before final integrated validation; the OLED test does not replace them.

## Blank-screen diagnostic update

The user reported a blank panel despite 0x3C ACK, initialized=yes, screen=on,
and frame_us=97471, with only OLED connected and 3.3 V measured at its header.
Startup contrast was changed from minimum to moderate 0x7F. After uploading
this update, first inspect the normal pattern, then send `all-on` and watch for
two seconds. It should illuminate the full panel, then restore the pattern.
Send `normal` to explicitly restore normal framebuffer mode if needed.
An illuminated all-on panel supports investigating framebuffer/controller
configuration; a blank all-on panel still does not alone identify a bad panel
versus an incompatible initialization/controller. No supply or wiring changes
are proposed. Physical diagnostic results remain pending.
The updated oled_test compiled/linked successfully: 22,048 bytes static RAM,
302,121 bytes flash. No upload was performed by the agent. Existing firmware
and medication logic were not changed.

## Driver references

- [Adafruit SSD1306 source](https://github.com/adafruit/Adafruit_SSD1306)
- [Adafruit GFX source](https://github.com/adafruit/Adafruit-GFX-Library)
