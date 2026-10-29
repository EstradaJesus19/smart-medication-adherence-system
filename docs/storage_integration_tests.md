# Storage integration acceptance checklist — proposed 2026-10-24

Current status: standalone `sd_test` and combined `scheduled_sd_test` logging
are implemented. A1-A4 passed according to the user's report. B1 operational
behavior and manual file-content review are user-reported; automated validation
of that physical export and B2-B10 remain pending. NVS/restart recovery (C) is
still proposed and unimplemented.

The A/B/C lists below preserve the original acceptance plan. Use the
[queued SD guide](scheduled_sd_test.md) for current B-stage commands and limits;
references to healthy internal recovery storage describe future requirements.
Keep validated environments unchanged. Record exact build/card/operation timings
and user observations; never equate a simulated failure with physical validation.

## A: standalone acceptance checklist (completed by user report)

A1. Remount guards: unmount; verify 0 and write an unused slot must refuse.
Mount and verify 0 must pass. No card removal while powered.
A2. Repeated writes: write five unused slots one at a time, then verify oldest.
Capture write_us, sync_us, total_us for all writes and final status. Existing
slots refused. These values are sample observations, not worst-case guarantees.
A3. Absent card: unmount/power OFF/remove, power on: mount fails clearly and writes
refuse. Power OFF/reinsert/reboot, verify original file passes. No live hot-swap.
A4. PC readback: unmount/power OFF, inspect records on PC: exact 512-byte size,
correct marker/slot, unrelated files unchanged. Safely eject/reinsert with board
power OFF; original verification passes. Back up data before experiments.

Already passed: detection, known record write/verify, overwrite refusal, EN
readback and clean power-cycle readback. Screenshot mount time 28.564 ms only;
write/sync/total observations still needed.

## B: original combined SD event-logging acceptance plan

B1. Normal grouped session: log due/start, both lid confirmations, closures and
finish. Check compartment/dated occurrence IDs, trusted RTC/uptime fields, boot
ID and sequence order against serial. Queued data has an explicit committed or
uncommitted state; do not claim exactly-once transactions across NVS and SD.
B2. Brief, wrong and repeated openings: expected warnings/events logged; no false
confirmation, deadline extension or incorrect pending LED. Silence logged without
changing dose outcomes. Holding/idle presses do not mute a future session.
B3. Timeouts: test both full timeout and one confirmed/one missed. Correct outcomes
and original 60-second deadline; no historical MISSED inferred from downtime.
B4. Refill: start/refill openings/finish and due reminder interrupting refill logged;
D4 and VP stay independent; pre-opened lids still require fresh opening.
B5. Display/control load: all four lids/LEDs, OLED clock and buzzer stay functional
while logging. Measure real stable-edge/button response against <=100 ms target;
firmware loop/write metrics alone do not establish physical latency.
B6. Missing card at boot: visible LOGGING_FAULT, normal live reminders and controls
continue when internal configuration/recovery storage is healthy. Logging faults
alone must not be confused with NVS corruption inhibiting safe scheduling.
B7. Full card/write/sync failure: explicit error, no false committed-success report,
no infinite retry, no mutation of committed dose outcomes or extension of deadline.
Exercise labelled software injection first, then controlled spare-card tests.
B8. Slow storage/queue saturation: injected delays exceed queue capacity. Controls
and timers keep working; bounded queue reports overflow/lost-record counts and
sequence gaps. Recovery is explicit, with neither duplicate replay nor silent loss.
B9. RTC fault/jump during logging: scheduler disarms, active uptime deadline remains;
logs identify untrusted/injected time and OLED error/recovery correctly. No auto-arm.
B10. Restart/card-file recovery: clean restart preserves completed logs; fresh boot
identity separates uptime sequences. Partial final record is detected and left
untrusted without destroying earlier valid records. Recovery onto a card with
existing files does not overwrite unrelated data. No automatic catch-up inferred
from card contents. Real interrupted-write tests use backed-up/spare media only
in the reviewed recovery stage, not while running the current basic sd_test.

## C: proposed NVS configuration/restart checks (after implementation)

C1. Save/load: valid grouped daily schedule survives reset and USB loss. Boot
remains DISARMED; explicit future-only arm required. Saving only at defined
transitions avoids per-loop/per-second writes. Card absence does not erase settings.
C2. Corrupt/version-mismatched/torn records: reject invalid candidates visibly,
select valid committed generation only, no silent erase/default-and-arm. Test
failed writes/readback plus power interruption during commit; unresolved latest
state must not be presented as certain. NVS failure policy is distinct from SD fault.
C3. Interrupted pending session: reset before opening, during short/long verification
and near deadline. Actual lids resampled; no restored partial opening timer or new
60-second window. Pending outcomes become explicit unresolved interruption evidence
under the proposed policy, not automatic confirmation/MISSED.
C4. Partial/final confirmation: reset after one of two confirms and after all confirm.
Preserve successfully committed outcomes, expose uncertainty around interrupted
commit, request closure for actual open lids and never fabricate ingestion.
C5. Durable replay protection: re-arm, schedule edit and RTC rollback cannot replay
consumed dated occurrences. Unobserved off-time is distinguished from observed
misses. Invalid RTC inhibits automatic scheduling; recovery does not auto-arm.
C6. Startup inputs/modes: reset with lid open, VP/D4 held, or during refill. Seed
real levels, silent boot, no automatic refill restoration or cached press action;
require release/fresh opening and explicit reconciliation acknowledgement as defined.

These are proposed grouped acceptance cases; exact fault commands and restart
acknowledgement workflow will be documented with each future implementation.
Passing A does not establish B/C. Optional physical midnight rollover remains a
separate previously deferred check.


## Historical standalone evidence — 2026-10-24

User screenshot confirms slot 5 WRITE_PASS/VERIFY_PASS and subsequent slot 0
VERIFY_PASS. Observed write_us=14625, sync_us=24523, total_us=65446;
max_operation_us=415915 for the run, originating operation unspecified.
Full A2 sequence and A1/A3/A4 still require explicit user completion reports.
Do not perform SD operations synchronously in the medication control path on
the assumption that small records always finish quickly. B/C remain proposed,
not implemented or physically validated.


### A-stage completion — user report 2026-10-24

User explicitly reports all A verifications passed. A1, A2, A3 and A4 are PASS;
this supersedes their earlier pending completion statuses. All basic standalone
SD acceptance checks are complete. B and C remain proposed future builds/tests.
The observed whole-operation maximum remains 415.915 ms; no worst-case or
physical combined control-response bound is claimed.


## B-stage implementation — 2026-10-24

Separate scheduled_sd_test now implements queued SD event logging; see
scheduled_sd_test.md for executable commands, schema, limitations and B1-B10
physical procedures. Initially all B physical checks were pending; the later
user report records B1 operational behavior and manual file-content review as
passed. Automated validation of that physical export and B2-B10 remain pending.
Desktop checks/build are recorded in that guide. C-stage NVS/restart reconciliation remains proposed
and is not implemented. Earlier future-tense sections reflect the initial plan;
the new guide controls the implemented B-stage behavior.
