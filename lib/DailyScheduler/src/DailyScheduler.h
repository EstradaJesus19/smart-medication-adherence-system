#pragma once
#include <stdint.h>

// Bench scheduler: seconds since 2000-01-01, literal RTC time, no DST conversion.
class DailyScheduler {
public:
    static constexpr unsigned CAPACITY = 8;
    struct Entry { uint32_t secondOfDay; uint8_t mask; };
    enum class Result { NONE, DUE, LATE_SKIPPED, RTC_FAULT, CLOCK_JUMP, POLL_GAP };
    struct Event { Result result; uint32_t scheduledAt; uint8_t mask; };
    bool add(uint32_t time, uint8_t mask) {
        if (armed_ || time >= DAY || !mask || (mask & 0xf0)) return false;
        for (unsigned i = 0; i < count_; ++i) {
            if (entries_[i].secondOfDay == time) {
                entries_[i].mask |= mask;
                return true;
            }
            const uint32_t delta = time > entries_[i].secondOfDay ?
                time - entries_[i].secondOfDay : entries_[i].secondOfDay - time;
            if (delta < WINDOW || DAY - delta < WINDOW) return false;
        }
        if (count_ == CAPACITY) return false;
        entries_[count_++] = {time, mask};
        return true;
    }
    bool clear() {
        if (armed_) return false;
        count_ = 0; // Preserve consumed occurrence watermark across editing/rearming.
        return true;
    }
    bool arm(uint32_t seconds, uint32_t ms, bool healthy) {
        if (armed_ || !healthy || !count_) return false;
        previous_ = anchor_ = seconds;
        previousMs_ = anchorMs_ = ms;
        armed_ = true;
        return true; // Never replay an occurrence at/before arm time.
    }
    void disarm() { armed_ = false; }
    bool armed() const { return armed_; }
    unsigned count() const { return count_; }
    const Entry& entry(unsigned i) const { return entries_[i]; }
    Event observe(uint32_t seconds, uint32_t ms, bool healthy) {
        if (!armed_) return {Result::NONE, 0, 0};
        if (!healthy) return stop(Result::RTC_FAULT);
        const uint32_t gap = ms - previousMs_;
        if (gap > 2000) return stop(Result::POLL_GAP);
        const int64_t driftMs = (static_cast<int64_t>(seconds) - anchor_) * 1000 -
                                static_cast<int64_t>(ms - anchorMs_);
        if (seconds < previous_ || driftMs > 2000 || driftMs < -2000)
            return stop(Result::CLOCK_JUMP);
        Event event{Result::NONE, 0, 0};
        // A healthy <=2 second observation interval crosses at most one entry:
        // distinct entries are at least 60 seconds apart, including midnight.
        for (unsigned i = 0; i < count_; ++i) {
            uint64_t occurrence = static_cast<uint64_t>(previous_ / DAY) * DAY + entries_[i].secondOfDay;
            if (occurrence <= previous_) occurrence += DAY;
            if (occurrence > seconds || occurrence <= consumed_) continue;
            consumed_ = static_cast<uint32_t>(occurrence);
            event = {occurrence == seconds ? Result::DUE : Result::LATE_SKIPPED,
                     static_cast<uint32_t>(occurrence), entries_[i].mask};
        }
        previous_ = seconds;
        previousMs_ = ms;
        if (ms - anchorMs_ >= 60000) { anchor_ = seconds; anchorMs_ = ms; }
        return event;
    }
private:
    // One second of guard covers RTC-second quantization and polling phase.
    static constexpr uint32_t DAY = 86400, WINDOW = 61;
    Entry entries_[CAPACITY]{};
    unsigned count_ = 0;
    bool armed_ = false;
    uint32_t previous_ = 0, previousMs_ = 0, anchor_ = 0, anchorMs_ = 0, consumed_ = 0;
    Event stop(Result result) { armed_ = false; return {result, 0, 0}; }
};
