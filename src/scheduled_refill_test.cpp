#include <Arduino.h>
#include <MedicationSystem.h>
#include <cstring>
#include <DailyScheduler.h>
#include <ScheduledRtc.h>
#include <RefillButtonControl.h>

namespace {
constexpr int COUNT = MedicationSystem::COMPARTMENT_COUNT;
static_assert(COUNT == 4, "Wiring map requires four compartments");
constexpr int REED_PINS[COUNT] = {25, 34, 35, 39};
constexpr int LED_PINS[COUNT] = {26, 27, 32, 33};
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr int REFILL_BUTTON_PIN = 4;
RefillButtonControl::Input refillButton;
bool rawOpen[COUNT]{};
bool stableOpen[COUNT]{};
bool ready = false;
uint32_t rawChangedAt[COUNT]{};
uint32_t eventTimeMs = 0;
char commandBuffer[80]{};
unsigned used = 0;
bool overflow = false;
DailyScheduler scheduler;
bool rtcHealthy = false;
bool rtcReported = false;
uint32_t rtcSeconds = 0;
uint32_t lastPollMs = 0;

void showSchedule() {
    Serial.printf("# scheduler=%s rtc=%s entries=%u (RAM only)\n",
        scheduler.armed() ? "ARMED" : "DISARMED", rtcHealthy ? "healthy" : "FAULT/unknown",
        scheduler.count());
    for (unsigned i = 0; i < scheduler.count(); ++i) {
        const auto& e = scheduler.entry(i);
        Serial.printf("# %02lu:%02lu:%02lu compartments=", static_cast<unsigned long>(e.secondOfDay / 3600),
            static_cast<unsigned long>(e.secondOfDay / 60 % 60), static_cast<unsigned long>(e.secondOfDay % 60));
        for (int c = 0; c < COUNT; ++c) if (e.mask & (1 << c)) Serial.printf(" %d", c + 1);
        Serial.println();
    }
}

void handleScheduleEvent(DailyScheduler::Event e) {
    using R = DailyScheduler::Result;
    if (e.result == R::NONE) return;
    if (e.result == R::DUE) {
        bool due[COUNT]{};
        for (int i = 0; i < COUNT; ++i) due[i] = e.mask & (1 << i);
        Serial.printf("# AUTOMATIC_DUE scheduled_at_s2000=%lu\n", static_cast<unsigned long>(e.scheduledAt));
        if (!MedicationSystem::startSession(due, e.scheduledAt, eventTimeMs))
            Serial.println("# AUTOMATIC_START_REJECTED; occurrence consumed, no retry");
    } else if (e.result == R::LATE_SKIPPED) {
        Serial.printf("# LATE_SKIPPED scheduled_at_s2000=%lu; no dose outcome inferred\n",
                      static_cast<unsigned long>(e.scheduledAt));
    } else {
        const char* fault = e.result == R::RTC_FAULT ? "RTC_FAULT" :
            e.result == R::CLOCK_JUMP ? "CLOCK_JUMP" : "POLL_GAP";
        Serial.printf("# %s: scheduler DISARMED; active session continues; explicit arm required\n", fault);
    }
}

void pollRtc() {
    const bool wasHealthy = rtcHealthy;
    rtcHealthy = ScheduledRtc::secondsSince2000(rtcSeconds);
    eventTimeMs = millis();
    if (!rtcReported || rtcHealthy != wasHealthy)
        Serial.println(rtcHealthy ? "# RTC_HEALTHY (does not auto-arm)" : "# RTC_FAULT: check OSF/calendar/I2C");
    rtcReported = true;
    handleScheduleEvent(scheduler.observe(rtcSeconds, eventTimeMs, rtcHealthy));
    lastPollMs = eventTimeMs;
}

bool timeOfDay(const char* text, uint32_t& seconds) {
    if (std::strlen(text) != 8 || text[2] != ':' || text[5] != ':') return false;
    for (int i : {0, 1, 3, 4, 6, 7}) if (text[i] < '0' || text[i] > '9') return false;
    const unsigned h = (text[0] - '0') * 10 + text[1] - '0';
    const unsigned m = (text[3] - '0') * 10 + text[4] - '0';
    const unsigned s = (text[6] - '0') * 10 + text[7] - '0';
    seconds = h * 3600 + m * 60 + s;
    return h < 24 && m < 60 && s < 60;
}

void help() {
    Serial.println("# Physical D4 button: press in IDLE=start refill; press in REFILL=finish if closed");
    Serial.println("# Held-at-startup ignored; release/repress required; button does not arm or silence");
    Serial.println("# schedule add HH:MM:SS N [N ...] | schedule show | schedule clear");
    Serial.println("# arm | disarm | status | clock | set YYYY-MM-DD HH:MM:SS | refill start|finish | help");
    Serial.println("# test rtc-fail | test clock-jump (explicit bench injections, no wiring changes)");
    Serial.println("# Daily literal RTC time; no DST; max 8 grouped times; edit only disarmed; arm never catches up");
    Serial.println("# N=1..4, distinct compartments; physical lids; real uptime; window=60000 ms; minimum_open=2000 ms");
    Serial.println("# LED ON=pending dose; OFF=all other conditions. Access is not ingestion.");
}

void status() {
    using namespace MedicationSystem;
    showSchedule();
    Serial.printf("# refill_button=%s\n", refillButton.pressed() ? "pressed" : "released/initializing");
    ScheduledRtc::showTime();
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
    if (!std::strncmp(line, "set ", 4)) {
        if (!ready || scheduler.armed() || MedicationSystem::getState() != MedicationSystem::SystemState::IDLE) {
            Serial.println("# error: set requires DISARMED and closed IDLE"); return;
        }
        ScheduledRtc::setTime(line + 4);
        pollRtc();
        return;
    }
    // Tokenize whitespace and validate the entire command before acting.
    char* tokens[7]{};
    unsigned count = 0;
    for (char* token = std::strtok(line, " \t"); token;
         token = std::strtok(nullptr, " \t")) {
        if (count == 7) { Serial.println("# error: too many arguments"); return; }
        tokens[count++] = token;
    }
    if (!count) return;
    const char* first = tokens[0];
    const char* second = count > 1 ? tokens[1] : nullptr;
    if (!second && std::strcmp(first, "help") == 0) { help(); return; }
    if (!ready) { Serial.println("# waiting for stable initial lid reading"); return; }
    if (!second && std::strcmp(first, "status") == 0) { status(); return; }
    if (count == 1 && !std::strcmp(first, "clock")) { pollRtc(); ScheduledRtc::showTime(); return; }
    if (count == 1 && !std::strcmp(first, "disarm")) {
        scheduler.disarm(); Serial.println("# DISARMED; active session unchanged"); return;
    }
    if (count == 1 && !std::strcmp(first, "arm")) {
        pollRtc();
        if (scheduler.arm(rtcSeconds, eventTimeMs, rtcHealthy))
            Serial.println("# ARMED: only future occurrences eligible");
        else Serial.println("# error: arm requires healthy RTC, nonempty schedule and DISARMED");
        return;
    }
    if (!std::strcmp(first, "schedule") && count == 2) {
        if (!std::strcmp(second, "show")) { showSchedule(); return; }
        if (!std::strcmp(second, "clear")) {
            Serial.println(scheduler.clear() ? "# schedule cleared" : "# error: disarm before editing"); return;
        }
    }
    if (count >= 4 && !std::strcmp(first, "schedule") && !std::strcmp(second, "add")) {
            uint32_t seconds;
            if (!timeOfDay(tokens[2], seconds)) { Serial.println("# error: time must be HH:MM:SS"); return; }
            uint8_t mask = 0;
            for (unsigned i = 3; i < count; ++i) {
                const char* n = tokens[i];
                if (n[0] < '1' || n[0] > '4' || n[1] != '\0' || (mask & (1 << (n[0] - '1')))) {
                    Serial.println("# error: use distinct compartments 1..4");
                    return;
                }
                mask |= 1 << (n[0] - '1');
            }
            Serial.println(scheduler.add(seconds, mask) ? "# schedule entry added/merged" :
                "# error: armed, full or overlapping window (including midnight); unchanged");
            return;
    }
    if (count == 2 && !std::strcmp(first, "test")) {
        if (!std::strcmp(second, "rtc-fail")) {
            Serial.println("# TEST INJECTION: one unhealthy RTC sample");
            handleScheduleEvent(scheduler.observe(rtcSeconds, eventTimeMs, false)); return;
        }
        if (!std::strcmp(second, "clock-jump")) {
            Serial.println("# TEST INJECTION: RTC sample +10 seconds; actual RTC unchanged");
            handleScheduleEvent(scheduler.observe(rtcSeconds + 10, eventTimeMs, true)); return;
        }
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
    Serial.printf("%lu,%s,%d,%lu,uptime+rtc_s2000,lid_only\n",
                  static_cast<unsigned long>(eventTimeMs), event, compartment,
                  static_cast<unsigned long>(scheduledAt));
}
} // namespace MedicationSystem

void setup() {
    Serial.begin(115200);
    pinMode(REFILL_BUTTON_PIN, INPUT); // Tested external 10 kOhm pull-up to 3V3.
    refillButton.begin(digitalRead(REFILL_BUTTON_PIN) == LOW, millis());
    for (int i = 0; i < COUNT; ++i) {
        pinMode(REED_PINS[i], INPUT); // Separate external 10 kOhm pull-ups to 3V3.
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
        rawOpen[i] = digitalRead(REED_PINS[i]) == HIGH;
        rawChangedAt[i] = millis();
    }
    Wire.begin(21, 22, 100000);
    Wire.setTimeOut(20);
    Serial.println("# Scheduled lids + physical refill button; DISARMED, schedule/history RAM only");
    Serial.println("# Serial state output only; OLED is not updated by this build");
    Serial.println("# scheduled_at_s counts seconds since 2000-01-01; RTC time is literal, no DST");
    Serial.println("uptime_ms,event,compartment,scheduled_at_s,clock_source,evidence");
    help();
    pollRtc();
}

void loop() {
    eventTimeMs = millis();
    const bool wasReady = ready;
    const auto buttonEvent = refillButton.sample(digitalRead(REFILL_BUTTON_PIN) == LOW, eventTimeMs);
    sampleLids(eventTimeMs);
    if (ready) MedicationSystem::update(eventTimeMs);
    // A newly accepted press forces a fresh calendar check before the action:
    // a due session wins over a simultaneous attempt to start/finish refill.
    if (ready && (eventTimeMs - lastPollMs >= 200 || buttonEvent == RefillButtonControl::Event::PRESSED))
        pollRtc();
    switch (buttonEvent) {
    case RefillButtonControl::Event::READY:
        Serial.println("# REFILL_BUTTON_READY"); break;
    case RefillButtonControl::Event::INITIAL_HELD:
        Serial.println("# REFILL_BUTTON_INITIAL_HELD: release before use"); break;
    case RefillButtonControl::Event::RELEASED:
        Serial.println("# REFILL_BUTTON_RELEASED"); break;
    case RefillButtonControl::Event::PRESSED:
        Serial.println("# REFILL_BUTTON_PRESS");
        if (!wasReady) Serial.println("# Button press ignored: lids not initialized; release/repress");
        else {
            RefillButtonControl::applyPress();
            Serial.printf("# button_result_state=%s\n", MedicationSystem::stateName(MedicationSystem::getState()));
        }
        break;
    case RefillButtonControl::Event::NONE: break;
    }
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
