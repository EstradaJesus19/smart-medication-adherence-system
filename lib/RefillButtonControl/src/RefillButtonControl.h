#pragma once
#include <stdint.h>
#include <MedicationSystem.h>

namespace RefillButtonControl {
enum class Event { NONE, READY, INITIAL_HELD, RELEASED, PRESSED };

class Input {
public:
    void begin(bool pressed, uint32_t now) {
        raw_ = pressed;
        stable_ = false;
        initialized_ = enabled_ = false;
        changedAt_ = now;
    }
    Event sample(bool pressed, uint32_t now) {
        if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
        if (now - changedAt_ < 30) return Event::NONE;
        if (!initialized_) {
            initialized_ = true;
            stable_ = raw_;
            enabled_ = !stable_;
            return stable_ ? Event::INITIAL_HELD : Event::READY;
        }
        if (stable_ == raw_) return Event::NONE;
        stable_ = raw_;
        if (!stable_) { enabled_ = true; return Event::RELEASED; }
        return enabled_ ? Event::PRESSED : Event::NONE;
    }
    bool pressed() const { return initialized_ && stable_; }
private:
    bool raw_ = false, stable_ = false, initialized_ = false, enabled_ = false;
    uint32_t changedAt_ = 0;
};

// Call only for a newly accepted press after initialization and due-session
// processing. Rejection never queues an action for a later state.
inline bool applyPress() {
    if (MedicationSystem::getState() == MedicationSystem::SystemState::REFILL)
        return MedicationSystem::finishRefill();
    return MedicationSystem::startRefill();
}
} // namespace RefillButtonControl
