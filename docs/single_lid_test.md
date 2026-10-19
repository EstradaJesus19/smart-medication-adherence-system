# Single physical lid integration

Use environment `single_lid_test`. Wiring is unchanged from reed_led_test:
external 10 kOhm pull-up from D25 to 3V3, reed from D25 to GND;
D26 through 220 Ohm to LED anode, cathode to GND. Disconnect USB before
changing wiring. Compartments 2–4 are inactive and logically closed.

The LED now means **pending dose**, not lid open. It stays on during brief
openings and verification, and turns off on confirmation or timeout.
Serial output distinguishes those outcomes. Access confirmation is evidence
of lid access only, not pill removal or ingestion.

The firmware waits for a stable startup reading (30 ms debounce), then uses
real elapsed uptime for the 2000 ms verification and 60000 ms session window.
An initially open lid starts in WAITING_FOR_CLOSURE. Calendar time is unavailable:
scheduled_at_s is zero. Events are serial output, not persistent storage;
reset loses history. Timers start from accepted debounced transitions.

## Upload

Close other serial monitors. From the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with the current Silicon Labs CP210x port:
& '.\.tools\venv\Scripts\pio.exe' run -e single_lid_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e single_lid_test --port COMx
```

Monitor baud is 115200. Send each command with Enter. Use Ctrl+C to close
the monitor. Opening the monitor may reset the board; press EN if startup
output was missed. Wait for the initial lid message before sending commands.

Commands: `dose 1`, `status`, `help`, `refill start`, `refill finish`.
There are no simulated open/close/advance commands. Manual doses may interrupt
refill; overlapping active sessions are rejected by the existing core.

## Physical acceptance tests

1. Start with magnet near (closed), then send `dose 1`: LED on.
   Move magnet away for over 2 seconds: expect VERIFICATION_STARTED,
   ACCESS_CONFIRMED, LED off and WAITING_FOR_CLOSURE. Return magnet:
   expect IDLE. Use `status` to inspect the state.
2. Start another dose while closed. Open for about half a second, close:
   expect BRIEF_OPENING and LED still on. Reopen for over 2 seconds:
   expect confirmation. Close afterward.
3. Open first, then send `dose 1`. Holding open must not confirm.
   Close, then reopen for over 2 seconds to confirm. Close afterward.
4. Start a dose while closed and leave closed for over 60 seconds:
   expect DOSE_MISSED, LED off and IDLE.
5. Confirm a fresh dose, close, then reopen: expect REPEATED_ACCESS.
   Close afterward. A new dose can subsequently be confirmed normally.
6. From closed IDLE, send `refill start`, open: no access warning.
   `refill finish` while open must request closure and remain REFILL.
   Close and send `refill finish` again: IDLE.

Hold positions comfortably beyond the timing thresholds during manual tests.
Compilation and upload do not verify these physical behaviors.

## Validation record

2026-10-18: User reported the standalone reed_led_test passed near/away LED
and serial transitions, ten-cycle repeatability, steady holds, slow movement,
and startup in both positions, without flicker or repeated messages.
Single-lid integration upload and physical results are recorded below.

PlatformIO builds passed for single_lid_test, reed_led_test and esp32dev.
The initial sandbox build could not execute the compiler; an approved retry
outside the sandbox succeeded. Single-lid firmware: 21,648 bytes static RAM,
272,909 bytes flash. Existing automated behavior tests were not rerun.

### Physical results reported by the user — 2026-10-18

These are user-observed manual checks, not automated tests or independently
captured serial traces. All checks below passed as reported in this chat.

| Check | Reported result |
| --- | --- |
| Normal access | dose 1 lit LED; opening over 2 seconds confirmed access and extinguished LED; closing returned IDLE |
| Brief opening | BRIEF_OPENING; LED on and dose pending; subsequent longer opening confirmed; closing returned IDLE |
| Lid already open at dose start | Remained pending until close/reopen; then confirmed normally |
| Closed-lid timeout | DOSE_MISSED, SESSION_FINISHED, IDLE at 60 seconds; LED off; MISSED status |
| Repeated access | Reopening after confirmation produced REPEATED_ACCESS and CLOSE_LID; LED off |
| New occurrence after confirmation | New dose confirmed normally; closing returned IDLE |
| Refill | Suppressed access warnings; finishing required closed lid and explicit command; prior confirmation preserved (REPEATED_ACCESS afterward) |
| Dose interrupts refill | Refill interrupted; already-open lid required close/reopen before confirmation |
| Deadline with lid open | MISSED, LED off, WAITING_FOR_CLOSURE; closing returned IDLE |
| Active-session conflict | Second dose command produced SESSION_CONFLICT; original 60-second deadline unchanged; MISSED and IDLE |
| Startup open | WAITING_FOR_CLOSURE, outcome NONE, LED off; closing returned IDLE |

Scope: one physical reed and one LED with manual dose triggering and uptime.
Four-lid interaction, RTC scheduling, power-loss recovery and integrated
peripherals have not been physically validated by these checks.
