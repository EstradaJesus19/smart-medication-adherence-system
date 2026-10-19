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
entries. The earlier class hierarchy and scheduler are archived. Calendar
scheduling will be introduced separately after this behavior is understood.

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

## Next stages

Add a validated RTC schedule that groups compartments due at the same time and
rejects overlapping windows at different times. Then integrate debounced reeds,
LEDs, buzzer, OLED and explicit controls. Planned storage separates microSD event
logs from a small internal recovery record. An app may later invoke the same
operations; no Wi-Fi, storage, app or restart recovery is implemented yet.

Current history is RAM-only: reboot loses it. The core runs one device and one
session at a time. A new dose overwrites that compartment's latest working record;
past outcomes survive only in captured serial output. Missed doses cannot be
confirmed retroactively. Physical response, usability and recovery need their
own tests.

Course outputs remain an eight-page LaTeX paper, five-minute pitch (about seven
slides), working prototype, 30-second video, code and experimental data.
[License selection](LICENSE) remains open.
