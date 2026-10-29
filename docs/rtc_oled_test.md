# Combined RTC and OLED bench test

Environment `rtc_oled_test`; source `src/rtc_oled_test.cpp`. Calendar decoding
is isolated in include/RtcOledCalendar.h and checked on desktop. This is a
separate experiment, not a medication build: no lid handling, scheduling,
RTC time setting, EEPROM writes or dose-history storage. Four external LEDs
are held off. All existing firmware and test sources are preserved.

## Existing wiring: no changes needed

| Connection | OLED | RTC |
| --- | --- | --- |
| Verified 3.3 V rail | VCC | VCC |
| Common GND | GND | GND |
| D21 / GPIO21 signal | SDA | SDA |
| D22 / GPIO22 signal | SCL | SCL |

RTC SQW and 32K remain disconnected. Keep its charging resistor removed and
the correctly fitted CR2032 in place. Preserve the expansion jumper at 3.3 V.
Use ESP32 USB-C as the only main power source. No extra pull-ups are proposed:
both modules already passed the user's address-scan/coexistence check.
If any wiring is adjusted, disconnect USB first. Do not unplug live I2C wires
to test a fault. Resistance checks require all relevant power removed,
including the RTC cell if measuring that circuit.

## Behavior

- Shared I2C bus at 100 kHz, SDA21/SCL22, transaction timeout 20 ms.
- OLED fixed at the physically tested 0x3C, provisional SSD1306 128x64
  configuration with working moderate contrast 0x7F.
- RTC at 0x68. Read one 16-byte register snapshot, covering calendar and
  control/status through 0x0F. Only the register pointer is selected for
  reading; no RTC register data is written or flags cleared.
- Display date, large 24-hour time and `RTC OK OSF=0` on valid reads.
- `RTC TIME INVALID` for OSF=1, malformed calendar/BCD, reserved bits or
  unsupported century. Supported years are 2000–2099; 12-hour register values
  are converted to 24-hour display. No timezone/DST conversion.
- `RTC READ ERROR` for unsuccessful/short I2C reads. Clear the prior timestamp
  from each newly composed frame so stale time is not deliberately reused.
- Take a fresh sample/display update about once a second. Serial prints the
  same date/time snapshot plus OLED availability and blocking `frame_us`.
  The displayed time may lag the current instant by a frame transfer and
  the one-second refresh interval. It is RTC calendar time, not ESP32 uptime.
- OSF=0 and a plausible calendar do not prove accuracy; compare with a reference.
  This is not a calibrated clock test or frozen-oscillator detector.
- OLED ACK/allocation checks do not prove successful rendering. If the entire
  bus/OLED fails, a fault message cannot be guaranteed to reach the screen;
  serial diagnostics remain necessary and the physical display may freeze.

The pinned Adafruit dependencies are inherited from oled_test. Existing
environments are unchanged. A 1024-byte framebuffer is allocated at startup;
full-frame transfers remain blocking. Approximately 97 ms was measured by the
user in oled_test. This experiment does not establish medication-loop latency
or justify adding display transfers unchanged to the lid-monitoring loop.

## Upload and monitor

Leave the verified combined wiring in place. Close other serial monitors.
From the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with the current CP210x port:
& '.\.tools\venv\Scripts\pio.exe' run -e rtc_oled_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e rtc_oled_test --port COMx --echo
```

115200 baud. Click inside the terminal, type and press Enter. Ctrl+C closes
the monitor. Opening it may reset ESP32; EN repeats initialization. The first
build may install the already-pinned libraries for this separate environment.

| Command | Effect |
| --- | --- |
| `status` | Fresh RTC read, display update and serial snapshot/timing |
| `scan` | Show address ACKs; expect 0x3C/0x68, possibly EEPROM |
| `init` | Retry OLED initialization at 0x3C, then read/update |
| `help` | Command summary |

There is deliberately no `set` command. If RTC time is invalid or incorrect,
use the existing rtc_test to set it explicitly, then restore this firmware.
Long/unknown commands are rejected without partial execution.

## Physical acceptance tests, in order

1. **Detection:** after upload expect ACKs at 0x3C and 0x68. The OLED should
   show `RTC + OLED TEST`, date, large time and `RTC OK OSF=0`.
2. **Agreement:** send `status`; compare the serial timestamp and OLED. They
   should match the same snapshot (allow for a refresh between observations).
   Compare both to an independent reference clock. No timezone conversion is
   applied, so the RTC must already be set to the intended literal time.
3. **Five-minute stability:** watch for advancing seconds with no corrupted
   characters, flickering, resets, RTC errors or lost ACKs. Send `status` and
   report frame_us. Exact timestamps may occasionally skip a displayed second
   because this is sampled output; repeated frozen readings need investigation.
4. **EN reset:** note time and press EN without disconnecting USB. Expect both
   devices to initialize and display the RTC's continuing time. No time reset
   or automatic set operation should occur.
5. **Backup:** close the monitor, note RTC/reference time, unplug USB for a
   few minutes with the CR2032 fitted and wiring untouched. Reconnect USB and
   reopen the monitor. Date/time should have advanced through the unpowered
   interval and OSF should remain 0. Do not send any set command.

Report results by test number, along with frame_us and any error lines/photo.
Do not deliberately invalidate an already-correct RTC or disconnect live wires
for fault testing. Failure-message logic is not a substitute for measured
combined-bus behavior. Buzzer/display integration with medication rules and the
deferred scheduler checks remain separate work.

## Desktop decoder checks

```powershell
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PWD '.tools\zig-cache'
& '.\.tools\venv\Lib\site-packages\ziglang\zig.exe' c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude checks/rtc_oled_calendar_checks.cpp -o build/rtc-oled-calendar-checks.exe
if ($LASTEXITCODE -ne 0) { throw 'Calendar check compilation failed' }
& '.\build\rtc-oled-calendar-checks.exe'
```

## Validation record — 2026-10-23

Before this implementation, the user reported the working OLED pattern,
four-corner/border/text/clear/off/on checks and stable two-minute operation
without clipping/flicker. Earlier blank output became visible after moderate
contrast was introduced. The user also reported shared-bus ACKs at 0x3C and
0x68 and two-minute OLED stability with RTC connected. Those checks did not
exercise calendar reads during display updates.

The user subsequently reported all combined RTC/OLED physical checks passed,
as recorded below. No physical behavior is inferred from compilation alone.

Executed checks: rtc_oled_test compiled/linked successfully (22,040 bytes
static RAM, 303,521 bytes flash; display framebuffer allocated at runtime).
New desktop calendar assertions passed with C++17 and strict warnings. Hashes
confirmed all 16 previously recorded source/header/test files unchanged.
Existing medication behavior tests were not rerun for this isolated addition.
Sandbox library setup and host compilation failed; approved outside-sandbox
retries completed successfully. No upload was performed by the agent.

### Combined physical results — user report, 2026-10-23

| Check | Reported result |
| --- | --- |
| Combined firmware operation and device detection | PASS; all combined checks reported passed |
| Display/serial/reference time agreement | PASS; displayed time matches |
| Five-minute combined operation | PASS; stable display and advancing time |
| EN reset with USB connected | PASS; continuing correct RTC time, OSF=0 |
| USB-disconnected backup and restart | PASS; correct time retained, OSF=0 |

Evidence is the user's manual observations, not agent-observed hardware or
archived serial traces. No new numeric frame_us was provided for this combined
run; the earlier OLED-only observation was 97,471 us. No precision calibration
or medication-loop latency validation is claimed. Deferred scheduler checks
remain pending. Recording results changed documentation only.
