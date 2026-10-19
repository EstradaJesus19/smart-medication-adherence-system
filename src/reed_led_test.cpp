#include <Arduino.h>

constexpr int REED_PIN = 25;
constexpr int LED_PIN = 26;

// Initial experimental value; adjust after testing the real switch.
constexpr unsigned long DEBOUNCE_MS = 30;

bool lastRawOpen = false;
bool stableOpen = false;
bool initialStateReported = false;
unsigned long lastRawChangeMs = 0;

void showLidCondition() {
    // In this test only: LED on means lid open.
    digitalWrite(LED_PIN, stableOpen ? HIGH : LOW);
    Serial.println(stableOpen ? "Lid OPEN" : "Lid CLOSED");
}

void setup() {
    Serial.begin(115200);
    // The external 10 kOhm resistor provides the pull-up to 3V3.
    pinMode(REED_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    lastRawOpen = digitalRead(REED_PIN) == HIGH;
    lastRawChangeMs = millis();
    Serial.println("Reed-switch test: waiting for a stable reading.");
}

void loop() {
    const unsigned long nowMs = millis();
    const bool rawOpen = digitalRead(REED_PIN) == HIGH;

    if (rawOpen != lastRawOpen) {
        lastRawOpen = rawOpen;
        lastRawChangeMs = nowMs;
    }

    if (nowMs - lastRawChangeMs >= DEBOUNCE_MS) {
        if (!initialStateReported || stableOpen != lastRawOpen) {
            stableOpen = lastRawOpen;
            initialStateReported = true;
            showLidCondition();
        }
    }
}
