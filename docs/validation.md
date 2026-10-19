# Validation — 2026-09-22 simplified update

## Checks executed locally

| Check | Result |
| --- | --- |
| 26 C++ behavior tests via standalone assertion runner | PASS, 26/26 |
| Desktop simulator, C++17 with strict project warnings | PASS |
| ESP32 Arduino firmware via PlatformIO | PASS |
| README two-dose and refill examples through desktop stdin | PASS; two confirmations, closure checks and final IDLE |
| Captured CSV schema | PASS; six fields and expected outcomes |
| Documentation links and original archive contents | PASS |
| Original bootstrap preservation | Source/configuration/docs archive in reference/bootstrap-v1.zip |
| Physical upload and hardware tests | Not performed |

ESP32 build: PlatformIO 6.1.18; espressif32 6.10.0; Arduino framework package
3.20017.241212+sha.dcc1105b; Xtensa toolchain 8.4.0+2021r2-patch5.
Build reports 21,656 bytes static RAM and 271,017 bytes flash. These are linker
figures, not measured peak runtime memory or physical performance.

Host checks use Zig 0.13.0's C++ frontend with
`-std=c++17 -Wall -Wextra -Wpedantic -Werror`.
The same test functions are configured for Unity in PlatformIO. This update's
local execution used the standalone assertion runner; neither the native
PlatformIO/Unity runner nor GitHub Actions has been run here.

## Reproduce with the existing local tools

From the project root in PowerShell:

```powershell
# Firmware
$env:PLATFORMIO_CORE_DIR = Join-Path $PWD '.tools\platformio'
& '.\.tools\venv\Scripts\pio.exe' run -e esp32dev

# Host tests
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PWD '.tools\zig-cache'
& '.\.tools\venv\Lib\site-packages\ziglang\zig.exe' c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -DSTANDALONE_TEST -Ilib/MedicationSystem/src -Ilib/Simulation/src lib/MedicationSystem/src/MedicationSystem.cpp lib/Simulation/src/Simulation.cpp test/test_domain/test_main.cpp -o build/domain-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& '.\build\domain-tests.exe'

# Desktop simulator
& '.\.tools\venv\Lib\site-packages\ziglang\zig.exe' c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Ilib/MedicationSystem/src -Ilib/Simulation/src lib/MedicationSystem/src/MedicationSystem.cpp lib/Simulation/src/Simulation.cpp src/native_main.cpp -o build/simulator.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& '.\build\simulator.exe'
```

These local tools and executables live in ignored .tools/ and build/ directories.
A fresh checkout uses the standard PlatformIO commands in the README and needs a
host compiler for the native environment. No new tool installation was required
for this update.

## Limits

No automatic scheduler, RTC driver, physical debounce/output, microSD logger,
internal recovery storage or app exists in this version. History is lost on
restart. Initialization intentionally clears RAM records. The archived bootstrap
contains the earlier implementation and its historical validation record; it
must not be confused with the active source.
