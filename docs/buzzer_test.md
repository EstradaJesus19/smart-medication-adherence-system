# Protected, current-limited active buzzer test — 2026-10-24

Separate environment buzzer_test, source src/buzzer_test.cpp. This experiment
switches a DC-operated buzzer through a PN2222A using GPIO14. It does not run
medication logic, buttons, OLED or RTC. Existing firmware/tests are unchanged.

## Evidence and unknowns

Unmarked KEPO buzzer, two wires; seller states 3–24 V operation. Exact model,
construction, full-voltage current and rated sound level are unverified. User
reported clear sound through 220 Ohm from 3.3 V. Corrected measurements:
2.052 V across resistor and 1.29 V across buzzer, total 3.342 V. Calculated
current approximately 9.3 mA applies to that limited test only, not 3.3 V directly
across the buzzer. Continuous sound on DC supports an internal oscillator.

User confirms this PN2222A's previously used pinout: flat face toward observer,
leads downward, E-B-C left-to-right. A low-current dummy-load test also passed:
collector near 3.3 V off and near ground on. Do not generalize its lead order to
other manufacturers or transistor types. User now has the requested diode;
the proposed part is 1N4148. Check actual marking/polarity before fitting.

## Wiring: disconnect USB before assembling

Remove the earlier 10 kOhm collector dummy load and manual base-control feed.
Use TWO separate 220 Ohm resistors and one 1 kOhm resistor (final tested value):

```text
3.3 V --- Rlimit 220 Ohm --- node A --- buzzer (+)
                                      buzzer (-) --- node B --- Q1 collector

D1 1N4148 across buzzer:
    banded cathode ---> node A
    unbanded anode --> node B

D14/GPIO14 --- Rbase 220 Ohm --- Q1 base
                                  |
                              Rpull 1 kOhm
                                  |
GND -------------------------- Q1 emitter
```

Use the verified 3.3 V rail, not 5/12 V. All grounds common. D1 is across the
BUZZER, after the supply resistor: its band goes to node A, NOT collector/GND.
The base pulldown stays fitted to cover startup/reset. GPIO drives the base,
never buzzer positive. Do not leave the previous manual base feed connected.
Do not confuse the supply-limiting resistor with the separate base resistor.

Keep Rlimit fitted during this first transistor test: load current remains
bounded to approximately 15 mA even if the load/diode is shorted. Rbase gives
roughly 10–12 mA total GPIO drive while on, for a brief test; some flows through
the base-emitter pulldown. This is deliberately
ample for the bounded collector load; later bias/current/volume optimization is
separate. The final 1 kOhm pulldown suppresses the observed reset/startup chirp.
The initial 10 kOhm value did not ensure silent startup on GPIO14.
Protection does not establish the buzzer's unrestricted current rating.

Leave the rest of the working wiring unchanged. External LEDs are kept off;
an old OLED image may remain frozen because this build does not refresh it.

## Upload

Close other monitors. From the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with current CP210x port:
& '.\.tools\venv\Scripts\pio.exe' run -e buzzer_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e buzzer_test --port COMx --echo
```

Serial 115200 baud. Click terminal, type command and press Enter. Ctrl+C exits.
The GPIO starts LOW; no automatic startup beep. Commands are `beep` (250 ms),
`stop`, `status`, `help`. No PWM/tone needed for a buzzer producing sound on DC.
Duration uses nonblocking unsigned millis arithmetic. Repeated beep requests
while active are ignored rather than extending the current beep. No continuous
on command. Serial reports requested output only; physical sound is user-verified.
The cutoff is software-controlled, not an independent hardware timeout.

## Physical acceptance

1. Power up/upload: silent. `status`: gpio14_requested=LOW.
2. `beep`: one short sound around a quarter-second, then automatic silence.
   Volume may stay low due to the intentionally retained supply resistor.
3. Repeat five times with a pause between: one beep each, no resets or stuck tone.
4. Send `stop`: remains silent. If sent during a beep, it requests immediate
   cancellation rather than waiting for timeout (manual typing may be too slow
   to observe interruption of a 250 ms beep).
5. EN/reset: silent startup again. Monitor opening may also reset ESP32.

Stop/disconnect USB if sound stays on or the board resets. Recheck transistor
orientation, base pulldown and diode polarity before another attempt. No live
wiring changes. Do not infer full-volume current or adequate final alert volume
from a pass here. Removing/bypassing Rlimit requires a separate current review.
Later integration will add alert policy and a silence button without changing
confirmation outcomes/deadlines. Existing refill D4 is not the silence button.

## Validation

Earlier manual transistor and series-limited sound checks are user reports.
Protected GPIO switching and startup behavior subsequently passed as reported
below. No sound is inferred from successful compilation.
PlatformIO buzzer_test compilation/linking passed: 21,512 bytes static RAM,
270,449 bytes flash. Sandbox compiler execution failed; approved outside-sandbox
retry passed. Hashes confirmed all 24 previously recorded source/header/test
files unchanged. Existing automated behavior tests were not rerun for this
isolated adapter. No upload or acoustic verification was performed by the agent.

### Final bench results — user report, 2026-10-24

The initial protected build produced clear timed beeps with acceptable bench
volume, but EN reset and USB power cycling produced a short unintended chirp.
With the GPIO14 feed disconnected and the pulldown retained, the chirp stopped,
isolating the control path. GPIO14's reset pull-up was the leading explanation,
not a captured waveform measurement. The user replaced the 10 kOhm base-emitter
pulldown with **1 kOhm**, reconnected the GPIO feed and reported sound only on
the beep command. Five separate beep requests and repeated EN/USB startup tests
then passed: short commanded sound, automatic silence, no startup sound.

This validates the protected, resistor-limited bench stage as user-observed
behavior. It does not measure unrestricted buzzer current, certified sound
level, final alert policy or response with the medication loop. The original
firmware help string still names the initial 10 kOhm resistor; the final physical
circuit uses 1 kOhm as specified here. No firmware was changed when recording
these results; update that informational string in a later reviewed change.
