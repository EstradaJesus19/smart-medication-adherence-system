# Smart Medication Adherence System

A four-compartment instrumented pill organizer for Digital Systems Design at
**Politecnico di Milano**, developed incrementally as a portfolio project.

**Current stage: portable medication logic with separate incremental hardware
experiments.** The same C++ core runs in desktop/ESP32 serial simulators and
four-lid hardware tests. It supports simultaneous dose occurrences, independent
lid verification, wrong/repeated-access warnings, closure checks and refill.
The separate scheduled_lid_test adds explicitly armed daily RTC reminders;
configuration and medication history are still volatile. See each test guide
for build results and user-reported physical validation.

An access confirmation means the expected lid stayed open long enough. It does
not prove pill removal or ingestion. The device does not dispense or count pills.

## Start here

Read these files in order:

1. [MedicationSystem.h](lib/MedicationSystem/src/MedicationSystem.h): the four
   states, per-compartment data and available operations.
2. [MedicationSystem.cpp](lib/MedicationSystem/src/MedicationSystem.cpp): the
   reminder and refill rules, implemented as plain functions.
3. [Simulation.cpp](lib/Simulation/src/Simulation.cpp): terminal commands and
   manual time. You can leave its parsing details until after the core is clear.

The previous bootstrap is preserved in [reference/bootstrap-v1.zip](reference/bootstrap-v1.zip).
It is historical reference material, not active code.

## Build and run

For the first wired sensor experiment, use the separate
[reed switch and LED test](docs/reed_led_test.md) (`reed_led_test` environment).
It reads GPIO25 and drives GPIO26; it does not run the medication application.
The commands below still build the original serial simulator.

For one physical lid connected to the medication logic with real elapsed time,
see [single lid integration](docs/single_lid_test.md) (`single_lid_test`).
Its LED means a pending dose; it does not mirror the lid position.

For four physical reeds and four independent pending-dose LEDs, see the
[four lid wiring and test guide](docs/four_lid_test.md) (`four_lid_test`).

For the separate battery-free DS3231 experiment, see the
[RTC wiring and measurement guide](docs/rtc_test.md) (`rtc_test`).
This environment does not run medication sessions or automatically set the clock.

For explicitly armed daily RTC reminders with four physical lids, see
[scheduled lid integration](docs/scheduled_lid_test.md) (`scheduled_lid_test`).
Its schedule and dose history are RAM-only; it boots disarmed.

For a separate provisional 128x64 SSD1306 OLED experiment, see the
[OLED wiring and measurement guide](docs/oled_test.md) (`oled_test`).
This test does not run medication sessions or write the RTC.

For actual RTC calendar time displayed on the OLED, see the separate
[combined RTC/OLED guide](docs/rtc_oled_test.md) (`rtc_oled_test`).
This read-only bench test does not set time or run medication sessions.

For the separate D4 refill-button input check, see the
[button wiring and test guide](docs/refill_button_test.md) (`refill_button_test`).
It reports presses only and does not change medication/refill state.

For physical button control of refill alongside automatic RTC reminders, see
[scheduled refill-button integration](docs/scheduled_refill_test.md)
(`scheduled_refill_test`). This build uses serial state output, not OLED updates.

For an isolated short-beep transistor-driver check, see the
[protected buzzer guide](docs/buzzer_test.md) (`buzzer_test`).
Keep its supply-limiting resistor and protection diode fitted for this stage.

For a separate repeating bench alert and physical VP/GPIO36 silence button,
see [buzzer/silence test](docs/buzzer_silence_test.md) (`buzzer_silence_test`).
Retain the final tested 1 kOhm buzzer base pulldown; no medication logic runs.

For the separate scheduled reminder, refill and session-silence integration, see
[scheduled audio guide](docs/scheduled_alert_test.md) (`scheduled_alert_test`).
This build does not update the OLED.

