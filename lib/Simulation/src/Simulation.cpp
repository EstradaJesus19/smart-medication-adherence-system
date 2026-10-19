#include "Simulation.h"
#include <cstdio>
#include <cstring>

namespace {
Simulation::WriteLine writeLine = nullptr;
uint32_t nowMs = 0;
char buffer[80]{};
unsigned used = 0;
bool overflow = false;

void print(const char* text) {
    if (writeLine) writeLine(text);
}
void help() {
    print("# dose N [N ...] | open N | close N | advance MS | refill start|finish | status | help");
    print("# N=1..4; advance=1..600000 milliseconds; restart/reset for a fresh run");
}
bool number(const char* text, uint32_t limit, uint32_t& value) {
    if (!text || !*text) return false;
    value = 0;
    for (const char* p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        const auto digit = static_cast<uint32_t>(*p - '0');
        if (digit > limit || value > (limit - digit) / 10) return false;
        value = value * 10 + digit;
    }
    return value != 0;
}
void status() {
    using namespace MedicationSystem;
    char row[200];
    std::snprintf(row, sizeof(row), "# time_ms=%lu state=%s",
                  static_cast<unsigned long>(nowMs), stateName(getState()));
    print(row);
    for (int c = 1; c <= COMPARTMENT_COUNT; ++c) {
        const auto& lid = *getCompartment(c);
        std::snprintf(row, sizeof(row),
            "# compartment=%d lid=%s session=%s verifying=%s outcome=%s scheduled_at_s=%lu",
            c, lid.lidOpen ? "open" : "closed", lid.inCurrentSession ? "yes" : "no",
            lid.verifying ? "yes" : "no",
            lid.hasDose ? outcomeName(lid.latestDose.outcome) : "NONE",
            static_cast<unsigned long>(lid.latestDose.scheduledAt));
        print(row);
    }
}
void command(char* text) {
    using namespace MedicationSystem;
    // Validate the entire command before changing any device state.
    char* tokens[6]{};
    unsigned count = 0;
    for (char* token = std::strtok(text, " \t"); token;
         token = std::strtok(nullptr, " \t")) {
        if (count == 6) { print("# error: too many arguments"); return; }
        tokens[count++] = token;
    }
    if (!count) return;
    if (count == 1 && std::strcmp(tokens[0], "help") == 0) { help(); return; }
    if (count == 1 && std::strcmp(tokens[0], "status") == 0) { status(); return; }

    if (std::strcmp(tokens[0], "dose") == 0 && count >= 2 && count <= 5) {
        bool due[COMPARTMENT_COUNT]{};
        for (unsigned i = 1; i < count; ++i) {
            uint32_t c;
            if (!number(tokens[i], COMPARTMENT_COUNT, c) || due[c - 1]) {
                print("# error: dose needs distinct compartments 1..4"); return;
            }
            due[c - 1] = true;
        }
        // Artificial time context, not a real date or an automatic scheduler.
        startSession(due, nowMs / 1000, nowMs);
        return;
    }
    if (count == 2 && std::strcmp(tokens[0], "refill") == 0) {
        if (std::strcmp(tokens[1], "start") == 0) { startRefill(); return; }
        if (std::strcmp(tokens[1], "finish") == 0) { finishRefill(); return; }
    }
    uint32_t value;
    if (count == 2 && std::strcmp(tokens[0], "advance") == 0 &&
        number(tokens[1], 600000U, value)) {
        // Visit intermediate times so a jump cannot skip dwell completion.
        for (uint32_t elapsed = 0; elapsed < value; ++elapsed) {
            ++nowMs;
            update(nowMs);
        }
        return;
    }
    if (count == 2 && number(tokens[1], COMPARTMENT_COUNT, value)) {
        if (std::strcmp(tokens[0], "open") == 0) {
            lidChanged(static_cast<int>(value), true, nowMs); return;
        }
        if (std::strcmp(tokens[0], "close") == 0) {
            lidChanged(static_cast<int>(value), false, nowMs); return;
        }
    }
    print("# error: invalid command; use help");
}
} // private parser and simulated clock

namespace MedicationSystem {
void report(const char* event, int compartment, uint32_t scheduledAt) {
    char row[180];
    std::snprintf(row, sizeof(row), "%lu,%s,%d,%lu,simulated,lid_only",
                  static_cast<unsigned long>(nowMs), event, compartment,
                  static_cast<unsigned long>(scheduledAt));
    print(row);
}
} // namespace MedicationSystem

namespace Simulation {
void begin(WriteLine output) {
    writeLine = output;
    nowMs = 0;
    used = 0;
    overflow = false;
    print("# SIMULATION ONLY: manual milliseconds; window=60000; minimum_open=2000");
    print("time_ms,event,compartment,scheduled_at_s,clock_source,evidence");
    const bool initialLids[MedicationSystem::COMPARTMENT_COUNT]{};
    MedicationSystem::initialize(initialLids, MedicationSystem::Settings{});
    help();
}
void feed(char byte) {
    if (byte == '\r') return;
    if (byte == '\n') {
        if (overflow) print("# error: line too long; discarded");
        else { buffer[used] = '\0'; command(buffer); }
        used = 0;
        overflow = false;
    } else if (!overflow && used + 1 < sizeof(buffer)) {
        buffer[used++] = byte;
    } else {
        overflow = true;
    }
}
} // namespace Simulation
