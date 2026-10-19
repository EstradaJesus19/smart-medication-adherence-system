#include "MedicationSystem.h"

namespace MedicationSystem {
namespace {
CompartmentStatus compartments[COMPARTMENT_COUNT];
SystemState state = SystemState::IDLE;
Settings config;
uint32_t sessionStartedAtMs = 0;

bool allLidsClosed() {
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        if (compartments[i].lidOpen) return false;
    }
    return true;
}
bool hasPendingDoses() {
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        if (compartments[i].inCurrentSession &&
            compartments[i].latestDose.outcome == DoseOutcome::PENDING) return true;
    }
    return false;
}
void requestClosure() {
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        if (compartments[i].lidOpen) report("CLOSE_LID", i + 1, 0);
    }
}
void goToIdleOrClosure() {
    if (allLidsClosed()) {
        state = SystemState::IDLE;
        report("IDLE", 0, 0);
    } else {
        state = SystemState::WAITING_FOR_CLOSURE;
        requestClosure();
    }
}
void finishSessionIfComplete() {
    if (state != SystemState::REMINDER || hasPendingDoses()) return;
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        compartments[i].inCurrentSession = false;
        compartments[i].verifying = false;
    }
    report("SESSION_FINISHED", 0, 0);
    goToIdleOrClosure();
}
} // private data and helpers

bool initialize(const bool initialLids[COMPARTMENT_COUNT], Settings settings) {
    // Bound demo windows to one day; the minimum must fit before the deadline.
    if (!initialLids || settings.minimumOpenTimeMs == 0 ||
        settings.minimumOpenTimeMs >= settings.doseWindowMs ||
        settings.doseWindowMs > 86400000U) {
        report("INVALID_SETTINGS", 0, 0);
        return false;
    }
    config = settings;
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        compartments[i] = CompartmentStatus{};
        compartments[i].lidOpen = initialLids[i];
    }
    sessionStartedAtMs = 0;
    goToIdleOrClosure();
    return true;
}

bool startSession(const bool due[COMPARTMENT_COUNT], uint32_t scheduledAt,
                  uint32_t nowMs) {
    bool anyDue = false;
    if (due) {
        for (int i = 0; i < COMPARTMENT_COUNT; ++i) anyDue = anyDue || due[i];
    }
    if (!anyDue) {
        report("EMPTY_SESSION_REJECTED", 0, scheduledAt);
        return false;
    }
    update(nowMs);
    if (state == SystemState::REMINDER) {
        report("SESSION_CONFLICT", 0, scheduledAt);
        return false;
    }
    if (state == SystemState::REFILL) report("REFILL_INTERRUPTED", 0, scheduledAt);

    sessionStartedAtMs = nowMs;
    state = SystemState::REMINDER;
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        CompartmentStatus& compartment = compartments[i];
        compartment.inCurrentSession = due[i];
        compartment.verifying = false;
        if (due[i]) {
            compartment.hasDose = true;
            compartment.latestDose = {scheduledAt, DoseOutcome::PENDING};
            report("DOSE_STARTED", i + 1, scheduledAt);
        }
    }
    // Already-open targets must close and reopen; no verification starts here.
    requestClosure();
    return true;
}

void update(uint32_t nowMs) {
    if (state != SystemState::REMINDER) return;
    const bool deadlineReached =
        nowMs - sessionStartedAtMs >= config.doseWindowMs;
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        CompartmentStatus& compartment = compartments[i];
        if (!compartment.inCurrentSession ||
            compartment.latestDose.outcome != DoseOutcome::PENDING) continue;

        // Deadline wins even when verification becomes eligible on this tick.
        if (deadlineReached) {
            compartment.latestDose.outcome = DoseOutcome::MISSED;
            compartment.verifying = false;
            report("DOSE_MISSED", i + 1, compartment.latestDose.scheduledAt);
            continue;
        }
        if (compartment.verifying && compartment.lidOpen &&
            nowMs - compartment.verificationStartedAtMs >= config.minimumOpenTimeMs) {
            compartment.latestDose.outcome = DoseOutcome::ACCESS_CONFIRMED;
            compartment.verifying = false;
            report("ACCESS_CONFIRMED", i + 1, compartment.latestDose.scheduledAt);
        }
    }
    finishSessionIfComplete();
}

