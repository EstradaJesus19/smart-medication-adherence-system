# Standalone buzzer and silence-button bench test

Environment buzzer_silence_test; source src/buzzer_silence_test.cpp; portable
bench timing/button controller lib/SilenceBench/src/SilenceBench.h. This is NOT
medication integration: no dose outcomes, refill actions, schedules, RTC reads
or OLED updates. The existing source/test files are preserved. Four external
LEDs are off; a previous OLED image may remain frozen.

## Wiring — disconnect USB before changing anything

Keep the successfully tested buzzer circuit unchanged:
- GPIO14 through its own 220 Ohm resistor to PN2222A base.
- **1 kOhm base-to-emitter pulldown**, not the old 10 kOhm value that chirped.
- Emitter to common GND; collector to buzzer negative.
- Verified 3.3 V through a separate 220 Ohm resistor to buzzer positive.
- Protection diode across buzzer: banded end at buzzer positive (after the
  supply resistor), other end at collector/buzzer negative.

Add a separate momentary silence button:

```text
3.3 V --- 10 kOhm ---+--- VP / GPIO36 signal
                    |
                push button
                    |
                   GND
```

GPIO36/VP is input-only and has no internal pull-up; external 10 kOhm is
required. Use the expansion-board S signal column. Keep its rail selector at
3.3 V. Leave the existing refill D4 button intact; do not share the switch's
signal connection between D4 and VP. Identify two button contacts open when
released and connected only when pressed; four-legged buttons contain connected
pairs, so do not infer them from appearance alone.

Before upload, measure VP-to-GND in DC voltage mode: approximately 3.3 V
released and approximately 0 V pressed. Stop/report unexpected readings before
proceeding. Only use continuity/resistance on an isolated unpowered button.
Power off before correcting wiring. No new power source or buzzer-current
increase is proposed. Keep the supply resistor, diode and final 1 kOhm pulldown.

## Upload

Close other monitors. From project-root PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with current CP210x port:
& '.\.tools\venv\Scripts\pio.exe' run -e buzzer_silence_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e buzzer_silence_test --port COMx --echo
```

115200 baud. Type command/Enter; Ctrl+C exits. Press EN if startup output was
missed. Opening the monitor may reset ESP32. Startup must remain silent.

## Commands and bench policy

| Command | Behavior |
| --- | --- |
| `alert` | Start 250 ms on / 750 ms off pattern, maximum ten seconds |
| `stop` | Cancel alert, GPIO14 LOW |
| `status` | Alert state, accepted button state and requested output |
| `help` | Command summary |

Button debounce is nonblocking 30 ms. A new accepted press cancels the entire
bench alert, including during a quiet gap; it does not merely skip one beep.
No automatic resume. An idle press is reported but not remembered. Alert
requests are rejected while a previous alert is active, before button stable
initialization, or while the button is pressed/releasing. Held at startup
produces no press action: release fully, then explicitly start an alert.
Holding/repeating a command cannot extend the active alert's original deadline.
After release, a new `alert` command is a fresh bench alert, not dose recovery.

The ten-second cutoff and waveform are a bounded bench policy for testing,
not the final medication-reminder policy. Timing is software-driven, not an
independent hardware cutoff. Serial says what output was requested; it does not
measure actual sound or button-to-sound latency. No GPIO interrupt is needed.

## Consecutive physical checks

1. **Electrical/startup:** check VP voltages; start with button released.
   Expect SILENCE_READY, silent output, status idle.
2. **Physical silence:** send `alert`. Hear short repeating beeps. Press the
   new button: ALERT_SILENCED and silence, with no restart after release.
3. **Quiet-gap silence:** start another alert; press during a quiet gap. No
   next beep should occur. This checks cancellation rather than momentary mute.
4. **Idle press:** press/release while idle: SILENCE_PRESS_IDLE. Send `alert`:
   it should still sound. Silence it normally.
5. **Hold/restart:** hold the button after silencing; `alert` is rejected.
   Holding produces no repeated action. Release fully and send a fresh `alert`:
   it sounds. Send `stop`: immediate off request.
6. **Timed limit:** release button, send `alert` and do nothing. At ten seconds,
   expect ALERT_TIMEOUT and silence. Sending another `alert` while it is active
   is rejected and must not extend this original limit.
7. **Held startup:** hold button and EN. Silent startup, SILENCE_INITIAL_HELD.
   Alert request rejected while held. Release, send `alert`: normal operation.
8. **Repeat/reset:** repeat several start/silence cycles; test EN and USB
   reconnect with released button: always silent until explicit alert command.

Report results by number. Disconnect USB if output sticks on, the board resets,
or a component gets hot. No live wiring changes. This test does not implement
silence for a medication session; the later reviewed integration must ensure
silencing leaves pending doses, verification, history and deadlines unchanged.

## Desktop checks

```powershell
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PWD '.tools\zig-cache'
& '.\.tools\venv\Lib\site-packages\ziglang\zig.exe' c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Ilib/SilenceBench/src checks/silence_bench_checks.cpp -o build/silence-bench-checks.exe
if ($LASTEXITCODE -ne 0) { throw 'Silence check compilation failed' }
& '.\build\silence-bench-checks.exe'
```

## Validation — 2026-10-24

Prior protected buzzer commanded-beep and silent-reset tests passed as reported
with the 1 kOhm base pulldown. Combined silence checks subsequently passed as
reported below. No physical sound/latency is inferred from build
success or desktop assertions. Existing tests and simulators are unchanged.

Executed checks: new portable controller assertions PASS (startup-held input,
debounce/bounce, silence/no replay, quiet interval, duty timing, ten-second
timeout, stop and millis rollover), compiled with strict C++17 warnings.
PlatformIO buzzer_silence_test compile/link PASS: 21,528 bytes static RAM,
271,125 bytes flash. Hashes verified all 25 previously recorded source/header/
test files unchanged. Existing automated behavior tests were not rerun for this
standalone adapter. Sandbox compiler failures were resolved by approved outside-
sandbox retries. No upload or acoustic verification was performed by the agent.

### Completed standalone physical stage — user report, 2026-10-24

The user reported the first sequence worked: startup ready/silent, repeating
alert, physical silence, explicit fresh alert, serial stop, and automatic
ten-second timeout. The user then reported all remaining checks passed:
quiet-gap cancellation with no subsequent beep; idle press not remembered;
held-button restart rejection followed by successful restart after release;
held-at-EN startup ignored until release; repeated start/silence cycles and
silent EN/USB startup. All planned standalone silence checks are therefore
PASS as user-observed bench behavior. No separate numeric VP voltage readings
were supplied and no measured button-response latency is claimed.

Medication-session silence behavior remains unimplemented/unvalidated by this
standalone test. Existing medication outcomes, deadlines, refill and deferred
scheduler tests remain separate. Only documentation changed when recording
these results; no new compilation or hardware test was performed for this entry.
