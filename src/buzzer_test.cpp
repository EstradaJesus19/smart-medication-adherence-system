#include <Arduino.h>
#include <cstring>

namespace {
constexpr int BUZZER_PIN = 14;
constexpr uint32_t BEEP_MS = 250;
bool active = false;
uint32_t startedMs = 0;
char line[32]{};
unsigned used = 0;
bool overflow = false;

void stopBeep() {
    digitalWrite(BUZZER_PIN, LOW);
    active = false;
    Serial.println("# Buzzer control LOW (off requested)");
}

void help() {
    Serial.println("# beep | stop | status | help");
    Serial.println("# Active-buzzer test: 250ms maximum per request; no automatic/repeating sound");
    Serial.println("# GPIO14 -> 220R -> PN2222A base; 10k base-emitter; protected, current-limited load");
    Serial.println("# Serial reports control requests, not measured acoustic behavior");
}

void command() {
    if (!std::strcmp(line, "beep")) {
        if (active) { Serial.println("# Already beeping; duration not extended"); return; }
        startedMs = millis();
        active = true;
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.println("# Buzzer control HIGH for 250ms (sound requested)");
    } else if (!std::strcmp(line, "stop")) stopBeep();
    else if (!std::strcmp(line, "status")) {
        Serial.printf("# gpio14_requested=%s; beep_ms=250; no sound feedback\n", active ? "HIGH" : "LOW");
    } else if (!std::strcmp(line, "help")) help();
    else if (*line) Serial.println("# Unknown command; use help");
}

void checkTimeout() {
    if (active && millis() - startedMs >= BEEP_MS) stopBeep();
}
} // namespace

void setup() {
    // Preload LOW before enabling the output. External pulldown also covers reset.
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
    for (int pin : {26, 27, 32, 33}) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
    Serial.begin(115200);
    Serial.println("# Standalone buzzer_test: startup silent, GPIO14 LOW");
    Serial.println("# Keep 220R in buzzer supply; banded diode end goes to buzzer POSITIVE");
    Serial.println("# No medication logic, button handling, OLED updates or RTC access");
    help();
}

void loop() {
    checkTimeout();
    for (unsigned i = 0; i < 64 && Serial.available(); ++i) {
        checkTimeout();
        const char byte = static_cast<char>(Serial.read());
        if (byte == '\r') continue;
        if (byte == '\n') {
            if (overflow) Serial.println("# Line too long; discarded");
            else { line[used] = '\0'; command(); }
            used = 0; overflow = false;
        } else if (!overflow && used + 1 < sizeof(line)) line[used++] = byte;
        else overflow = true;
    }
    checkTimeout();
    yield();
}
