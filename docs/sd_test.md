# Standalone SD card bench test

Environment `sd_test`; no medication scheduling, lid/button handling, RTC/NVS
writes or OLED updates. Buzzer GPIO14 and LEDs 26/27/32/33 are held LOW.
An image retained on the OLED is stale. No existing firmware/test/simulator is
modified. The default environment remains the original simulator.

## Verified module and wiring

User owns an 8 GB microSD card and a six-pin SPI module. Reported buffer marking:
LC125A / 15K / G4 / AJEJ (consistent with TI SN74LVC125A). Regulator marking:
117C-33-C238 (consistent with a 1117-family 3.3 V regulator; exact maker unknown).
The regulator/header continuity checks passed. User measured:

- Module VCC to GND: 5.05 V.
- Regulator metal tab to GND: 3.295 V.
- Tab to buffer pin14: 2 Ohm; buffer pin7 to GND: 0.5 Ohm, consistent with
  direct connections allowing probe/contact resistance.

These are user-reported measurements, not independent observations by the agent.
They support testing this specific module with its 5 V input and 3.3 V signals.

| Module header | ESP32 connection |
| --- | --- |
| VCC | Separate verified USB-derived 5 V header |
| GND | Common GND |
| CS | GPIO13 / D13 |
| SCK | GPIO18 / D18 |
| MOSI | GPIO23 / D23 |
| MISO | GPIO19 / D19 |

User reports these connections are fitted. Keep expansion selector at 3.3 V;
existing sensors/RTC/OLED remain on their verified supply. Use short SPI wires.
Retain the protected buzzer circuit and 1 kOhm base pulldown. Do not power the
SD module's VCC from the selectable 3.3 V rail for this configuration.
Insert/remove card or change wires only with USB/all power disconnected.

The 8 GB card is SDHC; FAT32 is the expected format. Check on a computer and
preserve existing files before any formatting. This firmware NEVER formats,
deletes, renames, repairs or deliberately overwrites an existing path. Existing
card data should be backed up before write experiments: a card/power failure
can still damage a filesystem even when writes use a dedicated directory.

## Upload

