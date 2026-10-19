#include <DailyScheduler.h>
#include <MedicationSystem.h>
#include <cassert>
#include <cstdio>
#include <cstdint>

using R = DailyScheduler::Result;
namespace MedicationSystem {
void report(const char*, int, uint32_t) {}
}
int main() {
    DailyScheduler s;
    assert(!s.arm(0, 0, true));
    assert(!s.add(86400, 1) && !s.add(100, 0) && !s.add(100, 16));
    assert(s.add(100, 1) && s.add(100, 4));
    assert(s.count() == 1 && s.entry(0).mask == 5);
    assert(!s.add(159, 2) && s.count() == 1);
    assert(!s.add(160, 2)); // Quantization guard at touching windows.
    assert(s.add(161, 2));
    assert(!s.arm(99, 0, false));
    assert(s.arm(99, 0, true));
    assert(!s.add(300, 1) && !s.clear());
    auto e = s.observe(100, 1000, true);
    assert(e.result == R::DUE && e.mask == 5 && e.scheduledAt == 100);
    assert(s.observe(100, 1200, true).result == R::NONE);
    s.disarm();
    assert(s.arm(99, 1300, true));
    assert(s.observe(100, 2300, true).result == R::NONE); // No replay on rearm/rollback.
    s.disarm();
    assert(s.clear() && s.add(100, 2) && s.arm(99, 2400, true));
    assert(s.observe(100, 3400, true).result == R::NONE); // Editing doesn't erase watermark.

    DailyScheduler late;
    assert(late.add(100, 1) && late.arm(99, 0, true));
    assert(late.observe(101, 2000, true).result == R::LATE_SKIPPED);
    assert(late.observe(101, 2200, true).result == R::NONE);

    DailyScheduler midnight;
    assert(midnight.add(86380, 1));
    assert(!midnight.add(10, 2)); // Overlap across midnight.
    assert(!midnight.add(40, 2));
    assert(midnight.add(41, 2));
    assert(midnight.arm(86399, 0, true));
    assert(midnight.observe(86400, 1000, true).result == R::NONE);
    // Re-arm closer to next occurrence; no historical replay.
    midnight.disarm();
    assert(midnight.arm(86440, 2000, true));
    assert(midnight.observe(86441, 3000, true).result == R::DUE);
    midnight.disarm();
    assert(midnight.arm(172840, 4000, true));
    assert(midnight.observe(172841, 5000, true).result == R::DUE); // Next day.

    DailyScheduler faults;
    assert(faults.add(100, 1) && faults.arm(99, 0, true));
    assert(faults.observe(100, 1000, false).result == R::RTC_FAULT && !faults.armed());
    assert(faults.observe(100, 1100, true).result == R::NONE && !faults.armed());
    assert(faults.arm(99, 1200, true));
    assert(faults.observe(98, 1400, true).result == R::CLOCK_JUMP && !faults.armed());
    assert(faults.arm(99, 1500, true));
    assert(faults.observe(109, 1700, true).result == R::CLOCK_JUMP);
    assert(faults.arm(99, 1800, true));
    assert(faults.observe(102, 3900, true).result == R::POLL_GAP);

    DailyScheduler stopped;
    assert(stopped.add(100, 1) && stopped.arm(90, 0, true));
    for (uint32_t ms = 200; ms <= 2000; ms += 200)
        assert(stopped.observe(90, ms, true).result == R::NONE);
    assert(stopped.observe(90, 2200, true).result == R::CLOCK_JUMP);

    DailyScheduler wrap;
    assert(wrap.add(100, 1) && wrap.arm(99, UINT32_MAX - 499, true));
    assert(wrap.observe(100, 500, true).result == R::DUE);

    DailyScheduler full;
    for (unsigned i = 0; i < 8; ++i) assert(full.add(i * 100, 1));
    assert(!full.add(800, 1) && full.count() == 8);
    assert(full.add(0, 2) && full.count() == 8);
    assert(full.arm(0, 0, true));
    assert(full.observe(0, 200, true).result == R::NONE); // At arm time not triggered.

    // Scheduler disarming cannot cancel or extend a live medication session.
    const bool closed[4]{};
    assert(MedicationSystem::initialize(closed, MedicationSystem::Settings{}));
    assert(MedicationSystem::startRefill());
    DailyScheduler integrated;
    assert(integrated.add(100, 5) && integrated.arm(99, 0, true));
    e = integrated.observe(100, 1000, true);
    assert(e.result == R::DUE);
    const bool due[4] = {true, false, true, false};
    assert(MedicationSystem::startSession(due, e.scheduledAt, 1000));
    assert(MedicationSystem::getState() == MedicationSystem::SystemState::REMINDER);
    assert(integrated.observe(101, 2000, false).result == R::RTC_FAULT);
    MedicationSystem::lidChanged(1, true, 2100);
    MedicationSystem::update(4100);
    assert(MedicationSystem::getCompartment(1)->latestDose.outcome == MedicationSystem::DoseOutcome::ACCESS_CONFIRMED);
    MedicationSystem::lidChanged(1, false, 4200);
    MedicationSystem::update(61000);
    assert(MedicationSystem::getCompartment(3)->latestDose.outcome == MedicationSystem::DoseOutcome::MISSED);
    assert(MedicationSystem::getState() == MedicationSystem::SystemState::IDLE);
    std::puts("Scheduler checks passed (grouping, overlap, timing, faults, replay, midnight, rollover)");
}
