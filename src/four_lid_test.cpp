#include <Arduino.h>
#include <MedicationSystem.h>
#include <cstring>

namespace {
constexpr int COUNT = MedicationSystem::COMPARTMENT_COUNT;
static_assert(COUNT == 4, "Wiring map requires four compartments");
constexpr int REED_PINS[COUNT] = {25, 34, 35, 39};
constexpr int LED_PINS[COUNT] = {26, 27, 32, 33};
constexpr uint32_t DEBOUNCE_MS = 30;
bool rawOpen[COUNT]{};
bool stableOpen[COUNT]{};
bool ready = false;
uint32_t rawChangedAt[COUNT]{};
uint32_t eventTimeMs = 0;
char commandBuffer[80]{};
unsigned used = 0;
bool overflow = false;

void help() {
    Serial.println("# dose N [N ...] | status | help | refill start | refill finish");
    Serial.println("# N=1..4, distinct compartments; physical lids; real uptime; window=60000 ms; minimum_open=2000 ms");
    Serial.println("# LED ON=pending dose; OFF=all other conditions. Access is not ingestion.");
}

void status() {
    using namespace MedicationSystem;
    Serial.printf("# uptime_ms=%lu state=%s\n",
                  static_cast<unsigned long>(eventTimeMs), stateName(getState()));
    for (int c = 1; c <= COUNT; ++c) {
        const auto& lid = *getCompartment(c);
        Serial.printf("# compartment=%d lid=%s verifying=%s outcome=%s session=%s\n", c,
                  lid.lidOpen ? "open" : "closed", lid.verifying ? "yes" : "no",
                  lid.hasDose ? outcomeName(lid.latestDose.outcome) : "NONE",
                  lid.inCurrentSession ? "yes" : "no");
    }
}

void command(char* line) {
    // Tokenize whitespace and validate the entire command before acting.
    char* tokens[5]{};
    unsigned count = 0;
    for (char* token = std::strtok(line, " \t"); token;
         token = std::strtok(nullptr, " \t")) {
        if (count == 5) { Serial.println("# error: too many arguments"); return; }
        tokens[count++] = token;
    }
    if (!count) return;
    const char* first = tokens[0];
    const char* second = count > 1 ? tokens[1] : nullptr;
    if (!second && std::strcmp(first, "help") == 0) { help(); return; }
    if (!ready) { Serial.println("# waiting for stable initial lid reading"); return; }
    if (!second && std::strcmp(first, "status") == 0) { status(); return; }
    if (second && std::strcmp(first, "dose") == 0) {
            bool due[COUNT]{};
            for (unsigned i = 1; i < count; ++i) {
                const char* n = tokens[i];
                if (n[0] < '1' || n[0] > '4' || n[1] != '\0' || due[n[0] - '1']) {
                    Serial.println("# error: dose needs distinct compartments 1..4");
                    return;
                }
                due[n[0] - '1'] = true;
            }
            // No calendar clock: scheduled_at_s is explicitly unavailable (zero).
            MedicationSystem::startSession(due, 0, eventTimeMs);
            return;
    }
    if (count == 2) {
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

void sampleLids(uint32_t nowMs) {
    bool allStable = true;
    for (int i = 0; i < COUNT; ++i) {
        const bool reading = digitalRead(REED_PINS[i]) == HIGH;
        if (reading != rawOpen[i]) { rawOpen[i] = reading; rawChangedAt[i] = nowMs; }
        if (nowMs - rawChangedAt[i] < DEBOUNCE_MS) { allStable = false; continue; }
        if (ready && stableOpen[i] != rawOpen[i]) {
            stableOpen[i] = rawOpen[i];
            MedicationSystem::lidChanged(i + 1, stableOpen[i], nowMs);
        }
    }
    if (!ready) {
        if (!allStable) return;
        for (int i = 0; i < COUNT; ++i) stableOpen[i] = rawOpen[i];
        ready = MedicationSystem::initialize(stableOpen, MedicationSystem::Settings{});
        for (int i = 0; i < COUNT; ++i)
            Serial.printf("# Initial lid %d %s\n", i + 1, stableOpen[i] ? "OPEN" : "CLOSED");
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
    for (int i = 0; i < COUNT; ++i) {
        pinMode(REED_PINS[i], INPUT); // Separate external 10 kOhm pull-ups to 3V3.
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
        rawOpen[i] = digitalRead(REED_PINS[i]) == HIGH;
        rawChangedAt[i] = millis();
    }
    Serial.println("# Four physical lids: waiting for stable initial readings");
    Serial.println("# scheduled_at_s=0: calendar time unavailable; history lost on reset");
    Serial.println("uptime_ms,event,compartment,scheduled_at_s,clock_source,evidence");
    help();
}

void loop() {
    eventTimeMs = millis();
    sampleLids(eventTimeMs);
    if (ready) MedicationSystem::update(eventTimeMs);
    // Bound serial work so sensor sampling and timers continue under input load.
    for (unsigned i = 0; i < 64 && Serial.available(); ++i) {
        feed(static_cast<char>(Serial.read()));
    }
    for (int i = 0; i < COUNT; ++i) {
        const auto* lid = ready ? MedicationSystem::getCompartment(i + 1) : nullptr;
        const bool pending = lid && lid->inCurrentSession &&
            lid->latestDose.outcome == MedicationSystem::DoseOutcome::PENDING;
        digitalWrite(LED_PINS[i], pending ? HIGH : LOW);
    }
    yield();
}
