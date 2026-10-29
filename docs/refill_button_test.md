# Refill button standalone check — 2026-10-24

Separate environment refill_button_test, source src/refill_button_test.cpp.
Existing medication firmware and tests are unchanged. This test does NOT
enter/finish refill, read lids, drive OLED, or access RTC. Four indicators are off.

Proposed wiring: external 10 kOhm from verified 3.3 V to D4/GPIO4 input node;
momentary normally-open button between that node and common GND. GPIO36 remains
reserved for silence. The user's button/contact variant has not been shown;
confirm its behavior electrically rather than assuming the internal contacts.
Disconnect USB before any wiring change. Four-leg buttons have connected pairs:
use contacts that are open released and connected only while pressed.

Before upload/test, use DC voltage mode with power on to measure D4-to-GND:
approximately 3.3 V released, approximately 0 V pressed. Stop if either differs;
disconnect USB before correcting wiring. Resistance/continuity checks are only
for an isolated, unpowered button. Do not probe powered circuits in current mode.

From the project root in PowerShell, close all other serial monitors:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\python.exe' -m serial.tools.list_ports -v
# Replace COMx with current CP210x port:
& '.\.tools\venv\Scripts\pio.exe' run -e refill_button_test -t upload --upload-port COMx
& '.\.tools\venv\Scripts\pio.exe' device monitor -e refill_button_test --port COMx
```

Monitor is 115200 baud; no commands required. Press EN if startup was missed.
Ctrl+C exits. State changes must be stable for 30 ms; debounce is nonblocking.

Physical checks:
1. Released at startup: READY, no PRESS.
2. Press once: PRESS 1. Hold 10 seconds: no repeats. Release: RELEASED once.
3. Ten further deliberate press/release cycles: exactly one PRESS per cycle,
   counter increases by one each time, no unsolicited events during steady holds.
4. Hold button, press EN: INITIAL HELD, no PRESS. Release: RELEASED. Press again:
   PRESS 1. Counter resets on ESP32 reset; startup hold is never a press action.

Report voltage readings and results. Only after this physical input check passes
will a separate reviewed integration map press edges to startRefill/finishRefill.
Closing lids alone must not finish refill; a due session must still interrupt
refill. This source is not that integration.

Validation: user subsequently reported all standalone checks worked as expected.
PlatformIO compilation/linking passed: 21,472 bytes static RAM, 269,833 bytes
flash. Sandbox compiler execution failed; approved outside-sandbox retry
succeeded. No upload or hardware verification was performed by the agent.
Existing automated behavior tests were not rerun for this standalone input test.

User report recorded 2026-10-24: standalone operation passed, including ready
released at startup, one event per press, no repeats during a ten-second hold,
ten deliberate press/release cycles, and held-at-reset ignored until release.
The user reported all worked as expected, including the requested electrical
checks, but did not supply separate numeric voltage readings. These are manual
user observations, not agent-observed hardware or archived serial traces.
Physical refill-state integration is a separate test: scheduled_refill_test.
