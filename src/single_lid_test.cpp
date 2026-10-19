#include <Arduino.h>
#include <MedicationSystem.h>
#include <cstring>

namespace {
constexpr int REED_PIN = 25;
constexpr int LED_PIN = 26;
constexpr uint32_t DEBOUNCE_MS = 30;
bool rawOpen = false;
bool stableOpen = false;
bool ready = false;
uint32_t rawChangedAt = 0;
uint32_t eventTimeMs = 0;
char commandBuffer[80]{};
unsigned used = 0;
bool overflow = false;

void help() {
    Serial.println("# dose 1 | status | help | refill start | refill finish");
    Serial.println("# Physical lid 1 only; real uptime; window=60000 ms; minimum_open=2000 ms");
    Serial.println("# LED ON=pending dose; OFF=all other conditions. Access is not ingestion.");
}

void status() {
    using namespace MedicationSystem;
    const auto& lid = *getCompartment(1);
    Serial.printf("# uptime_ms=%lu state=%s lid1=%s verifying=%s outcome=%s\n",
                  static_cast<unsigned long>(eventTimeMs), stateName(getState()),
                  lid.lidOpen ? "open" : "closed", lid.verifying ? "yes" : "no",
                  lid.hasDose ? outcomeName(lid.latestDose.outcome) : "NONE");
}

void command(char* line) {
    // Tokenize whitespace and validate the entire command before acting.
    char* first = std::strtok(line, " \t");
    if (!first) return;
    char* second = std::strtok(nullptr, " \t");
    char* extra = std::strtok(nullptr, " \t");
    if (!second && std::strcmp(first, "help") == 0) { help(); return; }
    if (!ready) { Serial.println("# waiting for stable initial lid reading"); return; }
    if (!second && std::strcmp(first, "status") == 0) { status(); return; }
    if (second && !extra) {
        if (std::strcmp(first, "dose") == 0 && std::strcmp(second, "1") == 0) {
            const bool due[MedicationSystem::COMPARTMENT_COUNT] = {true, false, false, false};
            // No calendar clock: scheduled_at_s is explicitly unavailable (zero).
            MedicationSystem::startSession(due, 0, eventTimeMs);
            return;
        }
        if (std::strcmp(first, "refill") == 0) {
            if (std::strcmp(second, "start") == 0) { MedicationSystem::startRefill(); return; }
            if (std::strcmp(second, "finish") == 0) { MedicationSystem::finishRefill(); return; }
        }
    }
    Serial.println("# error: invalid command; use help");
}

void feed(char byte) {
    if (byte == '\r') return;
    if (byte == '\n') {
        if (overflow) Serial.println("# error: line too long; discarded");
        else { commandBuffer[used] = '\0'; command(commandBuffer); }
        used = 0;
        overflow = false;
    } else if (!overflow && used + 1 < sizeof(commandBuffer)) {
        commandBuffer[used++] = byte;
    } else {
        overflow = true;
    }
}

void sampleLid(uint32_t nowMs) {
    const bool reading = digitalRead(REED_PIN) == HIGH;
    if (reading != rawOpen) { rawOpen = reading; rawChangedAt = nowMs; }
    if (nowMs - rawChangedAt < DEBOUNCE_MS) return;
    if (!ready) {
        stableOpen = rawOpen;
        const bool initialLids[MedicationSystem::COMPARTMENT_COUNT] = {stableOpen, false, false, false};
        ready = MedicationSystem::initialize(initialLids, MedicationSystem::Settings{});
        Serial.println(stableOpen ? "# Initial lid 1 OPEN" : "# Initial lid 1 CLOSED");
    } else if (stableOpen != rawOpen) {
        stableOpen = rawOpen;
        MedicationSystem::lidChanged(1, stableOpen, nowMs);
    }
}
} // namespace

namespace MedicationSystem {
void report(const char* event, int compartment, uint32_t scheduledAt) {
    Serial.printf("%lu,%s,%d,%lu,uptime,lid_only\n",
                  static_cast<unsigned long>(eventTimeMs), event, compartment,
                  static_cast<unsigned long>(scheduledAt));
}
} // namespace MedicationSystem

void setup() {
    Serial.begin(115200);
    pinMode(REED_PIN, INPUT); // Existing external 10 kOhm pull-up to 3V3.
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    rawOpen = digitalRead(REED_PIN) == HIGH;
    rawChangedAt = millis();
    Serial.println("# Single physical lid test: waiting for stable initial reading");
    Serial.println("# scheduled_at_s=0: calendar time unavailable; history lost on reset");
    Serial.println("uptime_ms,event,compartment,scheduled_at_s,clock_source,evidence");
    help();
}

void loop() {
    eventTimeMs = millis();
    sampleLid(eventTimeMs);
    if (ready) MedicationSystem::update(eventTimeMs);
    // Bound serial work so sensor sampling and timers continue under input load.
    for (unsigned i = 0; i < 64 && Serial.available(); ++i) {
        feed(static_cast<char>(Serial.read()));
    }
    const auto* lid = ready ? MedicationSystem::getCompartment(1) : nullptr;
    const bool pending = lid && lid->inCurrentSession &&
        lid->latestDose.outcome == MedicationSystem::DoseOutcome::PENDING;
    digitalWrite(LED_PIN, pending ? HIGH : LOW);
    yield();
}
