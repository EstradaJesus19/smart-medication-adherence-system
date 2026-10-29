#pragma once
#include <stdint.h>

namespace RtcOledCalendar {
struct DateTime { int year, month, day, hour, minute, second; };
inline int bcd(uint8_t value) {
    if ((value & 15) > 9 || (value >> 4) > 9) return -1;
    return (value >> 4) * 10 + (value & 15);
}
// Decode one coherent DS3231 calendar snapshot. OSF is checked by the caller.
// Restrict century to 2000..2099, matching the existing RTC experiment.
inline bool decode(const uint8_t* r, DateTime& out) {
    if ((r[0] & 0x80) || (r[1] & 0x80) || (r[2] & 0x80) ||
        (r[3] & 0xf8) || (r[4] & 0xc0) || (r[5] & 0xe0)) return false;
    DateTime t{2000 + bcd(r[6]), bcd(r[5]), bcd(r[4]),
               bcd(r[2] & 0x3f), bcd(r[1]), bcd(r[0])};
    if (r[2] & 0x40) {
        const int h = bcd(r[2] & 0x1f);
        if (h < 1 || h > 12) return false;
        t.hour = h % 12 + ((r[2] & 0x20) ? 12 : 0);
    }
    if (t.year < 2000 || t.year > 2099 || t.month < 1 || t.month > 12 ||
        t.hour < 0 || t.hour > 23 || t.minute < 0 || t.minute > 59 ||
        t.second < 0 || t.second > 59 || r[3] < 1 || r[3] > 7) return false;
    constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const int maxDay = days[t.month - 1] + (t.month == 2 && t.year % 4 == 0);
    if (t.day < 1 || t.day > maxDay) return false;
    out = t;
    return true;
}
} // namespace RtcOledCalendar
