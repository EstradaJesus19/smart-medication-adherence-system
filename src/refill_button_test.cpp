#include <Arduino.h>

namespace {
constexpr int BUTTON_PIN = 4;
constexpr uint32_t DEBOUNCE_MS = 30;
bool rawPressed = false;
bool stablePressed = false;
bool initialized = false;
bool enabled = false;
uint32_t rawChangedMs = 0;
unsigned presses = 0;
} // namespace

void setup() {
    Serial.begin(115200);
    // Separate experiment: keep the existing four indicators off.
    for (int pin : {26, 27, 32, 33}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    pinMode(BUTTON_PIN, INPUT); // External 10 kOhm pull-up to 3V3.
    rawPressed = digitalRead(BUTTON_PIN) == LOW;
    rawChangedMs = millis();
    Serial.println("# Refill BUTTON TEST ONLY: D4/GPIO4, external pull-up, pressed=LOW");
    Serial.println("# 30ms debounce; held-at-startup ignored until stable release");
    Serial.println("# No medication state changes, RTC writes or OLED updates");
}

void loop() {
    const uint32_t now = millis();
    const bool reading = digitalRead(BUTTON_PIN) == LOW;
    if (reading != rawPressed) { rawPressed = reading; rawChangedMs = now; }
    if (now - rawChangedMs >= DEBOUNCE_MS) {
        if (!initialized) {
            initialized = true;
            stablePressed = rawPressed;
            enabled = !stablePressed;
            Serial.println(stablePressed ? "# INITIAL HELD: release button to enable presses" :
                                           "# READY: button released");
        } else if (stablePressed != rawPressed) {
            stablePressed = rawPressed;
            if (!stablePressed) {
                enabled = true;
                Serial.println("# RELEASED: ready for next press");
            } else if (enabled) {
                ++presses;
                Serial.printf("# PRESS %u uptime_ms=%lu\n", presses, static_cast<unsigned long>(now));
            }
        }
    }
    yield();
}
