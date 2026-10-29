#include <SessionAlert.h>
#include <cassert>
#include <cstdio>
SessionAlert audio;
uint32_t now;
namespace MedicationSystem {
void report(const char* event, int, uint32_t) { audio.event(event, now); }
}
int main() {
    using namespace MedicationSystem;
    const bool closed[4]{}, due[4]{true,false,true,false};
    audio.begin(false,0); audio.sample(false,30);
    assert(initialize(closed,Settings{}));
    now=100; assert(startSession(due,1,now));
    assert(audio.outputHigh(100) && !audio.outputHigh(350) && audio.outputHigh(1100));
    audio.sample(true,1200); assert(audio.sample(true,1230));
    assert(audio.active() && audio.muted() && !audio.outputHigh(2100));
    assert(getCompartment(1)->latestDose.outcome==DoseOutcome::PENDING);
    now=2000; lidChanged(1,true,now); now=4000; update(now);
    assert(getCompartment(1)->latestDose.outcome==DoseOutcome::ACCESS_CONFIRMED);
    assert(audio.active() && audio.muted());
    now=60099; update(now); assert(audio.active());
    now=60100; update(now);
    assert(!audio.active() && getCompartment(3)->latestDose.outcome==DoseOutcome::MISSED);
    assert(getState()==SystemState::WAITING_FOR_CLOSURE);
    now=60101; lidChanged(1,false,now);
    now=61000; assert(startSession(due,2,now)); // Still held: audible, cannot pre-mute.
    assert(audio.outputHigh(now)); assert(!audio.sample(true,61030));
    audio.sample(false,61100); audio.sample(false,61130);
    audio.sample(true,61200); assert(audio.sample(true,61230));
    now=62000; lidChanged(1,true,now); lidChanged(3,true,now);
    now=64000; update(now); assert(!audio.active());
    now=64001; lidChanged(1,false,now); lidChanged(3,false,now);
    audio.sample(false,65000); audio.sample(false,65030);
    audio.sample(true,65100); assert(!audio.sample(true,65130)); // Idle press.
    audio.sample(false,65200); audio.sample(false,65230);
    assert(startRefill());
    now=66000; assert(startSession(due,3,now)); assert(audio.outputHigh(now));
    assert(!RefillButtonControl::applyPress()); assert(audio.active());
    now=68000; lidChanged(1,true,now); now=70000; update(now);
    assert(audio.active() && !audio.muted()); // Partial confirmation keeps sound.
    now=126000; update(now); assert(!audio.active());
    SessionAlert wrap;
    wrap.begin(false,UINT32_MAX-100); wrap.sample(false,UINT32_MAX-70);
    wrap.event("DOSE_STARTED",UINT32_MAX-50); assert(wrap.outputHigh(0));
    assert(!wrap.outputHigh(200)); wrap.event("SESSION_FINISHED",201); assert(!wrap.outputHigh(1000));
    SessionAlert overlap;
    overlap.begin(false,0); overlap.sample(false,30);
    overlap.sample(true,50); overlap.event("DOSE_STARTED",60);
    assert(!overlap.sample(true,80) && overlap.outputHigh(80)); // Press began before due.
    overlap.sample(false,90); overlap.sample(false,120);
    overlap.sample(true,130); assert(overlap.sample(true,160));
    overlap.event("DOSE_STARTED",170); assert(overlap.muted()); // Grouped event cannot reset mute.
    std::puts("Session audio checks passed: core outcomes/deadline, partial/all finish, refill, held/idle input, debounce and rollover");
}
