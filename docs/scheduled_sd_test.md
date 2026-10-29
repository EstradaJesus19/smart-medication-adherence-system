# Combined medication/OLED/queued SD logging bench

Environment `scheduled_sd_test`; a separate copy of the validated
scheduled_oled_test adapter with queued event logging. Existing firmware, core,
simulators and tests are preserved. No NVS configuration/restart recovery is
added: every reset still clears schedule, armed state and medication history.
Boot remains silent and DISARMED. An SD file is evidence, never a recovery input.

## Wiring and upload

Keep all tested wiring unchanged. SD module VCC uses the separate verified 5 V
header, GND common, CS13, SCK18, MISO19, MOSI23. Internal SD/buffer rail was
measured 3.295 V. Keep the expansion selector at 3.3 V for existing devices.
RTC/OLED SDA21/SCL22; reeds25/34/35/39, LEDs26/27/32/33, D4 refill, VP36 silence,
protected buzzer14 with tested 1 kOhm base pulldown and both 220 Ohm resistors.
Insert/remove card only with all power OFF. No new connections are required.
Use the tested 8 GB FAT32 card; back up existing data before storage experiments.

Close the monitor, then from project root PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\pio.exe' run -e scheduled_sd_test -t upload
& '.\.tools\venv\Scripts\pio.exe' device monitor -e scheduled_sd_test --echo
```

If needed, identify COMx with `device list`, use `--upload-port COMx` on upload
and `--port COMx` on monitor. Baud115200, Enter sends, Ctrl+C exits. Opening the
monitor may reset the board and therefore create another boot file.

## Operation and accounting

The control loop on core1 reads inputs, runs core/scheduler/audio, services OLED
chunks and enqueues fixed-size event copies with a ZERO-tick wait. The statically
allocated FreeRTOS queue holds 64 events. A priority1 worker on core0 alone owns
SD/SPI/VFS operations at 1 MHz, with an 8192-byte task stack. Task creation/SD
faults do not inhibit live medication behavior. There is no loop-side SD call,
wait-for-drain, file open, write or sync. Brief critical sections protect copied
status/counters; the storage worker never holds that lock during storage I/O.
No worker serial output is added, although SDK driver diagnostics can still occur.

The worker creates MEDLOG on demand and exclusively creates a file such as
`/MEDLOG/B0123456789abcdef_00.CSV`. The `/sd` prefix printed in log_path is an
ESP32 mount point, not another directory on the card. A random 64-bit boot ID
separates uptime sequences. Retry in the same boot creates the next unused suffix
00..99; existing paths are never overwritten. No formatting, deleting or old-file
repair. New boots create new files; they do not scan/replay old medication logs.

Each event is formatted as one CRC32-protected CSV row and written+fsynced.
`synced`/last_synced_seq increase only after a full write and successful fsync.
This is an API acknowledgment, not guaranteed power-loss durability or proof of
ingestion. A queued event is not yet synced. CRC detects incomplete/corrupt
records; it is not an independent witness that fsync completed.

Counters (all RAM-only, cumulative for this boot):
- attempted: every event offered, including rejected attempts;
- accepted: copied into queue, not necessarily written;
- pending/inflight: queued/current worker event;
- synced: worker acknowledged write+sync;
- dropped: queue-full, logging-disabled or explicitly discarded queued events;
- uncertain: dequeued events without write+sync acknowledgment, including
  simulated failures; such rows may be absent, partial or fully present;
- last_uncertain_seq identifies the most recent unacknowledged event.

On an I/O failure, close/stop the file, show LOGGING_FAULT and retain queued events
(up to capacity). No automatic retries. The uncertain dequeued row is NEVER
resent, avoiding implicit duplication. Explicit retry creates a new segment and
writes retained later events. A failed close/unmount can itself delay the worker;
main controls do not wait. Logger state/fault/path and counts are reported on
serial. OLED remains a clock, not a logging-status display.

Queue overflow drops the NEW event, increments dropped and leaves sequence gaps.
Existing queued records retain order. Loss counters sampled by the writer appear
on subsequent rows; if power is lost before another row, RAM-only counters and
queued records cannot be recovered. A logging fault does not change outcomes,
start/cancel sessions, extend deadlines, silence sound or auto-disarm scheduling.
No exactly-once guarantee or NVS/SD transaction is claimed.

## Commands

All scheduled_oled_test commands remain, plus:

| Command | Effect |
| --- | --- |
| `log status` | Status/counters/path/observed worker timings; no SD read |
| `log stop` | DISARMED and closed IDLE only: stop accepting, asynchronously drain/close/unmount |
| `log retry` | DISARMED and closed IDLE, logger FAULT/STOPPED only: retry/new file asynchronously |
| `test log-write-fail` | Next worker event write fails before writing (simulated EIO) |
| `test log-sync-fail` | Next worker event writes but sync acknowledgment fails (simulated EIO) |
| `test log-full` | Next worker event write fails with simulated ENOSPC; does not fill card |
| `test log-slow` | Add 1000 ms sleep per event on worker, yielding core0; control loop never sleeps |
| `test log-normal` | Clear injected worker delay (current sleep may finish first) |
| `test log-burst` | Offer 128 BENCH_EVENT rows, origin=test; creates queue pressure, no fake doses |

Write/sync/full injections require READY and logging enabled; they affect the
next worker event, which may precede the injection marker if a backlog exists.
They are explicitly labelled on serial and are not electrical/full-card tests.
After an injected sync failure the row may become fully readable when closing;
it remains uncertain in accounting and must not be inferred as acknowledged.

For clean removal: complete/close sessions, `disarm`, `test log-normal`, then
`log stop`. Wait for **STOPPED, pending=0, inflight=no, fault=none**, then unplug
USB before removing card. Controls/clock continue while draining. If logging was
already faulted with no writable file, stop discards the backlog and counts it;
it cannot promise that those records were saved. A remaining fault must be
reported rather than treated as a clean successful close. Events offered after
stop are counted dropped; logging resumes only through explicit `log retry`.

`status` includes loop/OLED metrics and logger status. max_storage_us measures
observed worker open/append operations, excluding injected sleeps; max_write_us
and max_sync_us are API call durations. These are sampled maxima, not bounds.
Task separation avoids control waiting on SD I/O, but shared CPU/SDK/serial
resources still require physical responsiveness measurements. The <=100 ms
stable-edge response target is not established by loop-gap counters alone.

## Log schema

CSV header:

```text
v,boot_id,seq,uptime_ms,rtc_s2000,rtc_quality,origin,event,compartment,scheduled_at_s2000,session_at_s2000,due_mask,state,dropped,uncertain,detail,crc32
```

- v=1; seq increases per offered event in one boot, including dropped attempts.
- RTC uses the shared scheduler snapshot; quality is valid/untrusted/stale
  (older than 1000 ms). Untrusted/stale timestamps are encoded as 0. Valid time
  is a checked calendar/OSF snapshot, not proof of absolute clock accuracy.
- origin=core/scheduler/control/test/logger. Injection markers and injected
  scheduler faults are origin=test; the RTC value is still an actual snapshot,
  not a fabricated +10 second clock-jump sample.
- scheduled_at_s2000 preserves the core's explicit dose timestamp (or scheduler
  occurrence); 0 means no explicit context. For a dose event, its dated key is
  scheduled_at_s2000 plus compartment. It never proves pill removal/ingestion.
- session_at_s2000/due_mask carry the active reminder context through final
  closure; mask bits0..3 select compartments1..4. A wrong-opening compartment
  may be outside that mask and is not thereby assigned a dose. Previous-dose
  repeated access can have a different explicit dose timestamp.
- state is sampled at the callback, which may precede a following state change.
- detail stores HH:MM:SS for an accepted SCHEDULE_ADDED; otherwise '-' currently.
  RTC_SET_REQUEST records intent only, not a successful validated time write.
- CRC32 covers every byte BEFORE the final comma/checksum, excluding newline.

Core serial CSV gains an event_seq column for comparison with SD rows; the old
simulator/other builds retain their schemas. System/control events also consume
sequence numbers, so adjacent core serial rows need not have consecutive IDs.

## Read-only PC validation

After a clean stop/power-off, use a PC reader. Inspect the path reported by
`log status`; use the corresponding drive path, e.g. E:\MEDLOG\B..._00.CSV:

```powershell
& '.\.tools\venv\Scripts\python.exe' checks/verify_event_log.py 'E:\MEDLOG\B0123456789abcdef_00.CSV'
```

Replace the example with the actual file. It checks schema, CRC, RTC quality,
boot identity and increasing sequence, reporting gaps and cumulative loss.
PASS means internally valid rows; inspect gaps/dropped/uncertain too. It does
not mean every event was saved or every row durably acknowledged. New retry
segments may start above sequence1; examine earlier segments/status as well.
An incomplete/bad row reports FAIL and valid-prefix count; the tool never trims,
repairs, deletes or rewrites it. Automatic on-device old-file reconciliation is
outside this build. Power loss may also corrupt a filesystem beyond one tail;
this stage does not implement filesystem crash recovery.

## Physical integration acceptance

Current evidence: B1 operational behavior and manual file-content review are
user-reported as passed (see the dated progress entry below). Automated validation
of that physical export and B2-B10 remain pending. The original implementation
results below retain the status at the time they were recorded.

For each new reminder: close lids/reach IDLE, `disarm`, `schedule clear`, `clock`,
add HH:MM:SS at least one minute ahead and `arm`. Use 1+3 unless specified.
Different times need >=61 seconds separation; no manual dose command.

B1. **Normal grouped log:** boot with card, `log status`: READY, fault=none,
zero dropped/uncertain and path in MEDLOG. Configure/arm a 1+3 session, silence
with VP, confirm each over 2 seconds and close. Clock continues; both LEDs off,
IDLE. Let queue drain; log status should show pending0/inflight=no and synced
increasing. Disarm/stop cleanly; PC validator passes with no gaps/loss. Compare
AUTOMATIC_DUE/DOSE_STARTED, silence, confirmations/closure/session finish against
serial event_seq. No unrelated file modification. Retry only after reinserting
with power OFF (cold boot) or `log retry` on STOPPED with card still fitted.

B2. **Access warnings and silence:** separate session, brief target opening,
wrong opening of a non-due compartment, fresh long target openings, then repeat
confirmed access. Check BRIEF_OPENING/wrong/repeated warnings in serial/file;
no incorrect dose outcome/deadline change. Idle VP press and VP held before next
reminder do not pre-mute it: release/repress. Repeat a normal reminder on 2+4 to
exercise all inputs/LEDs. Logging keeps operating and clock stays live.

B3. **Timeouts:** one full unattended 1+3 session and another with only 1
confirmed/closed. At original 60-second deadline, full session has both MISSED;
partial session retains 1 confirmed and marks only 3 MISSED. Sound/LEDs off;
correct events in file. No deadline extension from log activity.

B4. **Refill/buttons:** D4 enters refill, open due lids, reminder interrupts at
due time. D4 cannot silence; VP mutes without refill. Close/reopen to confirm,
then close all. D4 starts/finishes refill again. Check corresponding log rows.

B5. **Responsiveness under normal I/O:** use all four lids, D4 and VP while worker
writes; no duplicate edge or observable clock/control stall. Capture status
max_loop_gap_ms, max_oled_tx_us and worker max timings. Measure actual input/
response with suitable recording/instrumentation if claiming <=100 ms; firmware
metrics alone are supporting evidence, not that physical measurement.

B6. **Missing card:** stop/disarm, power OFF/remove card, reboot. Expect logger
FAULT/mount but OLED/RTC and controls work. Configure/arm a session and complete
it normally. Queue can retain at most64 events, then loss is counted; no card
format/retry storm. Power OFF/reinsert/reboot: new boot/file, no recovered RAM
queue or automatic schedule replay. Old completed log files remain intact.

B7. **Write/sync/full faults:** in a new audible reminder with READY logger,
send `test log-write-fail`, then `log status`. Expect FAULT, uncertain increases,
no false synced acknowledgment for that row, reminder deadline/buttons still
work. Try `log retry` while REMINDER/ARMED: rejected. Complete/close, `disarm`,
`log retry`; expect READY with a different segment, buffered later events saved,
uncertain row not resent. Repeat separately for `test log-sync-fail` and
`test log-full`. After sync fault a complete row may exist but is still uncertain.
These are software injections; real full-card/failed-medium tests remain separate.

B8. **Slow storage and queue overflow:** during reminder, send `test log-slow`,
then `test log-burst`. Expect dropped increases, queue never exceeds64, uncertainty
remains unchanged absent I/O fault. While the worker sleeps/backlogs, VP and
fresh lid openings still work; confirm/close within original deadline. OLED
clock advances. Send `test log-normal`; wait for drain and inspect counters and
CSV sequence gaps. No fake DOSE_STARTED/confirmation generated by BENCH_EVENT.
Do not expect loss-free logs after deliberately overflowing the queue.

B9. **RTC failure/jump:** during new reminder send `test rtc-unavailable`.
Scheduler DISARMED, OLED RTC UNTRUSTED, original uptime deadline/silence/lids
continue. Confirm1/let3 miss, `test rtc-restore`: clock resumes, no auto-arm.
Logs mark untrusted time as0 and injected origin appropriately. Repeat
`test clock-jump` in a new session: actual RTC unchanged, scheduler disarmed,
active session finishes normally. Clock-jump fault row is labelled test.

B10. **Clean restart/export and tail validator:** stop cleanly, power cycle.
Clock survives but schedule/history reset/DISARMED; new boot/file, older files
unchanged. PC validation of normal-run files passes. On a COPY of a log on the
PC, remove final few bytes and validate: FAIL with earlier valid-prefix count;
original is untouched. This is a validator test, not a live interrupted-write
hardware test. Actual cuts during writes use backed-up/spare media only in a
separately reviewed failure/recovery stage. No current restart-recovery claim.

Report B1-B10 individually, counts/timings/path, and any glitches. Code/desktop
checks do not substitute for these physical tests. C-stage NVS/reconciliation
checks remain future work. Optional physical midnight rollover remains deferred.

## Validation

2026-10-24 execution results:

- ESP32 scheduled_sd_test build: PASS. Static RAM 31,320 bytes; flash 373,205
  bytes. Worker task additionally requests an 8192-byte stack at runtime.
- New EventLog checks: PASS, C++17 with -Wall -Wextra -Wpedantic -Werror.
  Covers CRC standard vector, schema/trust marking, maximum field sizes,
  unsafe text/range/buffer rejection, short/failed writes and failed sync
  without internal retries or false success.
- Read-only Python validator self-tests and a fixture written by the compiled
  C++ encoder: PASS. Covers truncated tails, corruption, valid prefix,
  duplicate/backward sequence, gaps, boot identity and untrusted timestamp rules.
- Existing unchanged domain regression executable: PASS 26/26. Existing
  scheduler, session-audio and clock-display check executables rerun: PASS.
- All 38 pre-existing source/include/library/test/check files verified unchanged
  against pre-edit hashes; complete prior PlatformIO configuration retained as
  exact prefix, with only the new environment appended.
- Pinned existing GFX/BusIO versions resolved for the new environment. Initial
  sandbox network/compiler attempts failed; approved outside-sandbox runs passed.
- No agent upload or physical combined SD logging test was performed. All
  B1-B10 physical tests remain pending. Host tests do not exercise FreeRTOS
  concurrency, SPI/FATFS, real media faults or physical input latency.

Reproduce new portable checks:

```powershell
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PWD '.tools\zig-cache'
& '.\.tools\venv\Lib\site-packages\ziglang\zig.exe' c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Ilib/EventLog/src checks/event_log_checks.cpp -o build/event-log-checks.exe
if ($LASTEXITCODE -ne 0) { throw 'Event log check compilation failed' }
& '.\build\event-log-checks.exe' build/event-log-fixture.csv
& '.\.tools\venv\Scripts\python.exe' checks/verify_event_log.py --self-test build/event-log-fixture.csv
```


## User-reported physical progress — 2026-10-24

B1 operational behavior and manual PC content review: PASS.
- First single-compartment session confirmed/closed normally after VP silence.
- Subsequent grouped 1+3 session: independent ACCESS_CONFIRMED outcomes,
  SESSION_FINISHED, closure and IDLE as expected.
- Grouped-run screenshot: logger READY, fault=none, pending=0, inflight=no,
  attempted=34, accepted=34, synced=34, dropped=0, uncertain=0,
  last_synced_seq=34, last_uncertain_seq=0.
- Boot ID 835339e380420697; file /MEDLOG/B835339e380420697_00.CSV.
  Observed max_storage_us=79137, max_write_us=9979, max_sync_us=29414;
  these worker API/operation maxima are not physical control-response bounds.
- User subsequently reviewed the card file and reported all expected information
  present and correct. This is a user-reported content review; the automated
  validator's actual PASS/CRC/gap output has not been supplied and is not inferred.
- B2-B10 remain pending. This entry supersedes the initial all-pending statement
  only for the specifically reported behavior/content review above.
