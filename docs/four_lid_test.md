# Four physical lids and four pending-dose LEDs

Separate environment: `four_lid_test`; source: `src/four_lid_test.cpp`.
Existing hardware tests, simulators and medication core are unchanged.
No RTC, automatic scheduling or persistent storage is included.
Access confirmation means lid access, not pill removal or ingestion.

## Wiring

Disconnect USB and all other power before wiring. Use USB-C on the ESP32
as the only power source for this test. The user measured approximately
3.3 V between expansion-board V and GND after moving its jumper to 3.3 V.
Keep that jumper position; do not connect other power inputs.

| Compartment | Reed signal | Ordinary red LED output |
| --- | --- | --- |
| 1 | D25 (GPIO25) | D26 (GPIO26) |
| 2 | D34 (GPIO34) | D27 (GPIO27) |
| 3 | D35 (GPIO35) | D32 (GPIO32) |
| 4 | VN (GPIO39) | D33 (GPIO33) |

For EACH reed, use a separate 10 kOhm resistor:

```text
3.3 V --- 10 kOhm ---+--- assigned reed signal
                    |
                   reed
                    |
                   GND
```

For EACH ordinary red LED, use a separate 220 Ohm resistor:

```text
assigned LED output --- 220 Ohm --- LED anode
                                  LED cathode --- GND
```

Total: four reeds, four pull-ups, four ordinary red LEDs and four LED resistors.
All grounds are common. Use the verified 3.3 V rail; never pull inputs to 5 V.
GPIO34, 35 and 39 have no internal pull-ups. Do not substitute RGB LEDs.
On the expansion board follow printed S/V/G labels: the column order is
mirrored between banks. The GPIO connects to S, not V. Avoid stressing glass
seals. Label each reed, magnet and LED 1–4. Place magnets so moving one does
not unintentionally operate another reed. No other modules need connecting.

## Firmware behavior

Each reed has independent 30 ms nonblocking debounce. Initialization waits
until all four readings are simultaneously stable; all LEDs remain off until
initialization. Initially open lids require closure before IDLE.
Each accepted transition is passed to its compartment in the existing core.
Real uptime drives the 2000 ms verification and shared 60000 ms deadline.
Each LED is ON only for its own pending dose, including while verifying.
Serial provides warnings and outcomes; LEDs do not display warning types.

Commands (Enter terminates each line):
- `dose 1 3`: one session with compartments 1 and 3; any distinct subset of 1–4.
- `dose 1 2 3 4`: one session for all four.
- `status`: global state and each lid, verification, outcome and session flag.
- `refill start`, `refill finish`, `help`.

Duplicate numbers, numbers outside 1–4, extra arguments and overlong lines
are rejected without partially starting a dose. There are no simulated
open/close/advance commands. An active session rejects another dose command.
scheduled_at_s=0 means calendar time unavailable. Reset clears history.

## Upload and monitor

