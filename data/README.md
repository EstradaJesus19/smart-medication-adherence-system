# Event data — simplified simulation

No physical experimental data is included. Capture each run separately:
`data/<run-id>/raw.txt`, `events.csv`, `metadata.md` and independently
observed trial labels. Keep raw output unchanged.

Current CSV header (six fields):

```csv
time_ms,event,compartment,scheduled_at_s,clock_source,evidence
```

| Field | Meaning |
| --- | --- |
| time_ms | Simulated elapsed milliseconds, starting at zero |
| event | Named observation, outcome, warning or operation |
| compartment | 1–4, or 0 for a system/session event |
| scheduled_at_s | Dose context in artificial seconds; 0 when inapplicable |
| clock_source | simulated |
| evidence | lid_only |

Filter lines beginning with # before CSV import. They contain help, status and
parser errors. Unlike the old bootstrap, the CSV has no state column; status
reports state and each compartment separately. Do not mix old and new schemas.

Dose events and repeated-access warnings include the relevant scheduled timestamp.
Lid-level, closure and generic warning events have no occurrence context.
Time zero may also be a real injected occurrence: inspect the event type.
Manual dose commands in the same artificial second can share timestamps; segment
sessions using DOSE_STARTED / SESSION_FINISHED and run metadata. Do not treat this
as a globally unique identifier. A future calendar scheduler/recovery record must
define stable dated occurrence IDs.

State/history are RAM-only. Logging prints immediately; no microSD or persistent
writes exist. Completion records remain available after replacement only if the
serial output was captured.

Record: run ID, date, source revision, build target, simulation/hardware, settings,
schedule or injected commands, initial lids, expected/observed outcomes, pass/fail
and anomalies. Use dummy contents and no patient identifiers.

Planned storage: microSD for event history; internal storage for a small settings
and recovery record. Missing/full media, interrupted writes, consistency between
records and RTC validity must be addressed before claiming restart recovery.
