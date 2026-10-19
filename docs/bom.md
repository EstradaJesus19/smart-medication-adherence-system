# Planned bill of materials

This is a planning list, not a purchasing quote or verified wiring design.
“Required” means required for the Level 1 physical MVP. Simulation requires none
of these parts. Record actual manufacturer, module variant, cost and supplier
after selection; no availability or price is assumed here.

| Component | Qty | Classification | Role / selection note |
| --- | --- | --- | --- |
| ESP32 DevKit, likely ESP32-WROOM-32 family | 1 | Required | Confirm exact board and pinout before wiring; generic esp32dev build configured |
| DS3231 RTC module | 1 | Required | I2C time source; verify module pull-ups and backup circuit |
| Reed switch | 4 | Required | One per lid; determine open/closed electrical polarity experimentally |
| Small neodymium magnet | 4 | Required | One securely mounted in each lid; check alignment and neighboring-lid interference |
| LED | 4 | Required | One numbered compartment indicator |
| LED current-limiting resistor | 4 | Required | Select from LED forward voltage and intended current; no direct GPIO-to-LED connection |
| Active buzzer | 1 | Required | Choose supply/current rating; driver circuit depends on actual part |
| Buzzer transistor/MOSFET and bias components | 1 set | Recommended | Use a driver if needed by buzzer load; becomes required if GPIO limits are exceeded |
| Momentary push button | 1 | Required | Silence audible alert only; does not confirm access |
| Additional push buttons | 1–2 | Optional upgrade | Navigation/time setup if justified by UI |
| 0.96-inch I2C SSD1306 OLED module | 1 | Required | Text feedback; check actual resolution, address and logic voltage |
| USB data cable and suitable 5 V USB supply | 1 each | Required | Development power and programming through DevKit USB |
| Breadboard, jumper wires and headers | 1 set | Required | Initial interconnection |
| Four-compartment organizer / simple prototype body | 1 | Required | Repeatable independent lid movement; dummy contents for tests |
| RTC-compatible backup cell | 1 | Recommended | Match actual module chemistry and charging design; inspect before fitting |
| External input pull-ups and small decoupling parts | As needed | Recommended | Select after checking module schematics and noise behavior |
| Perfboard, connectors, strain relief | 1 set | Recommended | Stabilize Level 1 demonstration without a custom PCB |
| Logic analyzer / oscilloscope access | 1 | Recommended | Measure debounce and latency; borrowed lab equipment is sufficient |
| microSD card and ESP32-compatible SPI breakout/socket | 1 each | Recommended | Planned removable event logs; not needed for current simulation; verify actual module voltage compatibility before wiring |
| Load cell, nominal 100–500 g range | TBD | Optional upgrade | Capacity alone does not establish pill-removal resolution; account for container mass and mounting |
| HX711 module | 1 per chosen sensing channel | Optional upgrade | Decide whether weighing whole organizer or individual compartment before ordering |
| 3D-printed enclosure | 1 | Optional upgrade | Prototype geometry after sensor positioning is verified |
| Custom PCB | 1 design | Optional upgrade | Only after breadboard validation |

## Electrical integration decisions still to make

Use the DevKit's supported USB power input. Keep GPIO and I2C signal levels within
the ESP32 specification; a module advertised for 5 V power is not automatically
safe to connect to a 3.3 V signal bus. Inspect pull-ups on both I2C modules and
check the selected GPIOs for boot-strapping, flash and input-only restrictions.
These limits and pin capabilities are defined in the
[Espressif ESP32 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf).

The DS3231 supports I2C and battery-backed timekeeping; the module's own circuit
must still be checked separately from the IC. Source:
[Analog Devices DS3231 product documentation](https://www.analog.com/en/products/ds3231.html).

No GPIO assignment, buzzer current budget, backup-cell type or OLED address is
committed until the actual modules are available. Capture those decisions in the
schematic and board configuration rather than guessing them in firmware now.
