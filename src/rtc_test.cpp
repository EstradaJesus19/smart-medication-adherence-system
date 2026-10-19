#include <Arduino.h>
#include <Wire.h>
#include <cstring>

namespace {
constexpr uint8_t RTC = 0x68;
constexpr int LED_PINS[] = {26, 27, 32, 33};
char line[80]{};
unsigned used = 0;
bool overflow = false;
uint32_t lastPrint = 0;

bool readRegisters(uint8_t reg, uint8_t* values, uint8_t count) {
    Wire.beginTransmission(RTC);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(RTC, count) != count) {
        Serial.println("# error: RTC I2C read failed");
        return false;
    }
    for (uint8_t i = 0; i < count; ++i) values[i] = Wire.read();
    return true;
}

bool writeRegisters(uint8_t reg, const uint8_t* values, uint8_t count) {
    Wire.beginTransmission(RTC);
    Wire.write(reg);
    Wire.write(values, count);
    if (Wire.endTransmission() != 0) {
        Serial.println("# error: RTC I2C write failed; time may be partially changed");
        return false;
    }
    return true;
}

int decode(uint8_t bcd) {
    if ((bcd & 15) > 9 || (bcd >> 4) > 9) return -1;
    return (bcd >> 4) * 10 + (bcd & 15);
}
uint8_t encode(int n) { return static_cast<uint8_t>((n / 10) * 16 + n % 10); }
int daysInMonth(int year, int month) {
    constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    return days[month - 1] + (month == 2 && year % 4 == 0);
}
bool validDate(int y, int m, int d, int h, int min, int s) {
    return y >= 2000 && y <= 2099 && m >= 1 && m <= 12 &&
        d >= 1 && d <= daysInMonth(y, m) && h >= 0 && h <= 23 &&
        min >= 0 && min <= 59 && s >= 0 && s <= 59;
}

void showTime() {
    uint8_t regs[7], status;
    if (!readRegisters(0, regs, 7) || !readRegisters(0x0f, &status, 1)) return;
    const int second = decode(regs[0]);
    const int minute = decode(regs[1]);
    int hour;
    if (regs[2] & 0x40) {
        const int twelve = decode(regs[2] & 0x1f);
        hour = twelve >= 1 && twelve <= 12 ?
            twelve % 12 + ((regs[2] & 0x20) ? 12 : 0) : -1;
    } else hour = decode(regs[2] & 0x3f);
    const int day = decode(regs[4]);
    const int month = decode(regs[5] & 0x1f);
    const int year = 2000 + decode(regs[6]);
    const bool valid = !(regs[5] & 0x80) &&
        validDate(year, month, day, hour, minute, second) &&
        regs[3] >= 1 && regs[3] <= 7;
    if (!valid) {
        Serial.printf("# invalid/unsupported calendar registers; OSF=%u; use set (2000..2099)\n",
                      (status >> 7) & 1);
        return;
    }
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d OSF=%u time=%s\n",
        year, month, day, hour, minute, second, (status >> 7) & 1,
        (status & 0x80) ? "UNTRUSTED: oscillator stopped" : "plausible; compare with reference");
}

void scan() {
    bool found = false;
    Serial.println("# I2C scan; SDA=21 SCL=22 100kHz");
    for (uint8_t address = 8; address < 120; ++address) {
        Wire.beginTransmission(address);
        const uint8_t result = Wire.endTransmission();
        if (result == 0) {
            Serial.printf("# ACK 0x%02X%s\n", address, address == RTC ? " (expected RTC)" : "");
            if (address == RTC) found = true;
        } else if (result != 2) {
            Serial.printf("# I2C error %u at 0x%02X\n", result, address);
        }
    }
    if (!found) Serial.println("# RTC not detected at 0x68");
}
void help() {
    Serial.println("# scan | status | help | set YYYY-MM-DD HH:MM:SS");
    Serial.println("# Literal bench time, no timezone/DST conversion; years 2000..2099");
    Serial.println("# Battery removed. No automatic time setting, scheduling or EEPROM writes.");
}

void setTime(const char* text) {
    // Exact grammar; reject the entire command before any I2C write.
    if (std::strlen(text) != 19 || text[4] != '-' || text[7] != '-' ||
        text[10] != ' ' || text[13] != ':' || text[16] != ':') {
        Serial.println("# error: use set YYYY-MM-DD HH:MM:SS"); return;
    }
    for (int i = 0; i < 19; ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (text[i] < '0' || text[i] > '9') {
            Serial.println("# error: date/time must contain digits"); return;
        }
    }
    auto pair = [text](int i) { return (text[i] - '0') * 10 + text[i + 1] - '0'; };
    const int year = pair(0) * 100 + pair(2), month = pair(5), day = pair(8);
    const int hour = pair(11), minute = pair(14), second = pair(17);
    if (!validDate(year, month, day, hour, minute, second)) {
        Serial.println("# error: invalid date/time or year outside 2000..2099"); return;
    }
    // Monday=1..Sunday=7. 2000-01-01 was Saturday.
    int elapsed = 0;
    for (int y = 2000; y < year; ++y) elapsed += y % 4 == 0 ? 366 : 365;
    for (int m = 1; m < month; ++m) elapsed += daysInMonth(year, m);
    elapsed += day - 1;
    uint8_t control, status;
    if (!readRegisters(0x0e, &control, 1) || !readRegisters(0x0f, &status, 1)) return;
    control &= 0x7f; // Enable oscillator; preserve other control settings.
    if (!writeRegisters(0x0e, &control, 1)) return;
    const uint8_t regs[] = {encode(second), encode(minute), encode(hour),
        static_cast<uint8_t>((elapsed + 5) % 7 + 1), encode(day), encode(month), encode(year - 2000)};
    if (!writeRegisters(0, regs, 7)) return;
    // OSF is cleared only after an explicit successful time write.
    // Writing ones to alarm flags preserves them, including newly arrived flags.
    status = (status & 0x7f) | 0x03;
    if (!writeRegisters(0x0f, &status, 1)) return;
    Serial.println("# Time written; checking readback (not proof of accuracy)");
    showTime();
}
void command() {
    if (!std::strcmp(line, "help")) help();
    else if (!std::strcmp(line, "scan")) scan();
    else if (!std::strcmp(line, "status")) showTime();
    else if (!std::strncmp(line, "set ", 4)) setTime(line + 4);
    else if (*line) Serial.println("# error: unknown command; use help");
}
} // namespace

void setup() {
    Serial.begin(115200);
    // Existing indicators may remain wired; keep them off in this RTC-only test.
    for (int pin : LED_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    Wire.begin(21, 22, 100000);
    Wire.setTimeOut(50);
    Serial.println("# Battery-free DS3231 test; startup does not write RTC registers");
    help();
    scan();
    showTime();
    lastPrint = millis();
}
void loop() {
    for (unsigned i = 0; i < 64 && Serial.available(); ++i) {
        const char byte = static_cast<char>(Serial.read());
        if (byte == '\r') continue;
        if (byte == '\n') {
            if (overflow) Serial.println("# error: line too long; discarded");
            else { line[used] = '\0'; command(); }
            used = 0; overflow = false;
        } else if (!overflow && used + 1 < sizeof(line)) line[used++] = byte;
        else overflow = true;
    }
    const uint32_t now = millis();
    if (now - lastPrint >= 1000) { lastPrint = now; showTime(); }
    yield();
}
