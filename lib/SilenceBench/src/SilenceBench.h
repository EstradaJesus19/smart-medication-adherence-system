#pragma once
#include <stdint.h>

// Standalone bench controller, not the eventual medication alert policy.
namespace SilenceBench {
enum class ButtonEvent { NONE, READY, INITIAL_HELD, RELEASED, SILENCED, PRESS_IDLE };
class Controller {
public:
    void begin(bool pressed, uint32_t now) {
        raw_ = pressed;
        stable_ = initialized_ = active_ = false;
        changedAt_ = now;
    }
    ButtonEvent sample(bool pressed, uint32_t now) {
        if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
        if (now - changedAt_ < 30) return ButtonEvent::NONE;
        if (!initialized_) {
            initialized_ = true;
            stable_ = raw_;
            return stable_ ? ButtonEvent::INITIAL_HELD : ButtonEvent::READY;
        }
        if (stable_ == raw_) return ButtonEvent::NONE;
        stable_ = raw_;
        if (!stable_) return ButtonEvent::RELEASED;
        if (!active_) return ButtonEvent::PRESS_IDLE;
        stop();
        return ButtonEvent::SILENCED;
    }
    bool start(uint32_t now) {
        // Held, bouncing or initializing input cannot pre-silence a new alert.
        if (active_ || !initialized_ || stable_ || raw_) return false;
        startedAt_ = now;
        active_ = true;
        return true;
    }
    void stop() { active_ = false; }
    bool update(uint32_t now) {
        if (active_ && now - startedAt_ >= 10000) { stop(); return true; }
        return false;
    }
    bool outputHigh(uint32_t now) const {
        const uint32_t elapsed = now - startedAt_;
        return active_ && elapsed < 10000 && elapsed % 1000 < 250;
    }
    bool active() const { return active_; }
    bool ready() const { return initialized_; }
    bool pressed() const { return initialized_ && stable_; }
private:
    bool raw_ = false, stable_ = false, initialized_ = false, active_ = false;
    uint32_t changedAt_ = 0, startedAt_ = 0;
};
} // namespace SilenceBench
