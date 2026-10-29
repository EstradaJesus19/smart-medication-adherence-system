# Scheduled medication bench with incremental OLED clock

Separate environment `scheduled_oled_test`: retains four lids/LEDs, RTC daily
schedule, D4 refill and VP session silence from the validated scheduled_alert_test.
OLED shows only the literal RTC date/time and RTC trust status. Medication state
and outcomes remain on serial and the pending-dose LEDs. No timezone/DST or
persistent schedule/history is added. Boot remains silent and DISARMED.

## Connections

No changes if the previously tested OLED and RTC are already on the bus.
If the OLED was removed, disconnect USB before reconnecting it:

| OLED label | Connection |
| --- | --- |
| GND | Common GND |
| VCC | Verified 3.3 V rail |
| SCL | GPIO22 / D22, shared with RTC SCL |
| SDA | GPIO21 / D21, shared with RTC SDA |

Keep the expansion rail at 3.3 V. The tested provisional SSD1306 128x64 panel
uses address 0x3C and contrast 0x7F; RTC is 0x68. Retain RTC battery/disabled
charging path and every tested reed, LED, button and protected buzzer connection.
See scheduled_alert_test.md for the retained wiring, especially the 1 kOhm
base pulldown and both separate 220 Ohm buzzer circuit resistors.
No extra pull-ups are added at this stage. If wiring changed, verify powered
VCC/SDA/SCL near 3.3 V relative to GND before relying on communication.

## Transfer behavior and limits

A 1024-byte image is drawn in RAM only when the validated RTC second or health
changes. The framebuffer stays frozen during transfer. Each loop performs at
most one OLED transaction, with at most 16 payload bytes. Normal data transfer
at 100 kHz is roughly 1.6 ms per chunk, rather than a single ~97 ms flush.
Inputs/core timers and RTC polling run between chunks. Startup/retry commands
are also split, and the panel is enabled after its first full image is sent.
Updates are incremental, not atomic: briefly mixed old/new pixels may be visible.
The displayed timestamp trails the RTC snapshot by transfer/polling time; it
is not the source of dose timing.

The clock uses the scheduler's checked RTC snapshot, not another RTC read or
an advancing software clock. Failed/untrusted/stale readings replace time with
RTC UNTRUSTED on the next completed frame. Clock recovery does not auto-arm.
A stopped-but-plausible RTC can still look plausible until scheduler drift
checks detect it; this is not an independent clock accuracy monitor.

Any failed OLED transaction stops display work; serial prints OLED_FAULT and
the retained image can be stale or partial. No retry storm. `oled retry` is
accepted only while DISARMED and in closed IDLE. Restarting the display does
not arm, change the RTC, or create a dose. The display and RTC share an I2C bus:
a physically stuck line can affect both, causing scheduler disarm. Software
isolation of an OLED NACK does not establish electrical fault isolation.

Wire timeout remains 20 ms per operation. Serial output, RTC transactions and
rendering also take time: this is cooperative bounded display work, not a hard
real-time guarantee. `status` reports max_oled_tx_us, frame count and
max_loop_gap_ms since startup (including serial work). ACK/metrics do not prove
correct rendering or measured physical response latency.

## Upload

