# First physical test: one reed switch and one LED

This standalone firmware tests input polarity, stable transitions and LED output.
It does not run the medication state machine. The user verified continuity with
the magnet near and no continuity with the magnet away.

## Wiring (disconnect USB while assembling)

Use the board's printed D25 / D26 labels, corresponding to GPIO25 / GPIO26.

```text
3V3 --- 10 kOhm ---+--- D25
                  |
               reed switch
                  |
GND --------------+

D26 --- 220 Ohm --- LED anode (+)
                   LED cathode (-) --- GND
```

Use USB power only for this test. The pull-up goes to 3V3, not VIN/5 V.
GPIO25 is configured as INPUT because the pull-up is external. All GND
connections share the same ground. Avoid straining the reed's glass seal.

## Build, upload and observe

From the PlatformIO terminal at the project root:

```sh
pio run -e reed_led_test
pio device list
pio run -e reed_led_test -t upload --upload-port COMx
pio device monitor -e reed_led_test --port COMx
```

Replace COMx with the actual ESP32 port. Close any other program using that port
before uploading or opening the monitor. Baud rate is 115200. If the initial
message was missed, press EN/reset after opening the monitor.

With the existing workspace-local PlatformIO installation in PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\pio.exe' run -e reed_led_test
& '.\.tools\venv\Scripts\pio.exe' device list
# Replace COMx in the following commands:
& '.\.tools\venv\Scripts\pio.exe' run -e reed_led_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e reed_led_test --port COMx
```

## Acceptance observations

| Action | Expected output |
| --- | --- |
| Start with magnet near | LED off; Lid CLOSED once after stable initialization |
| Move magnet away | LED on; Lid OPEN once after 30 ms stable input |
| Hold magnet away | LED stays on; no repeated lines |
| Bring magnet near | LED off; Lid CLOSED once after 30 ms stable input |
| Start with magnet away | LED on after initialization; Lid OPEN once |

Repeat ten open/close cycles and note unexpected transitions. Test slow movement
and find a reliable magnet position away from the switching boundary. The 30 ms
interval is provisional electrical debounce, not medication access verification.
Until the first stable reading, the LED is off without implying a confirmed lid
condition. A physical test is complete only after the LED and sensor behavior
are observed; successful compilation/upload alone is insufficient.

If serial follows the magnet but the LED does not, check LED polarity and wiring.
If the input never changes, check the common ground, D25 node, resistor and switch
connections. Use continuity mode only on an unpowered circuit.

To restore the serial medication simulator, upload environment esp32dev instead.
The native desktop simulator and medication core are unaffected by this test.

## Validation record — 2026-10-16

- reed_led_test compiled successfully: 21,472 bytes static RAM, 269,337 bytes flash.
- esp32dev simulator also compiled successfully after source-filter separation.
- The CP210x USB interface was initially detected on COM9. It disappeared before
  the upload could open it; a subsequent port listing contained only Bluetooth
  ports. No upload completed and no LED/sensor observation has been recorded.
- Reconnect the ESP32, identify its current port, then upload and perform the
  acceptance observations above. Build success alone does not validate wiring.

### Successful retry

After reconnection, the CP210x interface was detected again on COM9. Upload
completed successfully with flash hashes verified. A short serial capture at
115200 baud after reset received the startup message followed by `Lid OPEN`.
This verifies firmware execution and a reported stable HIGH input; it does not
yet verify magnet-driven transitions or the external LED. Those observations
remain pending. The serial port was closed after capture.

### Physical follow-up — 2026-10-18

The user subsequently reported matching magnet/LED/serial transitions, steady
holds, ten-cycle repeatability, slow-movement checks and startup in both magnet
positions, with no flicker or repeated messages. These manual checks passed
as reported; no new serial capture was collected for this record.