void lidChanged(int compartmentNumber, bool isOpen, uint32_t nowMs) {
    if (compartmentNumber < 1 || compartmentNumber > COMPARTMENT_COUNT) {
        report("INVALID_COMPARTMENT", compartmentNumber, 0);
        return;
    }
    // Closing exactly at dwell confirms, provided the deadline has not expired.
    update(nowMs);
    CompartmentStatus& compartment = compartments[compartmentNumber - 1];
    if (compartment.lidOpen == isOpen) return;
    compartment.lidOpen = isOpen;
    report(isOpen ? "LID_OPEN" : "LID_CLOSED", compartmentNumber, 0);

    if (state == SystemState::REFILL) return;
    if (!isOpen) {
        if (compartment.verifying) {
            compartment.verifying = false;
            report("BRIEF_OPENING", compartmentNumber, compartment.latestDose.scheduledAt);
        }
        if (state == SystemState::WAITING_FOR_CLOSURE && allLidsClosed()) {
            state = SystemState::IDLE;
            report("IDLE", 0, 0);
        }
        return;
    }

    const bool expectedAccess = state == SystemState::REMINDER &&
        compartment.inCurrentSession &&
        compartment.latestDose.outcome == DoseOutcome::PENDING;
    if (expectedAccess) {
        compartment.verifying = true;
        compartment.verificationStartedAtMs = nowMs;
        report("VERIFICATION_STARTED", compartmentNumber, compartment.latestDose.scheduledAt);
        return;
    }

    const bool repeatedAccess = compartment.hasDose &&
        compartment.latestDose.outcome == DoseOutcome::ACCESS_CONFIRMED;
    if (repeatedAccess) {
        report("REPEATED_ACCESS", compartmentNumber, compartment.latestDose.scheduledAt);
    } else if (state == SystemState::REMINDER) {
        report("WRONG_COMPARTMENT", compartmentNumber, 0);
    } else {
        report("UNEXPECTED_ACCESS", compartmentNumber, 0);
    }
    // One access warning, with repeated access taking priority.
    report("CLOSE_LID", compartmentNumber, 0);
    if (state == SystemState::IDLE) state = SystemState::WAITING_FOR_CLOSURE;
}

bool startRefill() {
    if (state != SystemState::IDLE) {
        report("REFILL_START_REJECTED", 0, 0);
        return false;
    }
    state = SystemState::REFILL;
    report("REFILL_STARTED", 0, 0);
    return true;
}
bool finishRefill() {
    if (state != SystemState::REFILL) {
        report("REFILL_FINISH_REJECTED", 0, 0);
        return false;
    }
    if (!allLidsClosed()) {
        requestClosure();
        return false;
    }
    report("REFILL_FINISHED", 0, 0);
    goToIdleOrClosure();
    return true;
}
SystemState getState() { return state; }
const CompartmentStatus* getCompartment(int compartment) {
    if (compartment < 1 || compartment > COMPARTMENT_COUNT) return nullptr;
    return &compartments[compartment - 1];
}
const char* stateName(SystemState value) {
    switch (value) {
    case SystemState::IDLE: return "IDLE";
    case SystemState::REMINDER: return "REMINDER";
    case SystemState::WAITING_FOR_CLOSURE: return "WAITING_FOR_CLOSURE";
    case SystemState::REFILL: return "REFILL";
    }
    return "UNKNOWN";
}
const char* outcomeName(DoseOutcome value) {
    switch (value) {
    case DoseOutcome::PENDING: return "PENDING";
    case DoseOutcome::ACCESS_CONFIRMED: return "ACCESS_CONFIRMED";
    case DoseOutcome::MISSED: return "MISSED";
    }
    return "UNKNOWN";
}
} // namespace MedicationSystem
