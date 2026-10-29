#include <Arduino.h>
#include <MedicationSystem.h>
#include <cstring>
#include <DailyScheduler.h>
#include <ScheduledRtc.h>
#include <RefillButtonControl.h>
#include <SessionAlert.h>
#include <IncrementalClockOled.h>
#include <QueuedSdLogger.h>

namespace {
constexpr int COUNT = MedicationSystem::COMPARTMENT_COUNT;
static_assert(COUNT == 4, "Wiring map requires four compartments");
constexpr int REED_PINS[COUNT] = {25, 34, 35, 39};
constexpr int LED_PINS[COUNT] = {26, 27, 32, 33};
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr int REFILL_BUTTON_PIN = 4;
RefillButtonControl::Input refillButton;
constexpr int BUZZER_PIN = 14;
constexpr int SILENCE_BUTTON_PIN = 36;
SessionAlert sessionAlert;
IncrementalClockOled clockOled;
uint32_t lastLoopMs = 0, maxLoopGapMs = 0;
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
bool injectRtcUnavailable = false;
bool rtcReported = false;
uint32_t rtcSeconds = 0;
uint32_t lastPollMs = 0;
uint32_t logSessionAt=0;
uint8_t logSessionMask=0,startingMask=0;
uint64_t logEvent(const char* name,int compartment=0,uint32_t doseAt=0,
                  EventLog::Origin origin=EventLog::Origin::CONTROL,
                  uint32_t sessionOverride=0,uint8_t maskOverride=0,const char* detail="") {
    EventLog::Event event;
    event.uptime=eventTimeMs;
    event.quality=!rtcHealthy?EventLog::Quality::UNTRUSTED:
        ClockDisplay::trusted(rtcHealthy,lastPollMs,eventTimeMs)?EventLog::Quality::VALID:EventLog::Quality::STALE;
    event.rtc=rtcSeconds;
    event.compartment=static_cast<uint8_t>(compartment);
    event.doseAt=doseAt;
    event.sessionAt=sessionOverride?sessionOverride:logSessionAt;
    event.mask=maskOverride?maskOverride:logSessionMask;
    event.state=static_cast<uint8_t>(MedicationSystem::getState());
    event.origin=origin;
    std::strncpy(event.name,name,sizeof(event.name)-1);
    std::strncpy(event.detail,detail,sizeof(event.detail)-1);
    return QueuedSdLogger::enqueue(event); // Zero wait, RAM copy only.
}
void loggerNotifications() {
    static auto lastState=QueuedSdLogger::State::STARTING;
    static uint64_t lastDropped=0,lastUncertain=0;
    static uint32_t lastNotice=0;
    const auto s=QueuedSdLogger::snapshot();
    if(s.state!=lastState) {
        lastState=s.state;
        Serial.printf("# LOGGING_%s fault=%s errno=%d; reminder behavior unchanged\n",
            QueuedSdLogger::stateName(s.state),QueuedSdLogger::failureName(s.failure),s.error);
    }
    if((s.dropped!=lastDropped || s.uncertain!=lastUncertain) && millis()-lastNotice>=1000) {
        lastDropped=s.dropped; lastUncertain=s.uncertain; lastNotice=millis();
        Serial.printf("# LOGGING_LOSS dropped=%llu uncertain=%llu; inspect log status\n",
            static_cast<unsigned long long>(s.dropped),static_cast<unsigned long long>(s.uncertain));
    }
}


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

void handleScheduleEvent(DailyScheduler::Event e,bool injected=false) {
    using R = DailyScheduler::Result;
    if (e.result == R::NONE) return;
    const auto origin=injected?EventLog::Origin::TEST:EventLog::Origin::SCHEDULER;
    if (e.result == R::DUE) {
        bool due[COUNT]{};
        for (int i = 0; i < COUNT; ++i) due[i] = e.mask & (1 << i);
        Serial.printf("# AUTOMATIC_DUE scheduled_at_s2000=%lu\n", static_cast<unsigned long>(e.scheduledAt));
        logEvent("AUTOMATIC_DUE",0,e.scheduledAt,origin,e.scheduledAt,e.mask);
        startingMask=e.mask;
        if (!MedicationSystem::startSession(due, e.scheduledAt, eventTimeMs)) {
            logEvent("AUTOMATIC_START_REJECTED",0,e.scheduledAt,origin,e.scheduledAt,e.mask);
            Serial.println("# AUTOMATIC_START_REJECTED; occurrence consumed, no retry");
        }
        startingMask=0;
    } else if (e.result == R::LATE_SKIPPED) {
        logEvent("LATE_SKIPPED",0,e.scheduledAt,origin,e.scheduledAt,e.mask);
        Serial.printf("# LATE_SKIPPED scheduled_at_s2000=%lu; no dose outcome inferred\n",
                      static_cast<unsigned long>(e.scheduledAt));
    } else {
        const char* fault = e.result == R::RTC_FAULT ? "RTC_FAULT" :
            e.result == R::CLOCK_JUMP ? "CLOCK_JUMP" : "POLL_GAP";
        logEvent(fault,0,0,origin);
        Serial.printf("# %s: scheduler DISARMED; active session continues; explicit arm required\n", fault);
    }
}

void pollRtc() {
    const bool wasHealthy = rtcHealthy;
    rtcHealthy = !injectRtcUnavailable && ScheduledRtc::secondsSince2000(rtcSeconds);
    eventTimeMs = millis();
    lastPollMs = eventTimeMs;
    if (!rtcReported || rtcHealthy != wasHealthy) {
        logEvent(rtcHealthy?"RTC_HEALTHY":"RTC_UNTRUSTED",0,0,
            injectRtcUnavailable?EventLog::Origin::TEST:EventLog::Origin::SCHEDULER);
        Serial.println(rtcHealthy ? "# RTC_HEALTHY (does not auto-arm)" : "# RTC_FAULT: check OSF/calendar/I2C");
    }
    rtcReported = true;
    handleScheduleEvent(scheduler.observe(rtcSeconds, eventTimeMs, rtcHealthy),injectRtcUnavailable);
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
    Serial.println("# log status | log stop | log retry (stop/retry require DISARMED and closed IDLE)");
    Serial.println("# test log-write-fail | test log-sync-fail | test log-full | test log-slow | test log-normal | test log-burst");
    Serial.println("# Logging task core0, queue64, zero-wait enqueue; schedule/history still RAM-only");
    Serial.println("# OLED: RTC clock only, incremental transfers; oled retry (DISARMED/IDLE) | test oled-fail");
    Serial.println("# test rtc-unavailable | test rtc-restore: persistent READ injection, real RTC unchanged");
    Serial.println("# VP/GPIO36: fresh press silences current session only; next session audible again");
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
    QueuedSdLogger::printStatus();
    clockOled.status();
    Serial.printf("# max_loop_gap_ms=%lu (includes serial and RTC work)\n", static_cast<unsigned long>(maxLoopGapMs));
    Serial.printf("# refill_button=%s\n", refillButton.pressed() ? "pressed" : "released/initializing");
    Serial.printf("# audio_session=%s muted=%s silence_button=%s\n",
        sessionAlert.active() ? "active" : "inactive", sessionAlert.muted() ? "yes" : "no",
        sessionAlert.pressed() ? "pressed" : "released/initializing");
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
        logEvent("RTC_SET_REQUEST");
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
    if(count==2 && !std::strcmp(first,"log")) {
        if(!std::strcmp(second,"status")) { QueuedSdLogger::printStatus(); return; }
        if(!std::strcmp(second,"stop") || !std::strcmp(second,"retry")) {
            if(scheduler.armed() || MedicationSystem::getState()!=MedicationSystem::SystemState::IDLE) {
                Serial.println("# error: log stop/retry requires DISARMED and closed IDLE"); return;
            }
            if(!std::strcmp(second,"stop")) {
                logEvent("LOG_STOP_REQUEST",0,0,EventLog::Origin::LOGGER);
                QueuedSdLogger::stop();
                Serial.println("# Logging stop requested asynchronously; wait for STOPPED, pending=0, inflight=no, fault=none before clean power-off");
            } else {
                if(QueuedSdLogger::retry()) {
                    logEvent("LOG_RETRY_REQUEST",0,0,EventLog::Origin::LOGGER);
                    Serial.println("# Logging retry requested asynchronously; new exclusive file segment, no automatic replay of uncertain row");
                } else Serial.println("# error: retry requires logger FAULT/STOPPED with initialized task");
            }
            return;
        }
    }
    if (count == 1 && !std::strcmp(first, "clock")) { pollRtc(); ScheduledRtc::showTime(); return; }
    if (count == 1 && !std::strcmp(first, "disarm")) {
        scheduler.disarm(); logEvent("SCHEDULER_DISARMED"); Serial.println("# DISARMED; active session unchanged"); return;
    }
    if (count == 1 && !std::strcmp(first, "arm")) {
        pollRtc();
        if (scheduler.arm(rtcSeconds, eventTimeMs, rtcHealthy)) {
            logEvent("SCHEDULER_ARMED"); Serial.println("# ARMED: only future occurrences eligible");
        } else { logEvent("ARM_REJECTED"); Serial.println("# error: arm requires healthy RTC, nonempty schedule and DISARMED"); }
        return;
    }
    if (!std::strcmp(first, "schedule") && count == 2) {
        if (!std::strcmp(second, "show")) { showSchedule(); return; }
        if (!std::strcmp(second, "clear")) {
            const bool cleared=scheduler.clear();
            if(cleared) logEvent("SCHEDULE_CLEARED");
            Serial.println(cleared ? "# schedule cleared" : "# error: disarm before editing"); return;
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
            const bool added=scheduler.add(seconds,mask);
            if(added) logEvent("SCHEDULE_ADDED",0,0,EventLog::Origin::CONTROL,0,mask,tokens[2]);
            Serial.println(added ? "# schedule entry added/merged" :
                "# error: armed, full or overlapping window (including midnight); unchanged");
            return;
    }
    if (count == 2 && !std::strcmp(first,"oled") && !std::strcmp(second,"retry")) {
        if (scheduler.armed() || MedicationSystem::getState()!=MedicationSystem::SystemState::IDLE) {
            Serial.println("# error: OLED retry requires DISARMED and closed IDLE"); return;
        }
        pollRtc();
        logEvent("OLED_RETRY_REQUEST");
        clockOled.begin(ClockDisplay::trusted(rtcHealthy,lastPollMs,millis()),rtcSeconds);
        Serial.println("# OLED retry queued; no schedule or medication changes"); return;
    }
    if (count == 2 && !std::strcmp(first, "test")) {
        using Injection=QueuedSdLogger::Injection;
        if(!std::strcmp(second,"log-write-fail") || !std::strcmp(second,"log-sync-fail") || !std::strcmp(second,"log-full")) {
            const auto injection=!std::strcmp(second,"log-full")?Injection::FULL:
                !std::strcmp(second,"log-sync-fail")?Injection::SYNC:Injection::WRITE;
            const auto stats=QueuedSdLogger::snapshot();
            if(stats.state!=QueuedSdLogger::State::READY || !stats.run) {
                Serial.println("# error: injection requires logger READY"); return;
            }
            QueuedSdLogger::inject(injection);
            logEvent(injection==Injection::FULL?"TEST_LOG_FULL":injection==Injection::SYNC?"TEST_LOG_SYNC_FAIL":"TEST_LOG_WRITE_FAIL",0,0,EventLog::Origin::TEST);
            Serial.println("# TEST INJECTION queued: next worker row fails; no card removal/full-card creation"); return;
        }
        if(!std::strcmp(second,"log-slow") || !std::strcmp(second,"log-normal")) {
            const bool slow=!std::strcmp(second,"log-slow");
            QueuedSdLogger::delay(slow?1000:0);
            logEvent(slow?"TEST_LOG_SLOW":"TEST_LOG_NORMAL",0,0,EventLog::Origin::TEST);
            Serial.println(slow?"# TEST INJECTION: worker sleeps 1000ms per event; control task does not wait":"# TEST INJECTION: worker delay cleared"); return;
        }
        if(!std::strcmp(second,"log-burst")) {
            for(unsigned i=0;i<128;++i) logEvent("BENCH_EVENT",0,0,EventLog::Origin::TEST);
            Serial.println("# TEST INJECTION: 128 bench events offered to queue; inspect dropped count; no medication events fabricated"); return;
        }
        if (!std::strcmp(second,"rtc-unavailable") || !std::strcmp(second,"rtc-restore")) {
            injectRtcUnavailable = !std::strcmp(second,"rtc-unavailable");
            logEvent(injectRtcUnavailable?"TEST_RTC_UNAVAILABLE":"TEST_RTC_RESTORE",0,0,EventLog::Origin::TEST);
            Serial.println(injectRtcUnavailable ? "# TEST INJECTION: RTC reads unavailable until test rtc-restore" :
                "# TEST INJECTION ended: normal RTC reads restored; no automatic arm");
            pollRtc(); return;
        }
        if (!std::strcmp(second,"oled-fail")) {
            logEvent("TEST_OLED_FAIL",0,0,EventLog::Origin::TEST);
            clockOled.injectFailure();
            Serial.println("# TEST INJECTION: next OLED transfer fails; RTC unchanged"); return;
        }
        if (!std::strcmp(second, "rtc-fail")) {
            Serial.println("# TEST INJECTION: one unhealthy RTC sample");
            logEvent("TEST_RTC_FAIL",0,0,EventLog::Origin::TEST);
            handleScheduleEvent(scheduler.observe(rtcSeconds, eventTimeMs, false),true); return;
        }
        if (!std::strcmp(second, "clock-jump")) {
            Serial.println("# TEST INJECTION: RTC sample +10 seconds; actual RTC unchanged");
            logEvent("TEST_CLOCK_JUMP",0,0,EventLog::Origin::TEST);
            handleScheduleEvent(scheduler.observe(rtcSeconds + 10, eventTimeMs, true),true); return;
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
    if(!std::strcmp(event,"DOSE_STARTED") && !logSessionAt) {
        logSessionAt=scheduledAt; logSessionMask=startingMask;
    }
    sessionAlert.event(event, eventTimeMs);
    digitalWrite(BUZZER_PIN, sessionAlert.outputHigh(millis()) ? HIGH : LOW);
    const auto sequence=logEvent(event,compartment,scheduledAt,EventLog::Origin::CORE);
    Serial.printf("%lu,%s,%d,%lu,uptime+rtc_s2000,lid_only,%llu\n",
                  static_cast<unsigned long>(eventTimeMs), event, compartment,
                  static_cast<unsigned long>(scheduledAt),static_cast<unsigned long long>(sequence));
    if(!std::strcmp(event,"IDLE") || !std::strcmp(event,"REFILL_STARTED")) { logSessionAt=0; logSessionMask=0; }
}
} // namespace MedicationSystem

void setup() {
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SILENCE_BUTTON_PIN, INPUT); // External 10 kOhm pull-up; GPIO36 has no internal pull-up.
    sessionAlert.begin(digitalRead(SILENCE_BUTTON_PIN) == LOW, millis());
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
    Serial.println("# Scheduled lids + OLED + queued SD event log; DISARMED, schedule/history RAM only");
    Serial.println("# OLED clock at 0x3C; SDA21/SCL22 100kHz; one <=16-byte payload per loop");
    Serial.println("# scheduled_at_s counts seconds since 2000-01-01; RTC time is literal, no DST");
    QueuedSdLogger::begin();
    eventTimeMs=millis();
    logEvent("BOOT",0,0,EventLog::Origin::LOGGER);
    Serial.println("# SD logging uses core0 worker; SPI CS13 SCK18 MISO19 MOSI23; new MEDLOG CSV, no formatting");
    Serial.println("uptime_ms,event,compartment,scheduled_at_s,clock_source,evidence,event_seq");
    help();
    pollRtc();
    clockOled.begin(rtcHealthy,rtcSeconds);
    lastLoopMs=millis();
}

void loop() {
    eventTimeMs = millis();
    const uint32_t loopGap=eventTimeMs-lastLoopMs;
    if (loopGap>maxLoopGapMs) maxLoopGapMs=loopGap;
    lastLoopMs=eventTimeMs;
    // Handle silence before any new session: an idle edge cannot mute a new reminder.
    if (sessionAlert.sample(digitalRead(SILENCE_BUTTON_PIN) == LOW, eventTimeMs)) {
        digitalWrite(BUZZER_PIN, LOW);
        logEvent("SESSION_SILENCED");
        Serial.println("# SESSION_SILENCED: dose state and deadline unchanged");
    }
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
        logEvent("REFILL_BUTTON_PRESS");
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
    digitalWrite(BUZZER_PIN, sessionAlert.outputHigh(millis()) ? HIGH : LOW);
    clockOled.service(ClockDisplay::trusted(rtcHealthy,lastPollMs,millis()),rtcSeconds);
    loggerNotifications();
    yield();
}
