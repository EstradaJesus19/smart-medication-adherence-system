#include <RtcOledCalendar.h>
#include <cassert>
#include <cstdio>

int main() {
    uint8_t r[7] = {0x59, 0x58, 0x23, 5, 0x23, 0x10, 0x26};
    RtcOledCalendar::DateTime d{};
    assert(RtcOledCalendar::decode(r, d));
    assert(d.year == 2026 && d.month == 10 && d.day == 23 && d.hour == 23 && d.second == 59);
    r[2] = 0x52; // 12 AM
    assert(RtcOledCalendar::decode(r, d) && d.hour == 0);
    r[2] = 0x72; // 12 PM
    assert(RtcOledCalendar::decode(r, d) && d.hour == 12);
    r[2] = 0x61; // 1 PM
    assert(RtcOledCalendar::decode(r, d) && d.hour == 13);
    r[2] = 0x40; assert(!RtcOledCalendar::decode(r, d));
    r[2] = 0x24; assert(!RtcOledCalendar::decode(r, d));
    r[2] = 0x23;
    r[0] = 0x6a; assert(!RtcOledCalendar::decode(r, d));
    r[0] = 0x60; assert(!RtcOledCalendar::decode(r, d));
    r[0] = 0x59;
    r[5] = 0x82; assert(!RtcOledCalendar::decode(r, d)); // unsupported century
    r[5] = 0x02; r[4] = 0x29;
    assert(!RtcOledCalendar::decode(r, d)); // 2026 not leap
    r[6] = 0x24; assert(RtcOledCalendar::decode(r, d));
    r[4] = 0x30; assert(!RtcOledCalendar::decode(r, d));
    r[4] = 0x29; r[6] = 0xff; assert(!RtcOledCalendar::decode(r, d));
    r[6] = 0x24; r[3] = 0; assert(!RtcOledCalendar::decode(r, d));
    r[3] = 5; r[5] = 0; assert(!RtcOledCalendar::decode(r, d));
    r[5] = 0x02; r[0] = 0x80; assert(!RtcOledCalendar::decode(r, d));
    std::puts("RTC OLED calendar checks passed (BCD, 12/24h, leap dates, range, reserved bits)");
}
