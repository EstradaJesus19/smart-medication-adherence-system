#include <RefillButtonControl.h>
#include <DailyScheduler.h>
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
unsigned buttonRejects = 0, repeatedWarnings = 0;
}
namespace MedicationSystem {
void report(const char* event, int, uint32_t) {
    if (!std::strcmp(event, "REFILL_START_REJECTED")) ++buttonRejects;
    if (!std::strcmp(event, "REPEATED_ACCESS")) ++repeatedWarnings;
}
}
int main() {
    using namespace RefillButtonControl;
    using namespace MedicationSystem;
    Input input;
    input.begin(true, 0);
    assert(input.sample(true, 29) == Event::NONE);
    assert(input.sample(true, 30) == Event::INITIAL_HELD);
    assert(input.sample(true, 10000) == Event::NONE);
    assert(input.sample(false, 10001) == Event::NONE);
    assert(input.sample(false, 10031) == Event::RELEASED);
    assert(input.sample(true, 10040) == Event::NONE);
    assert(input.sample(false, 10050) == Event::NONE); // bounce restarts debounce
    assert(input.sample(true, 10060) == Event::NONE);
    assert(input.sample(true, 10089) == Event::NONE);
    assert(input.sample(true, 10090) == Event::PRESSED);
    assert(input.sample(true, 20000) == Event::NONE);

    Input wrap;
    wrap.begin(false, UINT32_MAX - 20);
    assert(wrap.sample(false, 9) == Event::READY);
    assert(wrap.sample(true, 10) == Event::NONE);
    assert(wrap.sample(true, 40) == Event::PRESSED);

    const bool closed[4]{};
    assert(initialize(closed, Settings{}));
    assert(applyPress() && getState() == SystemState::REFILL);
    lidChanged(1, true, 0);
    lidChanged(3, true, 0);
    assert(!applyPress() && getState() == SystemState::REFILL);
    lidChanged(1, false, 100);
    assert(!applyPress()); // all lids, not just the first
    lidChanged(3, false, 100);
    assert(getState() == SystemState::REFILL); // closing alone never finishes
    assert(applyPress() && getState() == SystemState::IDLE);
    lidChanged(2, true, 200);
    assert(!applyPress() && getState() == SystemState::WAITING_FOR_CLOSURE);
    lidChanged(2, false, 300);

    // Due session is processed before a simultaneous accepted button action.
    assert(applyPress());
    DailyScheduler schedule;
    assert(schedule.add(100, 1) && schedule.arm(99, 0, true));
    const auto dueEvent = schedule.observe(100, 1000, true);
    assert(dueEvent.result == DailyScheduler::Result::DUE);
    const bool due[4] = {true, false, false, false};
    assert(startSession(due, dueEvent.scheduledAt, 1000));
    assert(!applyPress() && getState() == SystemState::REMINDER);
    lidChanged(1, true, 1100);
    update(3100);
    assert(getState() == SystemState::WAITING_FOR_CLOSURE);
    assert(!applyPress());
    lidChanged(1, false, 3200);
    assert(getState() == SystemState::IDLE);
    // No new input edge: a held press cannot restart refill after the reminder.
    assert(input.sample(true, 30000) == Event::NONE);
    assert(getState() == SystemState::IDLE);

    // A refill cycle preserves the previous confirmation/repeated-access rule.
    assert(applyPress());
    lidChanged(1, true, 3300);
    assert(repeatedWarnings == 0);
    assert(!applyPress());
    lidChanged(1, false, 3400);
    assert(applyPress());
    lidChanged(1, true, 3500);
    assert(repeatedWarnings == 1);
    lidChanged(1, false, 3600);
    assert(buttonRejects == 3);

    // Rejected presses cannot reset the active deadline or pending outcome.
    assert(startSession(due, 200, 10000));
    assert(!applyPress());
    update(70000);
    assert(getCompartment(1)->latestDose.outcome == DoseOutcome::MISSED);
    assert(getState() == SystemState::IDLE);
    std::puts("Refill button checks passed (debounce, startup hold, rollover, closure, due priority, history, deadline)");
}
