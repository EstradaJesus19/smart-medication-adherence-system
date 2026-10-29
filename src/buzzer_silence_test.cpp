#include <Arduino.h>
#include <SilenceBench.h>
#include <cstring>

namespace {
constexpr int BUZZER_PIN = 14, SILENCE_PIN = 36;
SilenceBench::Controller controller;
char line[48]{};
unsigned used = 0;
bool overflow = false;

void service() {
    const uint32_t now = millis();
    const auto event = controller.sample(digitalRead(SILENCE_PIN) == LOW, now);
    // Write the requested LOW before serial output if a press stopped the alert.
    const bool expired = controller.update(now);
    digitalWrite(BUZZER_PIN, controller.outputHigh(now) ? HIGH : LOW);
    using E = SilenceBench::ButtonEvent;
    switch (event) {
    case E::READY: Serial.println("# SILENCE_READY"); break;
    case E::INITIAL_HELD: Serial.println("# SILENCE_INITIAL_HELD: release before starting alert"); break;
    case E::RELEASED: Serial.println("# SILENCE_RELEASED"); break;
    case E::SILENCED: Serial.println("# ALERT_SILENCED: GPIO14 LOW"); break;
    case E::PRESS_IDLE: Serial.println("# SILENCE_PRESS_IDLE: no alert; not remembered"); break;
    case E::NONE: break;
    }
    if (expired) Serial.println("# ALERT_TIMEOUT: 10-second bench limit, GPIO14 LOW");
}

void help() {
    Serial.println("# alert | stop | status | help");
    Serial.println("# alert: 250ms on / 750ms off, maximum 10 seconds; press VP/GPIO36 to cancel");
    Serial.println("# GPIO36 needs external 10k pull-up to 3.3V, button to GND");
    Serial.println("# Buzzer: GPIO14 via 220R; PN2222A; 1k base-emitter; supply 220R; protection diode");
    Serial.println("# Standalone only: no doses, refill, OLED updates or RTC access");
}

void command() {
    if (!std::strcmp(line, "alert")) {
        if (controller.start(millis())) Serial.println("# ALERT_STARTED: bounded repeating bench sound");
        else Serial.println("# ALERT_REJECTED: already active, initializing or button not fully released");
    } else if (!std::strcmp(line, "stop")) {
        controller.stop();
        digitalWrite(BUZZER_PIN, LOW);
        Serial.println("# ALERT_STOPPED: GPIO14 LOW");
    } else if (!std::strcmp(line, "status")) {
        Serial.printf("# alert=%s silence_ready=%s silence_pressed=%s gpio14_requested=%s\n",
            controller.active() ? "active" : "idle", controller.ready() ? "yes" : "no",
            controller.pressed() ? "yes" : "no", controller.outputHigh(millis()) ? "HIGH" : "LOW");
    } else if (!std::strcmp(line, "help")) help();
    else if (*line) Serial.println("# Unknown command; use help");
}
} // namespace

void setup() {
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
    for (int pin : {26, 27, 32, 33}) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
    pinMode(SILENCE_PIN, INPUT); // GPIO36 has no internal pull-up.
    controller.begin(digitalRead(SILENCE_PIN) == LOW, millis());
    Serial.begin(115200);
    Serial.println("# Buzzer/silence test: startup idle/LOW; retain tested 1k base-emitter pulldown");
    help();
}

void loop() {
    service();
    for (unsigned i = 0; i < 64 && Serial.available(); ++i) {
        service();
        const char byte = static_cast<char>(Serial.read());
        if (byte == '\r') continue;
        if (byte == '\n') {
            if (overflow) Serial.println("# Line too long; discarded");
            else { line[used] = '\0'; command(); }
            used = 0; overflow = false;
        } else if (!overflow && used + 1 < sizeof(line)) line[used++] = byte;
        else overflow = true;
    }
    service();
    yield();
}
