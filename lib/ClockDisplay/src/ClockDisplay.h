#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ClockDisplay {
struct Calendar { unsigned year, month, day, hour, minute, second; };
inline bool calendar(uint32_t seconds, Calendar& out) {
    // Literal DS3231 calendar range, seconds since 2000-01-01 (not Unix).
    if (seconds >= 3155760000UL) return false; // 2100-01-01
    uint32_t days = seconds / 86400;
    out.hour = seconds / 3600 % 24; out.minute = seconds / 60 % 60; out.second = seconds % 60;
    out.year = 2000;
    while (days >= (out.year % 4 == 0 ? 366U : 365U))
        days -= out.year++ % 4 == 0 ? 366U : 365U;
    constexpr unsigned lengths[]{31,28,31,30,31,30,31,31,30,31,30,31};
    out.month = 1;
    for (;;) {
        const unsigned length = lengths[out.month - 1] + (out.month == 2 && out.year % 4 == 0);
        if (days < length) break;
        days -= length; ++out.month;
    }
    out.day = days + 1;
    return true;
}
inline bool trusted(bool healthy, uint32_t sampledAt, uint32_t now) {
    return healthy && uint32_t(now - sampledAt) <= 1000;
}

// Caller owns a frozen, SSD1306 page-layout 1024-byte buffer until idle().
// One service() call performs at most ONE bounded I2C transaction.
class Transfer {
public:
    static constexpr size_t FRAME_BYTES = 1024, CHUNK_BYTES = 16;
    enum class State { INIT_A, INIT_B, WINDOW, DATA, ON, IDLE, FAILED };
    void begin() { state_ = State::INIT_A; offset_ = 0; initial_ = true; frames_ = 0; }
    bool request() {
        if (!idle()) return false;
        state_ = State::WINDOW; offset_ = 0; return true;
    }
    template<class Writer> void service(const uint8_t* frame, Writer write) {
        static constexpr uint8_t a[]{0xae,0xd5,0x80,0xa8,0x3f,0xd3,0x00,0x40,0x8d,0x14,0x20,0x00,0xa1,0xc8};
        static constexpr uint8_t b[]{0xda,0x12,0x81,0x7f,0xd9,0xf1,0xdb,0x40,0xa4,0xa6,0x2e};
        static constexpr uint8_t window[]{0x21,0,127,0x22,0,7};
        static constexpr uint8_t on[]{0xaf};
        bool ok = true;
        switch (state_) {
        case State::INIT_A: ok = write(0,a,sizeof(a)); if (ok) state_=State::INIT_B; break;
        case State::INIT_B: ok = write(0,b,sizeof(b)); if (ok) state_=State::WINDOW; break;
        case State::WINDOW: ok = write(0,window,sizeof(window)); if (ok) state_=State::DATA; break;
        case State::DATA:
            ok = write(0x40,frame+offset_,CHUNK_BYTES);
            if (ok) {
                offset_ += CHUNK_BYTES;
                if (offset_ == FRAME_BYTES) {
                    ++frames_;
                    state_ = initial_ ? State::ON : State::IDLE;
                }
            }
            break;
        case State::ON: ok = write(0,on,sizeof(on)); if (ok) { initial_=false; state_=State::IDLE; } break;
        case State::IDLE: case State::FAILED: return;
        }
        if (!ok) state_=State::FAILED;
    }
    bool idle() const { return state_ == State::IDLE; }
    bool failed() const { return state_ == State::FAILED; }
    uint32_t frames() const { return frames_; }
    State state() const { return state_; }
private:
    State state_ = State::FAILED;
    size_t offset_ = 0;
    bool initial_ = true;
    uint32_t frames_ = 0;
};
} // namespace ClockDisplay
