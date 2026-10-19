#pragma once
#include <stdint.h>

namespace MedicationSystem {
constexpr int COMPARTMENT_COUNT = 4;
enum class SystemState { IDLE, REMINDER, WAITING_FOR_CLOSURE, REFILL };
enum class DoseOutcome { PENDING, ACCESS_CONFIRMED, MISSED };

struct DoseOccurrence {
    uint32_t scheduledAt = 0; // Calendar seconds from a future scheduler.
    DoseOutcome outcome = DoseOutcome::PENDING;
};
struct CompartmentStatus {
    bool lidOpen = false;
    bool hasDose = false;
    DoseOccurrence latestDose;
    bool inCurrentSession = false;
    bool verifying = false;
    uint32_t verificationStartedAtMs = 0;
};
struct Settings {
    uint32_t doseWindowMs = 60000;
    uint32_t minimumOpenTimeMs = 2000;
};

// One device, fixed storage. Initialize before other operations.
// Invalid settings leave the running system unchanged.
bool initialize(const bool initialLids[COMPARTMENT_COUNT], Settings settings);
bool startSession(const bool due[COMPARTMENT_COUNT], uint32_t scheduledAt,
                  uint32_t nowMs);
// Supply stable, debounced edges in time order. Compartment numbers are 1..4.
void lidChanged(int compartment, bool isOpen, uint32_t nowMs);
// Call frequently; unsigned elapsed timers allow rollover, not backward jumps.
void update(uint32_t nowMs);
bool startRefill();
bool finishRefill();
SystemState getState();
const CompartmentStatus* getCompartment(int compartment); // nullptr if invalid
const char* stateName(SystemState state);
const char* outcomeName(DoseOutcome outcome);

// Output boundary implemented by Simulation.cpp for this build.
// Zero indicates no applicable compartment/dose context (except time zero).
void report(const char* event, int compartment, uint32_t scheduledAt);
} // namespace MedicationSystem