Close other serial monitors. From the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with the current CP210x port, not a remembered port:
& '.\.tools\venv\Scripts\pio.exe' run -e four_lid_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e four_lid_test --port COMx --echo
```

Baud is 115200; type commands in the monitor and press Enter. Ctrl+C exits.
Opening the monitor may reset the board. Press EN if startup was missed.
Wait for four initial-lid messages before testing. Start with all magnets near.

## Physical test sequence

Close all lids between cases. Reset where indicated to remove earlier history.
Complete access checks within the 60-second session window. Leave timing
margins rather than targeting exact millisecond boundaries manually.

1. **Mapping:** no dose; move each magnet separately. Only its matching lid
   number should change in serial/status. Access warnings here are expected;
   all LEDs stay off. Close afterward. Repeat steady holds and ten cycles
   per reed; check neighboring magnets do not switch other inputs.
2. **Each indicator:** for each N, send `dose N`; only LED N lights. Open N
   over 2 seconds; only its access confirms and LED goes off. Close: IDLE.
3. **Any order:** `dose 1 3`; only LEDs 1 and 3 light. Confirm 3 first and
   close it: LED 1 stays on and session remains REMINDER. Confirm 1 and close:
   IDLE. Repeat in reverse order.
4. **Overlapping openings:** `dose 1 2 3 4`; open all four within about a
   second and hold. Each must confirm after its own 2 seconds; serial lines
   need not share timestamps. LEDs go off independently. Close only three:
   still WAITING_FOR_CLOSURE. Close the fourth: IDLE.
5. **Independent brief opening:** `dose 1 3`; open 1 and keep it open.
   Briefly open/close 3 for about half a second while 1 verifies. Expect
   BRIEF_OPENING for 3; 1 still confirms without restarting. LED 3 remains on.
   Reopen 3 for over 2 seconds; it confirms. Close both: IDLE.
6. **Wrong compartment:** reset closed, `dose 1 3`; open 2. Expect
   WRONG_COMPARTMENT and CLOSE_LID for 2; pending LEDs/timers remain intact.
   Confirm 1 and 3 but keep 2 open: WAITING_FOR_CLOSURE until 2 also closes.
7. **Repeated access priority:** confirm a dose for 2, close it, then start
   `dose 1 3`. Reopen 2: REPEATED_ACCESS (not WRONG_COMPARTMENT). Finish/close
   all lids. A fresh `dose 2` can subsequently confirm normally.
8. **Partial timeout:** `dose 1 3`; confirm 1 and close. Keep 3 closed past
   60 seconds: only 3 becomes MISSED; 1 remains ACCESS_CONFIRMED; all LEDs off,
   IDLE. Repeat with 3 opened near 59 seconds: if accepted less than 2 seconds
   before deadline, 3 becomes MISSED while open; WAITING_FOR_CLOSURE until closed.
9. **Already open:** open 1 and 3, send `dose 1 3`; hold over 3 seconds:
   both pending, neither verifying. Close/reopen only 1 and confirm it;
   3 must still need its own close/reopen before confirmation.
10. **Refill:** closed IDLE, `refill start`; open several lids: no access
    warnings. `refill finish` stays REFILL while any lid is open. Close all
    and explicitly finish. Repeat refill with lids open, then `dose 1 3`:
    REFILL_INTERRUPTED; due lids already open must close/reopen to confirm.
11. **Input/session rejection:** reset closed; `dose 1 1`, `dose 1 5`, and
    `dose 1 2 3 4 1` must leave IDLE and LEDs off. Then `dose 1 3`; resend
    about 30 seconds later: SESSION_CONFLICT and original deadline unchanged.
12. **Startup:** reset with multiple lids open: outcome NONE and LEDs off;
    WAITING_FOR_CLOSURE until every open lid is closed.

## Validation record — 2026-10-18

User reports all four reeds passed individual tests. Board photos showed the
listed header labels; the expansion V rail was measured about 3.3 V by the user.
The user subsequently reported successful four_lid_test upload, correct
four-input mapping, matching individual pending-dose LEDs and successful
individual access confirmation/closure. Both completion orders for dose 1 3
also passed as reported.

PlatformIO compilation/linking passed for four_lid_test: 21,672 bytes static
RAM, 273,329 bytes flash. The initial sandbox attempt could not execute the
compiler; the approved outside-sandbox retry succeeded. Existing automated
behavior tests were not rerun. No upload or physical test was performed by
the agent for this implementation.

### Completed physical acceptance checks — user report, 2026-10-18

The user reported all 14 checks in the chat's consecutive test sequence passed
with the expected serial messages and LED behavior. This table uses that
sequence's numbering, which differs from the grouped guide above.

| Test | Result |
| --- | --- |
| 1. Four overlapping verifications | PASS |
| 2. Closure of every lid before IDLE | PASS |
| 3. Brief opening independent of another lid's verification | PASS |
| 4. Wrong-compartment warning, preserved session and closure requirement | PASS |
| 5. Repeated-access warning priority | PASS |
| 6. Partial timeout affects only pending doses | PASS |
| 7. Pending dose times out while open; closure required | PASS |
| 8. Two initially open targets each require close/reopen | PASS |
| 9. Multi-lid refill, explicit finish and preserved history | PASS |
| 10. Dose interrupts refill; already-open targets require close/reopen | PASS |
| 11. Invalid dose commands rejected without partial session | PASS |
| 12. Session conflict preserves original deadline | PASS |
| 13. Startup with multiple lids open | PASS |
| 14. Steady holds, repeated cycles and neighboring-magnet check | PASS |

Evidence is user-observed manual behavior, not agent-observed hardware,
archived serial traces or newly executed automated tests. Test 14's mounting
configuration was not specified; repeat magnetic-interference checks with the
final fixture/enclosure. These results validate this bench stage only, not RTC
scheduling, restart recovery or other peripherals. No wiring or firmware was
changed when recording these results.