Close the monitor. From the project root in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\pio.exe' device list
& '.\.tools\venv\Scripts\pio.exe' run -e sd_test -t upload
& '.\.tools\venv\Scripts\pio.exe' device monitor -e sd_test --echo
```

If needed, add `--upload-port COMx` to upload and `--port COMx` to monitor,
using the port from device list. Baud 115200, Enter sends commands, Ctrl+C exits.
Opening the monitor may reset the board. Mount is attempted at startup, but no
file is automatically created. SPI pins are set explicitly at 1 MHz.

## Commands and file policy

| Command | Action |
| --- | --- |
| `status` | Cached mount/type/capacity, write fault, last verified slot and operation timing |
| `write N` | Exclusively create a new 512-byte record, sync, close, reopen and verify |
| `verify N` | Open an existing record read-only and compare its size and every byte |
| `unmount` | Close SD filesystem; still disconnect power before card removal |
| `mount` | Retry mount after failure or unmount; clears write fault only on success |
| `help` | Command summary |

N is 0..9999 (up to four decimal digits). `write 0` creates
`/SD_BENCH/T0000.TXT` on the card. The POSIX `/sd` prefix shown in source is
only the ESP32 mount point; it is not another directory on the physical card.

The directory is created only on explicit write. File creation uses O_EXCL,
so an existing file/directory with that path is refused even if its content is
invalid. Use a different unused slot instead of erasing evidence. There is no
append, overwrite, format or arbitrary-path command. Test records contain a
version marker, four-digit slot and deterministic printable test data, not real
medication events or RTC timestamps. Computer file dates are not meaningful RTC
evidence in this standalone build.

Write count, fsync result, close result and complete byte-for-byte reopened
readback must all succeed for WRITE_PASS. If a write/sync/readback operation
fails, retain any partial file and inhibit further writes until an explicit
unmount/mount. Missing verification files and existing-file refusals are
reported without modifying them. No automatic repair or retries.

## Consecutive physical tests (pending)

1. **Detect:** with power OFF, insert the card. Power/upload/open monitor.
   Expect SD_MOUNT_OK, mounted=yes, type=SDHC and card_bytes near the nominal
   8 GB capacity (not exactly 8 GiB). Buzzer/LEDs off. Send `status`.
2. **First record:** send `write 0` if slot 0 is unused. Expect VERIFY_PASS then
   WRITE_PASS, requested=512 returned=512. Send `verify 0`: VERIFY_PASS with
   bytes_checked=512. Record the full timing output. If FILE_EXISTS, choose
   another unused slot and use that number in following steps.
3. **Overwrite refusal:** send `write 0` again (or your chosen slot). Expect
   FILE_EXISTS and no modification. `verify 0` must still pass.
4. **Remount:** send `unmount`, then `verify 0` and `write 1`: both must refuse
   while unmounted. Send `mount`, then `verify 0`: pass. Do not remove the card
   while powered even after unmounting.
5. **EN/reset retention:** after WRITE_PASS, press EN. Expect mount succeeds,
   no new automatic file writes, outputs quiet. `verify 0` still passes.
6. **Clean power-cycle retention:** after operations finish, send `unmount`;
   unplug USB for 10 seconds and reconnect with card still fitted. Send
   `verify 0`: pass. This validates this closed test record, not interruption
   during an active write or guaranteed filesystem crash recovery.
7. **Repeated records/timing:** create five more records using unused slots,
   e.g. `write 1` through `write 5`, one at a time after each finishes. Each
   must pass; verify the first record again. Capture the largest write_us,
   sync_us and total_us plus `status`. Existing slots must be refused.
8. **No card/recovery:** unmount, unplug USB, remove card, then reconnect.
   Expect SD_MOUNT_FAILED without formatting/reset loop or sound. `write 0`
   must refuse. Unplug, reinsert card, power on and `verify 0`: pass.
9. **PC readback:** unmount/power OFF and use a PC reader. Inspect SD_BENCH:
   each successful TNNNN.TXT is exactly 512 bytes with
   SMART_MEDICATION_SD_BENCH_V1 then the matching four-digit slot. Existing
   unrelated files should be unchanged. Safely eject, reinsert with board power
   OFF, power up and verify the first record again.

Report results by number, including timing lines and any failures. If the board
reports SD_MOUNT_FAILED with the card fitted, stop before write tests and share
startup output. Do not hot-swap or intentionally cut power mid-write here.
A real full-card, interrupted-write, write-fault and corrupted-record hardware
test is deferred to the storage failure/recovery stage. No full-card acceptance
claim follows from these basic checks; errors are surfaced, not silently repaired.

## Timing and limits

open_us measures exclusive file creation; write_us measures the 512-byte write;
sync_us measures fsync; close_us measures close. verify_us includes reopen,
size/content checks and close. total_us includes directory work, generation,
write/sync/close and verification plus intervening serial reporting.
max_operation_us tracks observed whole operations, not worst-case latency.
A VERIFY_PASS after reboot checks retained bytes; successful immediate readback
alone does not prove that data survives sudden power loss.

These are BLOCKING calls intentionally isolated from the medication firmware.
A 512-byte call can still take a long time due to card internals. A background
queue alone would not make it nonblocking. Use measured latency/failure results
to design logging concurrency before integrating it with controls. No storage
or restart-recovery claims are added to the validated medication build.

## Validation

2026-10-24 execution results:

- ESP32 `sd_test` build: PASS. Static RAM 22,136 bytes; flash 338,185 bytes.
- New portable record/parser checks: PASS, compiled with C++17 and
  -Wall -Wextra -Wpedantic -Werror. Covers exact command grammar/slot bounds,
  record slot identity, complete versus truncated/oversized records, every
  single-byte corruption in representative records, and offset overflow guards.
- These checks do not exercise a physical card or establish FAT/VFS durability,
  full-card handling or exclusive-create behavior on hardware; physical steps
  above explicitly test the normal path and existing-file refusal.
- All 35 pre-existing source/include/library/test/check files verified unchanged
  against pre-edit hashes. platformio.ini retains its exact original prefix;
  only the new sd_test environment was appended. No additional external library
  dependencies were installed. Arduino SD/SPI and SDK VFS are bundled locally.
- Sandbox compiler execution failed; approved outside-sandbox runs passed after
  fixing Arduino macro/pin-name collisions in the new source.
- No board upload or physical SD test performed by the agent. All nine numbered
  physical results are updated below. Full-card and interrupted-write hardware
  validation remain deferred, as stated above.

Sources used for module review:
- TI SN74LVC125A datasheet (marking/supply/pinout):
  https://www.ti.com/lit/ds/symlink/sn74lvc125a.pdf
- Representative 1117 pinout (does not establish regulator manufacturer):
  https://www.onsemi.com/download/data-sheet/pdf/lm1117-d.pdf
- SD Association SDHC/FAT32 capacity definition:
  https://www.sdcard.org/developers/sd-standard-overview/capacity-sd-sdhc-sdxc-sduc/

Build uses locally installed Arduino-ESP32 2.0.17 SD/SPI and POSIX VFS APIs;
no new external library dependencies or network features.


## User-reported physical results — 2026-10-24

- Checks 1-3: PASS. Card detection, first exclusive write/reopen verification,
  explicit verification and refusal to overwrite the existing test path.
- Checks 5-6: PASS. User reported VERIFY_PASS after EN reset and after the
  instructed clean unmount/USB-off interval/reconnect procedure.
- Latest startup screenshot: SD_MOUNT_OK, mounted=yes, type=SDHC,
  card_bytes=8053063680, spi_hz=1000000, write_fault=no; mount_us=28564,
  max_operation_us=28564, last_verified_slot=-1 after fresh startup.
  This shows a 28.564 ms mount, not a write/sync latency measurement.
- Checks 4, 7, 8 and 9 remain pending: unmounted command refusals/remount,
  repeated records with write/sync/total timings, absent-card recovery and PC
  readback. Reset success does not implicitly establish these checks.
- No write_us/sync_us/total_us values have yet been supplied by the user.
- All results are user-reported. Combined SD/medication logging and persistence
  are not implemented or validated by these standalone checks.

See storage_integration_tests.md for the proposed staged acceptance checklist.


### Additional user screenshot — 2026-10-24

- Slot 5: VERIFY_PASS, bytes_checked=512, verify_us=15476; WRITE_PASS,
  requested=512, returned=512, open_us=5148, write_us=14625, sync_us=24523,
  close_us=247, total_us=65446.
- Original slot 0 verified afterward: VERIFY_PASS, bytes_checked=512,
  verify_us=15497.
- status: mounted=yes, SDHC, card_bytes=8053063680, spi_hz=1000000,
  write_fault=no, last_verified_slot=0, mount_us=29288,
  max_operation_us=415915.

This establishes slot 5 write/readback and retention of slot 0 after that write.
The screenshot does not independently show every intermediate slot or unmounted
command refusals/absent-card/PC checks. Do not infer all A checks passed.
The 415.915 ms maximum is a whole-operation observation; its originating
operation is unknown and it is not a measured maximum single write call.
The shown write_us is 14.625 ms, sync_us 24.523 ms, total_us 65.446 ms.
These blocking times support isolating future storage work from the control
loop; they do not by themselves validate a background logger or input latency.


### Standalone acceptance complete — user report 2026-10-24

The user explicitly reported that all A1-A4 verifications passed:
unmounted-operation refusals and remount readback, all five additional records
and oldest-record verification, missing-card startup/reinsertion recovery, and
PC file inspection followed by verification on the ESP32. Together with prior
checks 1-3 and 5-6, all nine listed basic physical SD checks are now user-reported
PASS. This supersedes earlier pending statuses; earlier screenshot observations
remain limited to what those screenshots actually show.

The standalone SD stage is complete for these basic checks. Full-card and
interrupted-write hardware tests remain deferred. Combined event logging, its
control latency and NVS/restart reconciliation remain unimplemented/unvalidated.
No firmware or wiring changes accompanied this result update.
