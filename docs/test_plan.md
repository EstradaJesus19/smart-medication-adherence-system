# Test plan — simplified sessions

Use `pio test -e native` with a host GCC/MinGW compiler, or the standalone
commands in [validation.md](validation.md). The suite exercises the actual core
and command parser without hardware. Each test starts from a fresh simulated
device. An implemented test is not a claim that physical hardware has been tested.

## Automated cases

Function names in `test/test_domain/test_main.cpp` have the prefix `test_`.

| ID | Function suffix / scenario | Expected behavior | Requirements |
| --- | --- | --- | --- |
| T01 | correct_access_waits_for_closure | Confirm after dwell; IDLE only after closure | FR-03, FR-05, FR-11 |
| T02 | wrong_lid_must_also_close | Wrong lid remains part of closure check | FR-06, FR-11 |
| T03 | two_compartments_either_order | Both access orders succeed independently | FR-16 |
| T04 | simultaneous_openings_independent_timers | Both can verify concurrently | FR-05, FR-16 |
| T05 | brief_opening_does_not_affect_other_timer | Fresh dwell for retry; other target unaffected | FR-05, FR-16 |
| T06 | partial_confirmation_then_timeout | Preserve confirmed outcome; miss only pending dose once | FR-07 |
| T07 | no_interaction_misses_and_returns_idle | All pending missed; direct IDLE if closed | FR-07, FR-11 |
| T08 | deadline_wins_and_brief_attempts_do_not_extend_it | Timeout wins exact tie; original deadline retained | FR-05, FR-07 |
| T09 | close_at_exact_dwell_confirms | Exact dwell close confirms before deadline | FR-05 |
| T10 | duplicate_open_is_not_repeated_access | Held lid is one event; close/open warns | FR-04, FR-08 |
| T11 | repeat_during_pending_session_has_warning_priority | One repeated warning; other dose still pending | FR-06, FR-08, FR-16 |
| T12 | repeat_while_other_lid_waits_for_closure | Repeat warning during closure state | FR-08, FR-11 |
| T13 | next_dose_replaces_only_its_own_history | New occurrence pending; other histories retained | FR-08, FR-16 |
| T14 | missed_dose_cannot_be_confirmed_late | Unexpected late access; missed outcome unchanged | FR-07, FR-11 |
| T15 | startup_and_idle_openings_request_closure | Seed actual levels; no invented opening | FR-11, FR-15 |
| T16 | already_open_target_requires_reopening | No dwell until fresh close/open | FR-05, FR-11 |
| T17 | refill_explicit_finish_and_closed_lids_required | Closing alone is insufficient; finish rejects open lids | FR-14 |
| T18 | refill_suppresses_warnings_but_preserves_history | Reopening after refill still warns | FR-08, FR-14 |
| T19 | due_session_interrupts_refill_with_open_target | Refill interrupted; target must close/reopen | FR-14, FR-16 |
| T20 | refill_rejected_during_reminder_and_closure | Start only from IDLE | FR-14 |
| T21 | session_validation_and_conflict | Empty/replacement sessions rejected; expired old session resolved | FR-16, NFR-03 |
| T22 | invalid_settings_and_lid_numbers_do_not_corrupt_state | Reject invalid inputs without resetting session | FR-04, NFR-01 |
| T23 | elapsed_timers_across_rollover | Dwell and deadline work across uint32 rollover | FR-05, FR-07 |
| T24 | serial_workflow_status_and_csv_time | Commands, CRLF, status, refill and timestamped rows agree | FR-09, FR-10 |
| T25 | serial_input_rejection_is_atomic_and_parser_recovers | No partial dose on invalid list; overflow line discarded; next command works | FR-10 |
| T26 | long_advance_does_not_skip_confirmation | Confirmation at 2000 ms, other dose missed at 60000 ms | FR-05, FR-07, FR-10 |

Build/review checks cover NFR-01, NFR-07 and NFR-08. CSV capture review covers
NFR-05 and FR-09. Full dated scheduling (FR-01/02) is deferred, not covered by
manual injection tests.

## Manual examples

Reset between scenarios. Each semicolon below means a new command line.

- T02: `dose 2; open 3; open 2; advance 2000; close 2; status; close 3`.
  Status must still request closure while lid 3 is open.
- T05: `dose 1 3; open 1; advance 500; open 3; advance 500; close 1; advance 1500`.
  Compartment 3 confirmed; compartment 1 still pending.
- T19: `refill start; open 2; dose 2 4; advance 2000; status`.
  Refill interrupted, neither dose confirmed, lid 2 needs close/reopen.

## Later hardware and storage tests — not executed

| ID | Procedure / acceptance | Requirements |
| --- | --- | --- |
| H01 | Validate RTC, group equal-time entries, reject overlapping windows at different times; run offline across midnight without duplicate occurrences | FR-01/02, NFR-06 |
| H02 | Cycle each lid 30 times, including slow movement/bounce; no duplicate edges; target output latency <=100 ms after a stable edge | FR-04, NFR-02 |
| H03 | Repeat T01–T20 on wired device, ten trials per selected scenario | FR-05–08, FR-11, FR-14–16 |
| H04 | Simulate invalid/lost RTC time; inhibit automatic scheduling and show fault | FR-12 |
| H05 | Verify numbered LEDs/text, audible warnings, silence-only control and persistent closure indication | FR-03, FR-13, NFR-04 |
| H06 | Remove/fill microSD or interrupt writing; expose logging failure without silently losing configuration | FR-24 |
| H07 | Restart before/during/after dose; reconcile dated outcome and real lids; do not restore interrupted verification as completed | FR-20 |
| H08 | Startup with arbitrary open lids and stuck sensors; document limits rather than infer removal | FR-04, FR-15 |

## Experimental records

Use the [data protocol](../data/README.md): raw output, cleaned CSV, metadata and
independently observed labels. Use dummy objects. Report trials and denominators
for correct access, wrong/repeated warnings, missed outcomes and false
confirmations. Measure physical latency independently; simulated time does not
measure hardware response. Report failures and min/median/max latency.

A minimum-open interval can reject genuine fast access and accept longer accidental
openings. Experiment with both cases before selecting a value. Physical debounce
is separate from this interval.

The later optional lid-plus-weight comparison needs calibration, stable mounting,
known removals/returns and enclosure-motion trials. Neither lid nor weight
evidence establishes ingestion. Do not add that work before the core prototype.
