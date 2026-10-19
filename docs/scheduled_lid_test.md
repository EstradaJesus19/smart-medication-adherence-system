# RTC-scheduled four-lid bench test

Environment `scheduled_lid_test`; separate firmware with a fixed-size daily
schedule, four debounced physical lids and independent pending-dose LEDs.
Existing firmware, medication core and tests are unchanged. New portable
scheduler logic is in lib/DailyScheduler; new checks are in checks/scheduler_checks.cpp.
No wiring changes are required from the combined RTC/four-lid setup.

## Hardware

Reeds: GPIO25, 34, 35, 39 (VN), each with its external 10 kOhm pull-up to
3.3 V and reed to common GND. Ordinary red LEDs: GPIO26, 27, 32, 33,
each through its own 220 Ohm resistor. RTC: SDA21, SCL22, verified 3.3 V and
GND. Leave SQW/32K unconnected. Keep the removed RTC charging resistor absent;
the fitted CR2032 is for RTC backup only. Preserve the 3.3 V expansion jumper.
Disconnect USB before any wiring change; no live unplugging for fault tests.

## Upload

Close other monitors; from the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Substitute the current CP210x port for COMx:
& '.\.tools\venv\Scripts\pio.exe' run -e scheduled_lid_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e scheduled_lid_test --port COMx --echo
```

Baud 115200. Type commands and press Enter; Ctrl+C closes the monitor.
Opening the monitor may reset ESP32. Wait for four initial lid readings.
Startup is always DISARMED with an empty schedule. RTC is read, never
automatically set. The existing RTC battery keeps time, not dose history.

## Configure and arm

Commands:

| Command | Behavior |
| --- | --- |
| `clock` | Read RTC health and print date/time and OSF |
| `status` | Scheduler, RTC, state and all four compartment outcomes |
| `schedule add HH:MM:SS N [N ...]` | Add a daily time with a distinct subset of compartments 1–4 |
| `schedule show` | Show grouped schedule and armed/health status |
| `schedule clear` | Remove entries while disarmed |
| `arm` | Read RTC and arm only if healthy, nonempty and currently disarmed |
| `disarm` | Stop new automatic reminders; active session continues |
| `set YYYY-MM-DD HH:MM:SS` | Explicit RTC setting; requires disarmed, initialized closed IDLE |
| `refill start`, `refill finish` | Existing refill rules |
| `test rtc-fail`, `test clock-jump` | Labelled bench injections into scheduler; do not change real RTC or wiring |
| `help` | Command summary |

There is no manual `dose` command in this environment. Use four_lid_test for
manual sessions. This separates manual injections from calendar occurrences.

Choose times at least 30–60 seconds in the future, with time to type and arm.
Example ONLY if `clock` currently reads 14:29:00:

```text
schedule add 14:30:00 1
schedule add 14:30:00 3
schedule add 14:32:00 2 4
schedule show
arm
```

The two 14:30 entries merge into ONE session for 1 and 3. Repeating an already
listed compartment at the same time is idempotent; duplicate compartment
arguments within a single command are rejected. Up to eight distinct daily
times are allowed; malformed/overlapping additions change nothing. Edit only
while disarmed. Clearing/editing does not cancel an active session.

Distinct times must be at least **61 seconds apart**, including across midnight:
60 seconds for the session plus a one-second allowance for polling phase and
RTC second resolution. For example 14:30:30 conflicts with 14:30:00; a time
at 14:31:00 is also rejected by the guard; 14:31:01 can be accepted if it does
not conflict with another entry. 23:59:40 and 00:00:10 conflict too.

All times are literal daily RTC wall-clock values, years 2000–2099. No automatic
timezone/DST changes. RTC date plus entry time identifies an occurrence; serial
scheduled_at_s uses seconds since 2000-01-01, not Unix seconds. Uptime_ms is
still the elapsed timing source. No automatic conversion to the PC timezone.

## Timing, faults and restart rules

- RTC is normally polled every 200 ms; stable lid edges have independent 30 ms
  debounce. Verification stays 2000 ms, deadline stays 60000 ms, using millis().
- Arming excludes entries at or before that RTC second. It never catches up
  an earlier occurrence, even if its nominal window is still open.
- On a healthy time crossing, start only if the entry's due SECOND is observed.
  If that second was skipped, report LATE_SKIPPED and consume it; do not start
  late or infer a dose MISSED during an unobserved interval.
- Consume before trying to start. An unexpected core conflict reports
  AUTOMATIC_START_REJECTED and is never silently retried.
- A RAM watermark prevents replay of consumed occurrences in the same boot,
  including after disarm/rearm, schedule edits or a clock rollback. After a
  rollback, entries up through the watermark remain suppressed.
- I2C failure, OSF=1 or invalid/unsupported date disarms new reminders. Backward
  time movement, drift greater than two seconds against elapsed uptime, or an
  observation gap over two seconds also disarms. Small forward corrections
  within tolerance can still skip a due second; they do not cause catch-up.
- A frozen-but-plausible RTC is detected when its drift exceeds tolerance.
  The time reference is refreshed about every minute to bound counter range.
- An active session keeps its original uptime timers and sensor handling during
  a fault or disarm. Pending LEDs and closure/refill rules remain unchanged.
- Health recovery does NOT re-arm automatically. Check clock/schedule, then arm
  explicitly; future-only eligibility applies again.
- Reset loses schedule, armed state and medication history. Reconfigure/check
  and explicitly arm. No replay or automatic reconstruction of power-off doses.
  A clock backup battery does not implement recovery storage.

## Physical tests

Start with closed lids. Use times based on the current `clock` output, not the
example above. All automatic physical observations are initially pending.

1. **Boot and arming:** verify DISARMED/empty, clock correct/OSF=0. Empty arm
   rejected. Add a time 60 seconds ahead, confirm no activation before arming.
   Arm before its due second; expect AUTOMATIC_DUE, matching DOSE_STARTED and
   LEDs at that second. Open each selected lid over 2 seconds, close: IDLE.
2. **Grouping:** add two entries at one future time, one for 1 and one for 3.
   Show must list a single grouped time. Arm: both LEDs start together.
   Confirm in either order and check independent outcomes/closure.
3. **Overlaps and validation:** disarm/clear; add 14:30:00 1, then try 14:30:30 2
   and 14:31:00 2: rejected, original entry preserved. Try invalid time 25:00:00
   and duplicate arguments `schedule add 14:40:00 1 1`: rejected. Clear and
   test 23:59:40 against 00:00:10 to check midnight rejection. Rebuild a future
   schedule for subsequent tests.
4. **Timeout:** arm a future 1/3 session, confirm/close only 1. At the original
   60-second deadline, only 3 becomes MISSED; all LEDs off, IDLE if closed.
5. **Refill interruption:** arm a future session for 1/3, enter refill and open
   both before due time. At due time expect REFILL_INTERRUPTED; no confirmation
   until each due lid closes/reopens for over 2 seconds. Close all afterward.
6. **No replay:** disarm after finishing a due session, then rearm during the
   same due second if practical, or later. No second session for that occurrence.
   Confirm normal subsequent schedule time still works. Arming after a time has
   passed must not start it retroactively.
7. **RTC-fault injection:** arm a near-future session, let it start, then send
   `test rtc-fail`. Expect labelled injection and RTC_FAULT/DISARMED. Complete
   one lid and let another time out: original uptime rules still work. Healthy
   real polling does not rearm. This tests fault handling, not a real broken bus.
8. **Clock-jump injection:** arm, then `test clock-jump`: labelled +10 second
   sample produces CLOCK_JUMP/DISARMED; `clock` shows actual RTC unchanged.
   No automatic reminders until explicit arm. An active session still finishes.
   This is scheduler input injection, not a physical oscillator test.
9. **Reset:** configure/arm, then EN. Clock continues but schedule is empty and
   disarmed; no reminder until configuration/arming is explicitly repeated.
10. **Midnight/day rollover (optional bench clock):** disarm in closed IDLE;
    set a deliberate test date/time near midnight and add a future midnight
    entry. Arm, observe one session on the new date and restore actual time
    afterward while disarmed/IDLE. Desktop checks also cover the next-day key.

Do not unplug live I2C wires for testing. Actual electrical failure and long-term
drift are separate from the labelled scheduler injections. Late-poll, frozen
clock, backward movement and counter-rollover cases are exercised on desktop.

## Desktop checks

```powershell
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PWD '.tools\zig-cache'
& '.\.tools\venv\Lib\site-packages\ziglang\zig.exe' c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Ilib/DailyScheduler/src -Ilib/MedicationSystem/src checks/scheduler_checks.cpp lib/MedicationSystem/src/MedicationSystem.cpp -o build/scheduler-checks.exe
if ($LASTEXITCODE -ne 0) { throw 'Scheduler compilation failed' }
& '.\build\scheduler-checks.exe'
```

## Validation

Implementation is a bench prototype, with no persistent configuration/history
or clinical interpretation. Build and desktop results are recorded below;
upload and physical scheduling validation require user observations.

2026-10-18 checks executed:
- New standalone scheduler/core integration assertions: PASS, compiled with
  C++17 and -Wall -Wextra -Wpedantic -Werror. Covers grouping, capacity,
  midnight overlap, 61-second guard, future-only arm, late skip, replay
  suppression across rearm/edit, fault recovery requiring arm, clock jumps,
  stalled clock, poll gap, next-day occurrence and millis rollover. Also
  verifies refill interruption and active core confirmation/timeout after a
  scheduler RTC fault.
- Existing unchanged medication/simulator behavior tests: PASS, 26/26.
- ESP32 PlatformIO builds: PASS for scheduled_lid_test, rtc_test,
  four_lid_test, single_lid_test, reed_led_test and esp32dev.
- Final scheduled firmware: 22,120 bytes static RAM; 296,049 bytes flash.
- Initial compiler execution failed inside the sandbox; approved outside-
  sandbox compiler runs passed. No dependencies were downloaded for this work.
- No upload, hardware fault test or automatic physical reminder was performed
  by the agent. Desktop checks do not establish measured sensor/I2C latency.
