# Storage and restart proposal — 2026-10-24

This document preserves the original storage/restart design proposal. Its NVS
configuration and restart-recovery policies remain unimplemented and require
review before implementation. Standalone SD testing and queued event logging
have since been implemented in `sd_test` and `scheduled_sd_test`; see the
[current logging guide](scheduled_sd_test.md) for behavior and validation.
The staged sequence below records the original plan, not the current work order.
The user owns an 8 GB card and an SPI SD module with LC125A buffer and
117C-33 regulator markings. User-reported power/continuity checks support its
5 V module input with an internal 3.295 V rail; standalone testing now uses
`sd_test`. See [SD test guide](sd_test.md) for the measurements and wiring.
That standalone build does not run medication sessions or restart recovery.

## Roles

- ESP32 internal NVS: small validated configuration and recovery records, available
  even if the SD card is missing. Use a versioned encoding, generation counter,
  integrity check and validated previous record; verify write results/readback.
- Removable SD: append-only event history for export and a later app. It is not
  the authority for live dose state or configuration. Record occurrence ID,
  event sequence, boot ID, RTC time/trust, uptime, compartment and evidence.
- RTC: calendar time only. Its battery does not retain ESP32 state or power the
  controller during USB loss.

Espressif Preferences stores small values in NVS and retains them across reset
and power loss: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html
This does not imply atomicity across independent application records or an
exactly-once transaction spanning NVS and SD. Interrupted writes need explicit
validation and power-cut tests.

## Proposed staged implementation

1. Separate configuration-persistence build, no SD wiring: explicit save/load of
   validated daily schedule and timing settings. Boot loads valid settings but
   remains DISARMED. Reject corrupt/incompatible records visibly; no silent
   automatic erasure. Do not write every loop, RTC poll or OLED refresh.
2. Identify/test actual SD module separately: photographs of both sides, readable
   chip/regulator markings, header labels, card type/capacity. Verify supply and
   logic voltage before wiring. Existing SPI reservation: SCK18, MISO19, MOSI23,
   CS13. These are tentative until module review. Test card discovery, small file
   write/read, restart, no/full card and write latency before medication logging.
3. Separate restart-reconciliation build with validated recovery records and
   explicit interruption evidence. Save accepted occurrence identity/outcomes
   on state transitions. Persist consumption before allowing reminder activation;
   define and visibly handle persistence failure. Never infer committed state
   from a partial or failed write; expose uncertainty if reset interrupted commit.
4. Integrate queued SD logging only after SD latency/failure behavior is measured.
   Dose handling must not wait indefinitely for storage. Missing/full card gives
   visible LOGGING_FAULT; queue is bounded and any lost records are counted.
   A queue alone does not make a synchronous SD write nonblocking. Choose a worker
   and synchronization or deferred-write policy based on standalone results.

## Proposed first restart policy

- Boot silent and DISARMED; restore validated saved schedule, not an armed flag.
- Read/debounce actual lids and validate RTC before reconciliation.
- Retain outcomes that were successfully committed, with dated occurrence IDs.
- Never restore an in-progress two-second opening timer or refill mode.
- A pending session interrupted by reset is recorded as INTERRUPTED/UNRESOLVED,
  a separate recovery condition, not automatically ACCESS_CONFIRMED or MISSED.
  Do not call startSession to grant a fresh 60-second window on reboot.
- No automatic catch-up or reminders for occurrences during unobserved downtime.
  Distinguish downtime/unobserved evidence from observed missed interactions.
- Require explicit acknowledgement of interrupted recovery before future-only
  manual arming. Preserve durable no-replay information across schedule edits.
- Invalid RTC/storage keeps automatic scheduling inhibited with a visible reason.

This first policy deliberately requires user review/re-arm after power loss.
Autonomous continuation inside an original calendar deadline would be a later
reviewed feature, requiring additional core recovery APIs and time-validity rules.
It must not silently be implemented by restoring old millis() values.

## Acceptance plan

Configuration save/reset/load; invalid/corrupt/version-mismatched record; power
cut during save; startup lids open/held buttons; reset before/during/after dose;
partial confirmation then reset; reset near deadline; RTC invalid/backward;
no replay after re-arm/edit; SD absent/full/write failure/truncated final record;
queue saturation/lost-record count; measured control latency during storage work.
Existing validated environments, tests and simulators remain unchanged.