Close the monitor first. From the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\pio.exe' device list
& '.\.tools\venv\Scripts\pio.exe' run -e scheduled_oled_test -t upload
& '.\.tools\venv\Scripts\pio.exe' device monitor -e scheduled_oled_test --echo
```

If auto-detection fails, append `--upload-port COMx` to upload and `--port COMx`
to monitor, using the actual port. Serial 115200; commands end with Enter.
Ctrl+C closes the monitor. This guide uses the project executable because `pio`
is not on this user's PATH.

## Schedule preparation

Close all lids and reach IDLE. Send `disarm`, `schedule clear`, then `clock`.
Check actual RTC time. Add a time at least one minute ahead and arm:

```text
schedule add HH:MM:SS 1 3
schedule show
arm
```

Replace HH:MM:SS; different entries require at least 61 seconds separation.
Use the same preparation for each new reminder. RAM configuration is lost on
reset. All original scheduling/refill commands remain available; no manual dose
command. `set YYYY-MM-DD HH:MM:SS` still requires DISARMED and closed IDLE.

## Physical acceptance checks (user-reported PASS)

1. **Clock/startup:** reset, then USB reconnect separately: no chirp, LEDs off,
   empty DISARMED schedule. OLED date/time matches `clock`, digits fit, contrast
   readable, advances stably for two minutes. `status`: no OLED_FAULT, frame count
   increases. Capture max_oled_tx_us/max_loop_gap_ms for the validation record.
2. **Reminder/silence:** schedule 1+3. Both LEDs and sound start while clock
   continues. VP silences only sound; LEDs/outcomes remain pending. Open 1 over
   2 seconds and close, then 3: each confirms and IDLE returns. Clock keeps running
   throughout; no duplicate lid/button events. Check a brief opening first if
   practical: BRIEF_OPENING, dose stays pending, fresh long opening confirms.
3. **Other compartments/automatic stop:** new session 2+4 sounds again. Confirm
   both without VP, leaving both open: sound stops after final confirmation,
   WAITING_FOR_CLOSURE, LEDs off; closing returns to IDLE. Clock remains live.
4. **Timeout:** new 1+3 session, confirm/close only 1 and leave 3 untouched.
   Sound continues until original 60-second deadline; only 3 becomes MISSED,
   LEDs/sound off and IDLE. OLED clock never pauses for the session.
5. **Refill/buttons:** enter refill with D4 and open due lids before a reminder.
   Reminder interrupts refill and sounds; close/reopen lids for confirmation.
   D4 cannot silence; VP cannot enter refill. Complete within 60 seconds, close
   all, then D4 starts/finishes refill normally. Clock stays live in every state.
6. **RTC unavailable injection:** during new audible 1+3 reminder send
   `test rtc-unavailable`. Expect scheduler DISARMED, OLED RTC UNTRUSTED with no
   old timestamp after its frame completes. Sound and original dose deadline
   continue. VP can mute; confirm/close 1 and let 3 miss at original deadline.
   Send `test rtc-restore`: actual RTC time returns to OLED, scheduler stays
   DISARMED. These commands simulate failed reads; do not unplug live wires.
   They neither stop nor change the actual RTC. Re-arm only explicitly.
7. **OLED-only failure injection:** during a new reminder send `test oled-fail`.
   Expect OLED_FAULT; image may freeze/partially update. `status` should show
   scheduler still ARMED and RTC healthy; VP/lids/LEDs/deadline still work.
   `oled retry` during reminder must be rejected. Finish/close, then `disarm`
   and `oled retry`: live clock resumes. Restore any RTC read injection first.
   This simulates a failed OLED transaction, not a physically stuck bus.
8. **Reset after integration:** configure/arm a future reminder, then EN.
   RTC retains time, OLED resumes, schedule/history clear, scheduler DISARMED,
   LEDs/sound off. No automatic replay. Repeat USB disconnect/reconnect;
   backup timekeeping should retain correct RTC time and OSF=0.

All eight checks below were reported passed by the user; see the physical
validation record below. Access confirmation does not prove ingestion.

## Validation

New host checks cover calendar conversion including leap day/year limits,
snapshot freshness/rollover, exact 1024-byte ordering, one <=16-byte transaction
per service call, panel enable only after initial frame, all 68 initial transfer
failure positions stopping further writes, explicit restart, and medication/
scheduler/audio progress while display transfers fail. Automated checks do not
measure real I2C timing or verify the display controller visually.
2026-10-24 execution results:
- Final ESP32 scheduled_oled_test build: PASS; RAM 23,240 bytes, flash 304,657 bytes.
- New clock-display checks compiled with C++17 and warnings treated as errors:
  PASS, including successful full restart after every injected failure position.
- Existing scheduler and session-audio regression executables rerun: PASS.
- All 31 pre-existing source/include/library/test/check files verified unchanged
  against pre-edit hashes. Only a new PlatformIO environment was appended;
  existing environment configuration was retained.
- Pinned Adafruit GFX 1.13.0 and BusIO 1.17.4 resolved for the new environment.
  Initial sandbox compiler/network attempts failed; approved outside-sandbox
  runs passed.
- No upload or combined physical OLED/medication test performed by the agent.
  User-reported physical results are recorded separately below.


## User-reported physical validation — 2026-10-24

All eight numbered acceptance checks passed on scheduled_oled_test. The user
reported clock/startup measurements first, then reminder/silence/brief-opening
success, followed by successful completion of all remaining checks:

1. Stable RTC clock/startup: PASS. Reported max_oled_tx_us = 1731 (1.731 ms),
   max_loop_gap_ms = 12. These are firmware-reported maxima for the observed run,
   not independently measured physical input latency or worst-case guarantees.
2. Reminder, VP silence and brief/long lid openings: PASS.
3. Compartments 2/4 and automatic sound stop at final confirmation: PASS.
4. Partial confirmation and original-deadline timeout: PASS.
5. Refill interruption and separate D4/VP behavior with clock live: PASS.
6. Injected RTC-unavailable behavior and explicit restore without auto-arm: PASS.
7. Injected OLED failure, continued controls and restricted retry/recovery: PASS.
8. Reset/USB power cycle, RTC retention, cleared configuration/history and no
   automatic replay: PASS.

The combined OLED/medication/audio prototype is validated for these bench
acceptance checks. Results are user-reported, not observations made by the agent.
The injected fault checks do not establish physical stuck-bus isolation. Optional
physical midnight rollover remains deferred as previously recorded. Schedule,
armed state and medication history remain volatile; restart recovery is not
implemented. No firmware or wiring changes accompanied this results update.
