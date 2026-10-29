#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RtcOledCalendar.h>
#include <cstring>

namespace {
constexpr uint8_t OLED = 0x3c, RTC = 0x68;
constexpr uint32_t BUS_HZ = 100000;
constexpr int LED_PINS[] = {26, 27, 32, 33};
Adafruit_SSD1306 display(128, 64, &Wire, -1, BUS_HZ, BUS_HZ);
enum class Health { OK, INVALID, READ_ERROR };
bool displayReady = false;
bool oledAck = false;
Health health = Health::READ_ERROR;
bool osf = false;
RtcOledCalendar::DateTime date{};
uint32_t lastUpdateMs = 0, frameUs = 0;
char line[48]{};
unsigned used = 0;
bool overflow = false;

bool probe(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

Health readRtc() {
    // Register-address selection followed by reads only: no RTC data writes.
    Wire.beginTransmission(RTC);
    Wire.write(static_cast<uint8_t>(0));
    if (Wire.endTransmission(false) != 0) return Health::READ_ERROR;
    constexpr uint8_t count = 16; // Calendar through control/status (0x0F).
    if (Wire.requestFrom(RTC, count) != count) {
        while (Wire.available()) Wire.read();
        return Health::READ_ERROR;
    }
    uint8_t regs[count];
    for (uint8_t& value : regs) value = Wire.read();
    osf = regs[0x0f] & 0x80;
    if (!RtcOledCalendar::decode(regs, date) || osf) return Health::INVALID;
    return Health::OK;
}

void scan() {
    Serial.println("# I2C scan: OLED=0x3C RTC=0x68, SDA21/SCL22 100kHz");
    for (uint8_t address = 8; address < 120; ++address) {
        Wire.beginTransmission(address);
        const uint8_t error = Wire.endTransmission();
        if (!error) Serial.printf("# ACK 0x%02X\n", address);
        else if (error != 2) Serial.printf("# I2C error %u at 0x%02X\n", error, address);
    }
}

void initDisplay() {
    displayReady = false;
    oledAck = probe(OLED);
    if (!oledAck) { Serial.println("# OLED missing; RTC diagnostics continue on serial"); return; }
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED, false, false)) {
        Serial.println("# OLED buffer allocation failed"); return;
    }
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0x7f);
    display.ssd1306_command(SSD1306_DISPLAYALLON_RESUME);
    display.ssd1306_command(SSD1306_NORMALDISPLAY);
    displayReady = true;
}

void render() {
    frameUs = 0;
    oledAck = probe(OLED);
    if (!displayReady || !oledAck) return;
    display.clearDisplay(); // Never retain an old timestamp on RTC read failure.
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.setTextSize(1);
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.setCursor(4, 3); display.print("RTC + OLED TEST");
    if (health == Health::OK) {
        char text[24];
        snprintf(text, sizeof(text), "%04d-%02d-%02d", date.year, date.month, date.day);
        display.setCursor(4, 15); display.print(text);
        snprintf(text, sizeof(text), "%02d:%02d:%02d", date.hour, date.minute, date.second);
        display.setTextSize(2);
        display.setCursor(4, 28); display.print(text);
        display.setTextSize(1);
        display.setCursor(4, 49); display.print("RTC OK  OSF=0");
    } else {
        display.setCursor(4, 22);
        display.print(health == Health::READ_ERROR ? "RTC READ ERROR" : "RTC TIME INVALID");
        display.setCursor(4, 35);
        if (health == Health::READ_ERROR) display.print("Check bus/power");
        else { display.print("OSF="); display.print(osf ? 1 : 0); display.print(" / calendar"); }
        display.setCursor(4, 49); display.print("No trusted time");
    }
    const uint32_t started = micros();
    display.display(); // Library exposes no per-frame I2C success result.
    frameUs = micros() - started;
}

void update() {
    lastUpdateMs = millis();
    health = readRtc();
    render();
    if (health == Health::OK) {
        Serial.printf("%04d-%02d-%02d %02d:%02d:%02d OSF=0",
            date.year, date.month, date.day, date.hour, date.minute, date.second);
    } else if (health == Health::INVALID) {
        Serial.printf("# RTC TIME INVALID OSF=%u (or unsupported/invalid calendar)", osf ? 1 : 0);
    } else Serial.print("# RTC READ ERROR OSF=unknown");
    Serial.printf(" oled=%s frame_us=%lu\n", displayReady && oledAck ? "ACK/initialized" : "unavailable",
        static_cast<unsigned long>(frameUs));
}

void help() {
    Serial.println("# status | scan | init | help");
    Serial.println("# Read-only RTC calendar; literal time, no timezone conversion, no set command");
    Serial.println("# No medication logic. frame_us measures transfer duration, not visual correctness.");
}

void command() {
    if (!std::strcmp(line, "help")) help();
    else if (!std::strcmp(line, "status")) update();
    else if (!std::strcmp(line, "scan")) scan();
    else if (!std::strcmp(line, "init")) { initDisplay(); update(); }
    else if (*line) Serial.println("# Unknown command; use help");
}
} // namespace

void setup() {
    Serial.begin(115200);
    for (int pin : LED_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    Wire.begin(21, 22, BUS_HZ);
    Wire.setTimeOut(20);
    Serial.println("# Combined read-only RTC + OLED test, contrast=0x7F");
    help();
    scan();
    initDisplay();
    update();
}

void loop() {
    for (unsigned i = 0; i < 64 && Serial.available(); ++i) {
        const char byte = static_cast<char>(Serial.read());
        if (byte == '\r') continue;
        if (byte == '\n') {
            if (overflow) Serial.println("# Line too long; discarded");
            else { line[used] = '\0'; command(); }
            used = 0; overflow = false;
        } else if (!overflow && used + 1 < sizeof(line)) line[used++] = byte;
        else overflow = true;
    }
    if (millis() - lastUpdateMs >= 1000) update();
    yield();
}
