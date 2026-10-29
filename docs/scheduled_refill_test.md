# Scheduled lids with physical refill button

Separate environment scheduled_refill_test, source src/scheduled_refill_test.cpp.
It copies the existing scheduled bench adapter and adds a physical button via
lib/RefillButtonControl. Existing firmware, scheduler and medication core are
unchanged. OLED and buzzer are NOT driven by this build; use serial for state.
A previously drawn OLED clock may remain frozen: it is not live in this build.

## Wiring

No changes from the tested setup: D4/GPIO4 with external 10 kOhm pull-up to
3.3 V and momentary button to common GND. Reeds: 25/34/35/39, LEDs: 26/27/32/33.
RTC on SDA21/SCL22, verified 3.3 V and GND. Keep the RTC charging-path resistor
removed and correctly fitted backup cell. OLED may remain connected but is not
updated. Keep GPIO36 reserved for the eventual silence button.
Disconnect USB before any wiring change; none is needed for this test.

## Button rules

| State at an accepted press | Action |
| --- | --- |
| IDLE (all lids closed) | startRefill: enter REFILL |
| REFILL with any lid open | finishRefill rejects completion, requests closure |
| REFILL with all lids closed | finishRefill: return IDLE |
| REMINDER or WAITING_FOR_CLOSURE | startRefill rejected; current session/state preserved |

Independent nonblocking 30 ms button debounce. One action per stable press;
release/repress is necessary for another. Held at startup generates no action
until stable release and a subsequent press. Button sampling continues while
lids initialize, but accepted presses before initialization are discarded rather
than queued. A rejected press is not replayed after a state change.

Each accepted press forces a fresh RTC/scheduler check before applying the
refill action. A due session wins if it coincides with a button action. It still
interrupts refill and pre-opened due lids still need close/reopen. A button held
through that interruption cannot restart refill when the reminder finishes.
Button does not arm/disarm schedules, set time, silence, or confirm access.
Dose outcomes and deadlines remain owned by the unchanged core.

Serial `refill start`/`refill finish` remain available for troubleshooting; avoid
mixing them into physical button acceptance sequences. Scheduler rules remain
those in docs/scheduled_lid_test.md: RAM-only, explicit arm, 61-second separation,
no automatic catch-up/replay, faults disarm new reminders, active timers continue.
RTC backup preserves time, not medication state. Startup is disarmed/empty.

## Upload

Close other serial monitors. From project-root PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with the detected CP210x port:
& '.\.tools\venv\Scripts\pio.exe' run -e scheduled_refill_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e scheduled_refill_test --port COMx --echo
```

115200 baud; Enter sends a command, Ctrl+C exits. EN repeats startup. Wait for
four initial lid messages and REFILL_BUTTON_READY before using the button.
`status` reports all lids, global state, scheduler and accepted button level.

## Consecutive physical tests

1. **Enter and finish:** startup with closed lids/button released. Press/release:
   REFILL_STARTED. Hold another press 10 seconds with lids closed: one
   REFILL_FINISHED/IDLE, no new refill until released/repressed.
2. **Explicit closure:** enter refill, open 1 and 3. No access warnings.
   Press/release: CLOSE_LID requests, still REFILL. Close only 1 and try again:
   still REFILL. Close 3: still REFILL until a new press finishes to IDLE.
3. **Closure-state rejection:** from IDLE, open a lid without refill:
   WAITING_FOR_CLOSURE. Press: REFILL_START_REJECTED, unchanged state. Close
   lid: IDLE; that old press must not automatically start refill.
4. **Held startup:** hold button, EN. Expect REFILL_BUTTON_INITIAL_HELD; no
   REFILL_STARTED. Release: REFILL_BUTTON_RELEASED. A new press enters refill.
   Close lids and press again to finish before continuing.
5. **Automatic interruption:** `clock`; choose a time at least a minute ahead.
   `schedule add HH:MM:SS 1 3`, `schedule show`, `arm` (substitute actual future
   time). Enter refill with button, open 1/3. Due time: REFILL_INTERRUPTED,
   DOSE_STARTED, LEDs on. Press: rejection, original REMINDER unchanged.
   Close/reopen each lid over 2 seconds, close all: IDLE, LEDs off.
6. **Hold through reminder completion:** configure another future session.
   Once REMINDER starts, press and keep button held. Expect one rejection.
   Confirm/close due lids while still held (or let them time out closed).
   On returning to IDLE, hold 5 seconds more: must stay IDLE. Release/repress:
   may now enter REFILL. Finish refill with another press when closed.
7. **History:** after a confirmed dose, enter/finish refill physically. Reopen
   the previously confirmed lid afterward: REPEATED_ACCESS still applies.
8. **Repeatability:** ten press/release cycles while closed should alternate
   REFILL and IDLE once per press; even number ends IDLE. No repeated actions
   on holds. Button debounce is separate from 2000 ms lid verification.

Report results by number, with serial errors if any. OLED clock rendering,
buzzer/silence and remaining deferred scheduler checks are separate work.
No live wire removal is necessary for these tests.

## Desktop checks

```powershell
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PWD '.tools\zig-cache'
& '.\.tools\venv\Lib\site-packages\ziglang\zig.exe' c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Ilib/RefillButtonControl/src -Ilib/MedicationSystem/src -Ilib/DailyScheduler/src checks/refill_button_checks.cpp lib/MedicationSystem/src/MedicationSystem.cpp -o build/refill-button-checks.exe
if ($LASTEXITCODE -ne 0) { throw 'Button integration check compilation failed' }
& '.\build\refill-button-checks.exe'
```

## Validation — 2026-10-24

Standalone button tests passed as reported by the user; see refill_button_test.md.
Combined scheduled_refill_test physical checks subsequently passed as reported
below. Desktop assertions and build results do not establish physical behavior.

Executed: new desktop button/core/scheduler assertions PASS with C++17 and strict
warnings; scheduled_refill_test PlatformIO compile/link PASS (22,128 bytes
static RAM, 296,937 bytes flash). Hash verification confirmed all 21 previously
recorded source/header/test files unchanged. Existing automated behavior tests
were not rerun; the new checks exercise the unchanged core's refill/rejection,
due interruption, retained history and original deadline behavior. Sandbox
compiler failures were resolved by approved outside-sandbox retries. No upload
or physical integration test was performed by the agent.

### Completed physical checks — user reports recorded 2026-10-24

All eight guide checks passed as reported: entry/explicit finish, partial and
complete closure rules, rejection while waiting for closure, held-at-startup
suppression, automatic reminder interruption, rejected hold through reminder
completion without replay, preserved confirmation/repeated-access history, and
ten-cycle repeatability. The user confirmed the last three outstanding checks
(partial closure, closure-state rejection and ten cycles) all passed.
This completes the planned physical refill-button bench validation. Evidence
is manual user observation, not agent-observed hardware or captured serial
traces. OLED/buzzer integration and deferred scheduler checks remain separate.
Only documentation was changed when recording these results.
