#include <SilenceBench.h>
#include <cassert>
#include <cstdio>

int main() {
    using namespace SilenceBench;
    Controller c;
    c.begin(true, 0);
    assert(!c.start(0));
    assert(c.sample(true, 30) == ButtonEvent::INITIAL_HELD);
    assert(!c.start(31));
    assert(c.sample(true, 1000) == ButtonEvent::NONE);
    assert(c.sample(false, 1001) == ButtonEvent::NONE);
    assert(c.sample(false, 1031) == ButtonEvent::RELEASED);
    assert(c.start(1100));
    assert(c.outputHigh(1100) && c.outputHigh(1349));
    assert(!c.outputHigh(1350) && !c.outputHigh(2099));
    assert(c.outputHigh(2100));
    assert(!c.start(2100)); // Cannot extend active alert.
    assert(c.sample(true, 2200) == ButtonEvent::NONE);
    assert(c.sample(false, 2210) == ButtonEvent::NONE);
    assert(c.sample(true, 2220) == ButtonEvent::NONE);
    assert(c.sample(true, 2249) == ButtonEvent::NONE);
    assert(c.sample(true, 2250) == ButtonEvent::SILENCED);
    assert(!c.active() && !c.outputHigh(2250));
    assert(c.sample(true, 20000) == ButtonEvent::NONE);
    assert(!c.start(20000)); // Held button cannot affect a new alert.
    c.sample(false, 20001);
    assert(c.sample(false, 20031) == ButtonEvent::RELEASED);
    c.sample(true, 20100);
    assert(c.sample(true, 20130) == ButtonEvent::PRESS_IDLE);
    c.sample(false, 20200);
    assert(c.sample(false, 20230) == ButtonEvent::RELEASED);
    assert(c.start(20300)); // Idle press did not pre-silence.
    assert(!c.update(30299));
    assert(c.active());
    assert(!c.outputHigh(30300));
    assert(c.update(30300) && !c.active());
    assert(c.start(30400));
    c.stop();
    assert(!c.outputHigh(30401));

    Controller wrap;
    wrap.begin(false, UINT32_MAX - 100);
    assert(wrap.sample(false, UINT32_MAX - 70) == ButtonEvent::READY);
    assert(wrap.start(UINT32_MAX - 50));
    assert(wrap.outputHigh(0));
    assert(!wrap.update(9948));
    assert(wrap.update(9949));
    std::puts("Silence bench checks passed (startup hold, debounce, silence, duty, timeout, no replay, rollover)");
}
