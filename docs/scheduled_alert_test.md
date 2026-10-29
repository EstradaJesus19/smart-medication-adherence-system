# Scheduled reminders with session sound and silence

Separate `scheduled_alert_test` build. Existing firmware and simulators are preserved.
No OLED writes: any old display image is frozen, not a live clock.

## Wiring

Keep the tested wiring unchanged; disconnect USB before inspecting connections.
Reeds: GPIO25/34/35/39; LEDs: GPIO26/27/32/33. RTC SDA21/SCL22 at 3.3 V.
D4/GPIO4 refill and VP/GPIO36 silence each use their own external 10 kOhm
pull-up to 3.3 V and momentary switch to GND.

Retain the tested buzzer driver: 3.3 V through 220 Ohm supply resistor to buzzer+;
buzzer- to PN2222A collector, emitter to GND. GPIO14 through a separate 220 Ohm
resistor to base; **1 kOhm base-to-emitter pulldown**. Protection diode across
buzzer: band to buzzer+ AFTER the supply resistor, other end to collector.
Retain the verified transistor pinout and all current limiting.

## Behavior

Pending session: repeating 250 ms on / 750 ms off sound. A fresh silence press
mutes only this session, including presses in a quiet gap. Confirmation still
requires a fresh opening held for 2 seconds; the deadline remains 60 seconds.
Partial confirmation keeps sound until muted or all pending doses finish.
All confirmed/missed stops sound, even while waiting for lid closure.
The next session clears mute. An idle press or already-held button cannot mute
it: release and press again. D4 retains refill behavior and cannot silence.
RTC faults/disarm do not cancel active dose timing or sound. No extra sound is
assigned to warnings, refill or missed outcomes after session completion.

## Upload

Close the serial monitor before uploading. From the project terminal:

```powershell
pio run -e scheduled_alert_test
pio device list
pio run -e scheduled_alert_test -t upload --upload-port COMx
pio device monitor -e scheduled_alert_test --port COMx --echo
```

Replace COMx with the ESP32 port. If using the workspace tool, replace `pio`
with `.\.tools\venv\Scripts\pio.exe` and set
`$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'`.
Serial is 115200 baud; send commands with Enter.

## Configure each run

Boot is silent and DISARMED; schedule/history are RAM-only. Close all lids.
Send `clock`, confirm correct RTC time and health. If needed, while disarmed and
IDLE, use `set YYYY-MM-DD HH:MM:SS`. Do not set time during an active session.

Choose a time at least one minute ahead of the displayed RTC clock:

```text
schedule clear
schedule add HH:MM:SS 1 3
schedule show
arm
status
```

Use actual future HH:MM:SS, not the placeholder. Entries at different times must
be at least 61 seconds apart. Arm never catches up past occurrences. To prepare
another test, close lids, wait for IDLE, `disarm`, clear/add a new future entry,
then `arm`. Existing schedule and fault commands match scheduled_refill_test.

## Consecutive physical checks

1. Reset and reconnect USB: no chirp, no LED, DISARMED. Check `status`.
2. Schedule 1+3. At due time only LEDs 1+3 light and sound repeats. Press VP;
   expect SESSION_SILENCED, sound off, both outcomes PENDING and LEDs still on.
   Open each for over 2 seconds, then close: confirmation and IDLE normally.
3. New 1+3 reminder must sound again. Confirm only 1 and close it: LED1 off,
   LED3 on, sound continues. Silence during a quiet gap; wait until the original
   60-second deadline: 1 confirmed, 3 missed, sound/LEDs off, IDLE.
4. New reminder: confirm both without silencing, leave lids open. Sound stops
   on final confirmation, WAITING_FOR_CLOSURE; closing returns to IDLE.
5. New reminder: touch nothing. At 60 seconds both MISSED, sound and LEDs off.
6. Press/release VP in IDLE. Hold it before the next scheduled reminder: that
   reminder must still sound. Release/repress to silence. Complete the session.
   Also reset with VP held: silent DISARMED; release before use.
7. Start refill with D4. Let a scheduled reminder interrupt it. Sound starts;
   already-open due lids require closing/reopening. D4 cannot mute or finish the
   session. VP mutes without entering refill. Complete/close, then D4 can start
   and finish refill normally again.
8. New reminder: send `test rtc-fail` while active. Scheduler DISARMED, sound and
   original deadline continue. VP still silences; pending doses miss at deadline.
   Healthy readings must not re-arm. Repeat with `test clock-jump`; actual RTC
   time must remain unchanged. Re-arm explicitly only for a later test.

User-reported results (2026-10-24): all combined physical checks passed.
Checks 2 and 3 were reported individually first; after clarification of refill
check 7, the user reported that all tests went well. This covers startup/reset,
session-only silence, partial confirmation and original-deadline timeout,
automatic sound stop on final confirmation, unmuted timeout, idle/held-button
behavior, refill interruption and separate D4/VP functions, RTC fault and
clock-jump injection. Next-session sound and explicit re-arm behavior passed.
The scheduled buzzer/silence integration is physically validated for this
prototype under the listed bench checks. Results are user-reported, not
measurements or observations performed by the agent.

The previously deferred required scheduler acceptance checks also passed on
this build, user-reported 2026-10-24: empty arm rejection, overlap/invalid-input
rejection including midnight overlap, no replay, no late catch-up with a later
future reminder working, and reset clearing configuration/history while RTC time
continues. See scheduled_lid_test.md for the acceptance record. Optional physical
midnight/day rollover remains deferred. Access confirmation is not evidence of
ingestion.

## Validation

2026-10-24: ESP32 build passed (RAM 22,144 bytes; flash 297,545 bytes).
New host checks passed with warnings treated as errors: current-session mute,
unchanged core outcomes/deadline, partial confirmation, final completion while
open, timeout, next-session sound, refill interruption/rejection, idle/held input,
press begun before due, debounce and unsigned timer rollover.
Existing source/library/include/test/check files were verified byte-for-byte
unchanged against pre-edit hashes. Existing environments retain their configuration;
only a new environment was appended. No board upload or physical combined test
was performed by the agent; user-reported physical results are listed above.