For the separate combined medication bench with an incremental OLED clock, see
[scheduled OLED integration](docs/scheduled_oled_test.md) (`scheduled_oled_test`).
It splits display transfers between input/timer updates and retains serial state output.

Open the folder in VS Code with PlatformIO. In its terminal:

```sh
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Confirm the actual ESP32 board before uploading; the provisional target is
`esp32dev`. No peripheral wiring is needed for this simulation.

For desktop use with GCC/MinGW on PATH:

```sh
pio run -e native -t exec
pio test -e native
```

PlatformIO native requires a host compiler; see the
[official native guide](https://docs.platformio.org/en/latest/platforms/native.html).
This workspace also has a locally built Windows simulator:
```powershell
.\build\simulator.exe
```

The executable and local tools are ignored by Git. See [validation](docs/validation.md)
for reproducible local build commands and exactly which checks ran.

## Try two simultaneous doses

Send one command per line:

```text
dose 1 3
open 3
open 1
advance 2000
status
close 1
close 3
```

Both dose occurrences become confirmed at 2000 ms, regardless of opening order.
The system waits for both lids to close before returning to IDLE.

Then try refilling:

```text
refill start
open 2
refill finish
close 2
refill finish
```

The first finish request asks for closure and remains in REFILL. The second succeeds.
A new `dose` command during refill interrupts it and starts a reminder.

| Command | Meaning |
| --- | --- |
| `dose 1 [2 ...]` | Start one session for 1–4 distinct compartment numbers |
| `open N`, `close N` | Change a simulated lid condition |
| `advance MS` | Advance 1–600000 **milliseconds**; intermediate ticks are processed |
| `refill start`, `refill finish` | Explicit refill operations |
| `status` | Show state, all lids, session membership, verification and latest outcomes |
| `help` | List commands |

Time starts at zero and only moves with `advance`, on both targets. The example
window is 60000 ms and verification interval is 2000 ms. Restart/reset for a fresh
run. An unchanged open level is not a new opening.

**Changes from the original bootstrap:** `advance` now takes milliseconds,
`dose` can select multiple compartments, and there are no automatic 08:00/20:00
entries in the simulator. The earlier class hierarchy and scheduler are archived.
RTC calendar scheduling is implemented in the separate `scheduled_*` hardware
environments; the simulator retains its manual clock.

## Layout and documentation

- `lib/MedicationSystem/src/`: hardware-independent core, with fixed storage.
- `lib/Simulation/src/`: shared command parser and CSV output adapter.
- `src/main.cpp` / `src/native_main.cpp`: short Arduino/desktop entry points.
- `test/test_domain/`: automated behavior tests.
- `docs/`: [requirements](docs/requirements.md),
  [architecture](docs/system_architecture.md), [state machine](docs/state_machine.md),
  [test plan](docs/test_plan.md), [BOM](docs/bom.md),
  [development/course plan](docs/development_plan.md),
  [literature worksheet](docs/literature_notes.md), [validation](docs/validation.md).
- `hardware/`, `mechanical/`, `figures/`: planned physical artifacts.
- `data/`: [log schema and capture instructions](data/README.md).
- `reference/`: preserved original source and documentation.

## Current status and next stages

The separate hardware environments implement RTC scheduling with grouped doses
and overlap rejection, debounced lid inputs, LEDs, refill/silence buttons, buzzer
alerts and an incremental OLED clock.

Next, validate storage separately before integrating event logging. Internal
configuration persistence, restart recovery, Wi-Fi and an app remain planned.

The active schedule and medication history are RAM-only: reset clears them and
boots DISARMED. The core runs one device and one session at a time. A new dose
replaces that compartment's latest working record; captured serial output can
preserve past evidence. Missed doses cannot be confirmed retroactively. Physical
response, usability and recovery need their own tests.

Course outputs remain an eight-page LaTeX paper, five-minute pitch (about seven
slides), working prototype, 30-second video, code and experimental data.
[License selection](LICENSE) remains open.
