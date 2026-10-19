# Requirements baseline — revision 0.2

Revised 2026-09-22 after the step-by-step design review. “Implemented” means core
logic plus serial simulation, not physical integration. Historical revision 0.1
is preserved in reference/bootstrap-v1.zip.

## Functional requirements

| ID | Requirement / acceptance | Status |
| --- | --- | --- |
| FR-01 | Configure daily entries; group compartments at the same time and reject overlapping windows at different times | Planned scheduler; no automatic entries in current build |
| FR-02 | Start each calendar occurrence once; validate time and late-occurrence handling | Planned RTC scheduler; manual start-now injection implemented |
| FR-03 | Indicate every pending compartment and keep closure requests visible until resolved | Console events/status implemented; LEDs/OLED/buzzer planned |
| FR-04 | Track four stable lid levels; a closed-to-open transition is a new opening, unchanged levels are not | Implemented for simulated inputs; debounce planned |
| FR-05 | Independently confirm each pending occurrence after continuous minimum opening time; brief openings restart only that timer | Implemented |
| FR-06 | Warn for wrong openings without changing pending doses or deadlines | Implemented |
| FR-07 | Mark each remaining pending occurrence missed at the shared deadline; timeout wins ties | Implemented |
| FR-08 | Warn for a new opening after that compartment's latest dose was confirmed; prioritize this over a wrong-opening warning | Implemented; volatile history |
| FR-09 | Emit parseable event records with timestamp, compartment and occurrence context | Implemented with artificial time, serial only |
| FR-10 | Simulate sessions, openings, closures, time and refill using bounded commands; reject malformed commands atomically | Implemented |
| FR-11 | Request closure after unsolicited access; pre-opened targets require close/reopen; finish interactions only when every lid is closed | Implemented |
| FR-12 | Invalid RTC time inhibits automatic scheduling with a visible fault | Planned physical integration |
| FR-13 | A silence button affects sound only, never dose outcomes/deadlines | Planned physical integration |
| FR-14 | Explicit refill start/finish; suppress access warnings; retain history; due session interrupts refill | Implemented |
| FR-15 | Inspect actual lids at startup; request closure without generating artificial access | Core accepts initial levels; hardware reading planned |
| FR-16 | Support 1–4 compartments in one session, any order or simultaneously, with independent outcomes and a shared deadline | Implemented |

## Non-functional requirements

| ID | Requirement | Acceptance |
| --- | --- | --- |
| NFR-01 | Understandable portable C++ with fixed storage and no explicit application heap allocation | Core compiles without Arduino; plain functions and structs |
| NFR-02 | Physical stable-edge response target <=100 ms; nonblocking physical loop | Hardware measurements pending; simulator advance is synchronous |
| NFR-03 | One reminder session at a time; no silent replacement | Reject conflicting sessions |
| NFR-04 | Numbered compartments, readable text, information not dependent only on color | Console numbers present; physical UI review pending |
| NFR-05 | Distinguish simulation from measured hardware data; retain run metadata | CSV clock source and data capture protocol |
| NFR-06 | Local reminder operation independent of an app/network | Core has no network dependencies; physical test pending |
| NFR-07 | Traceable acceptance tests | test_plan.md maps implemented and future requirements |
| NFR-08 | Reproducible builds and automated behavior tests | PlatformIO config, test suite, CI workflow and validation record |

## Planned storage and optional extensions

| ID | Feature | Scope |
| --- | --- | --- |
| FR-20 | Save small recovery/configuration records internally | Planned; requires occurrence IDs, write validation and recovery policy |
| FR-21 | Optional app/caregiver connectivity | Later; call existing operations without controlling core timing remotely |
| FR-22 | Optional weight-change verification | Later experiment; no inference of ingestion |
| FR-23 | Optional dashboard for history/configuration | Later |
| FR-24 | microSD event logging | Planned next storage stage; define missing-card, full-card and interrupted-write behavior |
| NFR-20 | Improved enclosure and mounting | Later |
| NFR-21 | Custom PCB | Later, after breadboard validation |

No pill counting, dispensing, automatic locking or clinical interpretation.
Verification interval and reminder window are experimental settings, not
medication advice. Millisecond counters cannot reconstruct calendar deadlines
after restart. The eventual recovery behavior must distinguish unobserved
power-off intervals from an observed missed interaction.
