#pragma once
#include <RefillButtonControl.h>
#include <cstring>

// Audio follows core session events; input never changes medication state.
class SessionAlert {
public:
    void begin(bool pressed, uint32_t now) { rawPressed_ = pressed; button_.begin(pressed, now); stop(); }
    bool sample(bool pressed, uint32_t now) {
        rawPressed_ = pressed;
        const auto e = button_.sample(pressed, now);
        if (e == RefillButtonControl::Event::READY || e == RefillButtonControl::Event::RELEASED)
            eligible_ = true;
        if (e != RefillButtonControl::Event::PRESSED || !active_ || !eligible_) return false;
        muted_ = true;
        return true;
    }
    void event(const char* name, uint32_t now) {
        if (!std::strcmp(name, "SESSION_FINISHED")) stop();
        else if (!std::strcmp(name, "DOSE_STARTED") && !active_) {
            active_ = true;
            muted_ = false;
            startedAt_ = now;
            eligible_ = !rawPressed_ && !button_.pressed();
        }
    }
    void stop() { active_ = muted_ = eligible_ = false; }
    bool outputHigh(uint32_t now) const {
        return active_ && !muted_ && (uint32_t(now - startedAt_) % 1000 < 250);
    }
    bool active() const { return active_; }
    bool muted() const { return muted_; }
    bool pressed() const { return button_.pressed(); }
private:
    RefillButtonControl::Input button_;
    bool rawPressed_ = false, active_ = false, muted_ = false, eligible_ = false;
    uint32_t startedAt_ = 0;
};
