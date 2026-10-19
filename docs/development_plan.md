# Incremental development plan

The user requested explanation and review before edits. The simplified core and
simulator update was explicitly approved on 2026-09-22. Future feature changes
should again be explained first, then implemented only when requested.

## Current milestone

Understand and exercise reminder sessions, independent verification, warnings,
closure and refill. No automatic daily scheduler, physical driver, persistent
storage or app is included. The earlier bootstrap is archived in reference/.

Suggested next steps, each reviewed separately:

1. Run the serial scenarios and examine per-compartment status.
2. Add a small daily scheduler: group equal times, reject overlapping windows,
   define late polls and use a validated RTC.
3. Integrate one debounced physical lid, then four; seed actual levels at startup.
4. Bind LEDs, buzzer, OLED and explicit controls to the already-tested behavior.
5. Add microSD event history and a small internal recovery/configuration record.
6. Test power interruption and recovery before making persistence claims.
7. Consider enclosure improvements, optional app, and weight verification.

Storage design must not merely save and restore the old global state. Read actual
lids after reboot, validate the clock, reconcile dated occurrences, and restart
interrupted verification rather than assuming continuous observation. App
commands can invoke the existing operations; basic reminders should remain local.

## Course mapping

| Phase | Work and gate |
| --- | --- |
| Definition | Agree requirements and limits; reconcile instructor rubric |
| Related works | Fill verified evidence table in literature_notes.md |
| Methodology | Describe session logic, sensing assumptions and test protocol |
| Implementation | Demonstrate simulation first, then tested hardware |
| Experiments | Save raw output and metadata; report counts, failures and timing |
| Presentation/video | Match claims to implemented features and measurements |

Final deliverables: eight-page LaTeX paper, five-minute pitch (about seven slides),
working prototype, 30-second video, source code and experimental data.

Tentative paper allocation: Introduction 1 page, Related Works 1, Methodology 2,
Results 2, Conclusions 0.5, References 1, layout margin 0.5; adapt to the rubric.
Pitch: problem, prior work, architecture, behavior, demo, results, limitations.
The video can show a reminder, wrong access, correct access and complete closure;
label accelerated simulation time explicitly.

## Suggested commits for this update

No commits are created automatically.

1. `docs: preserve original bootstrap reference` — reference/ archive and note.
2. `refactor: simplify core to multi-compartment sessions and refill` — replace
   Adherence with MedicationSystem; update simulation and entry points.
3. `test: cover session verification closure and refill behavior` — revised tests.
4. `docs: align requirements and examples with simplified simulation` — README,
   requirements, architecture, state machine, test plan, BOM, data and validation.

If the folder is not a Git repository, initialize it before making these commits.
Choose the license before publishing. Original first-stage commit suggestions
remain inside the preserved bootstrap; they describe the historical design.
