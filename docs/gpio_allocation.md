# Provisional GPIO allocation — 2026-10-18

Proposal for review only. No wiring or firmware changes implement this map.
Only GPIO25 and GPIO26 have been physically verified in this project.
The board is labelled ESP32 DEVKIT V1 TYPE-C; upload identified ESP32-D0WD-V3.
This does not identify the complete module or prove header exposure.
Before wiring, inspect clear photographs of both board sides, all header
labels and the metal module marking. Verify every proposed pin is exposed
and not occupied by board circuitry. GPIO numbers below are not header positions.

| Function | Proposed GPIO | Condition |
| --- | --- | --- |
| Lid 1 reed | 25 | Keep existing tested connection |
| Lid 2 reed | 34 | Confirm header; external 10 kOhm pull-up to 3V3 |
| Lid 3 reed | 35 | Confirm header; external 10 kOhm pull-up to 3V3 |
| Lid 4 reed | 39 | Confirm header (may be labelled VN); external 10 kOhm pull-up to 3V3 |
| Compartment 1 ordinary LED | 26 | Keep existing tested 220 Ohm series resistor |
| Compartment 2 ordinary LED | 27 | Separate current-limiting resistor; verify LED current |
| Compartment 3 ordinary LED | 32 | Separate current-limiting resistor; verify LED current |
| Compartment 4 ordinary LED | 33 | Separate current-limiting resistor; verify LED current |
| Shared I2C SDA: RTC + OLED | 21 | Verify modules, addresses and pull-ups to 3V3 |
| Shared I2C SCL: RTC + OLED | 22 | Same shared bus; combined pull-ups must be checked |
| microSD SPI clock | 18 | Reader/card and 3.3 V logic compatibility unconfirmed |
| microSD SPI MISO | 19 | Same compatibility check |
| microSD SPI MOSI | 23 | Same compatibility check |
| microSD chip select | 13 | Explicitly configure in future driver; not GPIO5 |
| Buzzer control | 14 | Driver-control reservation; not approval for direct buzzer connection |
| Silence button | 36 | Confirm header (may be labelled VP); external 10 kOhm pull-up to 3V3, button to GND |
| Optional additional buttons | Unassigned | Revisit actual need and module identification |
| RGB indicators | Unassigned | Identify common-anode/cathode or addressable type before deciding interface |

Each reed uses the tested topology: pull-up to 3V3, switch to shared GND;
magnet near LOW/closed, away HIGH/open. Confirm all four bare reeds have that
polarity individually. Keep independent external pull-ups and debounce per lid.

## Reasons and limits

GPIO34–39 are input-only and lack internal pull-up/down resistors; using them
for switches saves output-capable pins. Avoid boot-strapping GPIO0, 2, 5, 12,
15 for this proposal, GPIO6–11 for flash, and GPIO1/3 for programming/serial.
GPIO13/14 use JTAG functions by default; this proposal does not reserve JTAG
debugging. GPIO16/17 are deliberately unassigned until the module is identified:
they can be occupied by memory on some variants.

Four ordinary RGB LEDs require twelve independently driven color channels
plus current-limiting resistors, versus four outputs in this proposal. This
table does not allocate twelve channels. After identifying the LEDs, review
an external LED driver/expander, an addressable interface if applicable, or
retaining ordinary LEDs. Check per-channel and total supply/driver current.

Inspect the buzzer rating and interface before choosing its driver and supply.
Inspect RTC charging/backup circuitry before fitting a cell. Inspect OLED,
RTC and SD board power pins and level-shifting/pull-up circuits before
connection; module supply labels alone do not establish safe signal voltage.
No ESP32 signal should be pulled up to 5 V.

Next stage after map review: test the remaining reeds individually, then
propose a separate four-lid firmware and wiring step. Check adjacent magnets
for unwanted activation in representative mounting positions. Disconnect USB
before wiring changes. No RTC/OLED/buzzer/SD integration is included yet.

Sources checked for chip constraints (not a pinout of the user's board):
- [Espressif GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html)
- [ESP32 datasheet: pins and boot configuration](https://documentation.espressif.com/esp32_datasheet_en.html)
- [WROOM-32E/UE module datasheet: flash and pin mapping](https://documentation.espressif.com/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.html)
