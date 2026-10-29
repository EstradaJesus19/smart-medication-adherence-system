#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cstring>

namespace {
constexpr int WIDTH = 128, HEIGHT = 64;
constexpr uint32_t BUS_HZ = 100000;
constexpr uint8_t CONTRAST = 0x7f;
constexpr uint32_t ALL_ON_MS = 2000;
constexpr int LED_PINS[] = {26, 27, 32, 33};
// Provisional controller/geometry, not identification from an I2C ACK.
Adafruit_SSD1306 display(WIDTH, HEIGHT, &Wire, -1, BUS_HZ, BUS_HZ);
enum class View { PATTERN, TEXT, CLEAR };
View view = View::PATTERN;
bool initialized = false;
bool screenOn = true;
bool lastAck = false;
bool allOn = false;
uint32_t allOnStartedMs = 0;
uint8_t address = 0;
uint32_t lastFrameMs = 0;
uint32_t frameDurationUs = 0;
char line[64]{};
unsigned used = 0;
bool overflow = false;

bool probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// Returns an address only if exactly one usual OLED address acknowledged.
uint8_t scan() {
    uint8_t candidate = 0;
    unsigned candidates = 0;
    Serial.println("# I2C scan (7-bit addresses), SDA21 SCL22, 100kHz");
    for (uint8_t addr = 8; addr < 120; ++addr) {
        Wire.beginTransmission(addr);
        const uint8_t result = Wire.endTransmission();
        if (result == 0) {
            Serial.printf("# ACK 0x%02X\n", addr);
            if (addr == 0x3c || addr == 0x3d) { candidate = addr; ++candidates; }
        } else if (result != 2) {
            Serial.printf("# I2C error %u at 0x%02X\n", result, addr);
        }
    }
    if (candidates != 1) {
        Serial.println("# Expected one OLED at 0x3C/0x3D; no automatic initialization");
        return 0;
    }
    Serial.println("# Address ACK is not controller identification");
    return candidate;
}

void render() {
    if (!initialized || !screenOn || allOn) return;
    lastAck = probe(address);
    if (!lastAck) { Serial.println("# OLED no ACK; frame skipped; check wiring with power OFF"); return; }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    if (view == View::PATTERN) {
        display.drawRect(0, 0, WIDTH, HEIGHT, SSD1306_WHITE);
        display.fillRect(2, 2, 3, 3, SSD1306_WHITE);
        display.fillRect(123, 2, 3, 3, SSD1306_WHITE);
        display.fillRect(2, 59, 3, 3, SSD1306_WHITE);
        display.fillRect(123, 59, 3, 3, SSD1306_WHITE);
        display.setCursor(8, 10); display.print("OLED TEST");
        display.setCursor(8, 21); display.print("128x64 SSD1306?");
        display.setCursor(8, 32); display.print("Seconds: "); display.print(millis() / 1000);
        display.setCursor(8, 45); display.print("I2C 0x"); display.print(address, HEX);
    } else if (view == View::TEXT) {
        display.setCursor(0, 0); display.print("Top: 0123456789");
        display.setCursor(0, 16); display.print("LEFT");
        display.setCursor(98, 16); display.print("RIGHT");
        display.setCursor(0, 32); display.print("ABCDEFGHIJKLMNOPQRSTU");
        display.setCursor(0, 44); display.print("abcdefghijklmnopqrstu");
        display.setCursor(0, 56); display.print("Bottom: 0123456789");
    }
    const uint32_t started = micros();
    display.display(); // Library does not provide per-frame I2C success status.
    frameDurationUs = micros() - started;
    lastFrameMs = millis();
}

void initDisplay(uint8_t addr) {
    initialized = false;
    if (!probe(addr)) { Serial.println("# OLED address did not ACK; initialization skipped"); return; }
    // Keep the already configured Wire bus and use the onboard charge pump.
    if (!display.begin(SSD1306_SWITCHCAPVCC, addr, false, false)) {
        Serial.println("# Display buffer allocation failed"); return;
    }
    address = addr;
    initialized = true;
    screenOn = true;
    allOn = false;
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(CONTRAST);
    view = View::PATTERN;
    render();
    Serial.printf("# Initialization commands sent to 0x%02X; provisional 128x64 SSD1306\n", addr);
    Serial.println("# Visually verify border, four corner squares, text and advancing counter");
}

void help() {
    Serial.println("# scan | init 0x3c | init 0x3d | pattern | text | clear | off | on | all-on | normal | status | help");
    Serial.println("# Contrast=0x7F; all-on lasts 2 seconds then restores pattern automatically");
    Serial.println("# Standalone OLED test only: no reminders, lid handling or RTC/EEPROM writes");
}

void restoreNormal() {
    allOn = false;
    screenOn = true;
    view = View::PATTERN;
    display.ssd1306_command(SSD1306_DISPLAYALLON_RESUME);
    display.ssd1306_command(SSD1306_NORMALDISPLAY);
    display.ssd1306_command(SSD1306_DISPLAYON);
    render();
    Serial.println("# Normal framebuffer mode restored; pattern sent");
}

void command() {
    if (!std::strcmp(line, "help")) { help(); return; }
    if (!std::strcmp(line, "scan")) { scan(); return; }
    if (!std::strcmp(line, "init 0x3c")) { initDisplay(0x3c); return; }
    if (!std::strcmp(line, "init 0x3d")) { initDisplay(0x3d); return; }
    if (!std::strcmp(line, "status")) {
        Serial.printf("# initialized=%s screen=%s address=0x%02X last_probe=%s frame_us=%lu\n",
            initialized ? "yes" : "no", screenOn ? "on" : "off", address,
            lastAck ? "ACK" : "no ACK", static_cast<unsigned long>(frameDurationUs));
        Serial.println("# frame_us is transfer duration, not proof of a correctly rendered display");
        Serial.printf("# mode=%s contrast=0x7F\n", allOn ? "ALL-ON (timed)" : "framebuffer");
        return;
    }
    if (!*line) return;
    if (!initialized) { Serial.println("# No initialized display; use scan/help"); return; }
    if (!std::strcmp(line, "all-on")) {
        if (allOn) { Serial.println("# All-on already active; timer not extended"); return; }
        if (!probe(address)) { Serial.println("# No OLED ACK; diagnostic skipped"); return; }
        display.ssd1306_command(SSD1306_NORMALDISPLAY);
        display.ssd1306_command(SSD1306_DISPLAYON);
        display.ssd1306_command(SSD1306_DISPLAYALLON);
        screenOn = true;
        allOn = true;
        allOnStartedMs = millis();
        Serial.println("# All pixels commanded ON for 2 seconds; observe screen now");
        return;
    }
    if (!std::strcmp(line, "normal")) { restoreNormal(); return; }
    // Ignore other display commands during the short diagnostic so they cannot
    // accidentally leave the panel in all-on mode or extend its duration.
    if (allOn && std::strcmp(line, "off")) {
        Serial.println("# Wait for timed restoration, or use normal/off"); return;
    }
    if (!std::strcmp(line, "off")) {
        allOn = false;
        display.ssd1306_command(SSD1306_DISPLAYALLON_RESUME);
        display.ssd1306_command(SSD1306_DISPLAYOFF); screenOn = false; return;
    }
    if (!std::strcmp(line, "on")) {
        display.ssd1306_command(SSD1306_DISPLAYON); screenOn = true; render(); return;
    }
    if (!std::strcmp(line, "pattern")) view = View::PATTERN;
    else if (!std::strcmp(line, "text")) view = View::TEXT;
    else if (!std::strcmp(line, "clear")) view = View::CLEAR;
    else { Serial.println("# Unknown command; use help"); return; }
    render();
}
} // namespace

void setup() {
    Serial.begin(115200);
    for (int pin : LED_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    Wire.begin(21, 22, BUS_HZ);
    Wire.setTimeOut(20);
    Serial.println("# OLED-only test; first test with RTC disconnected");
    help();
    const uint8_t candidate = scan();
    if (candidate) initDisplay(candidate);
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
    if (allOn && millis() - allOnStartedMs >= ALL_ON_MS) restoreNormal();
    if (initialized && screenOn && millis() - lastFrameMs >= 1000) {
        render();
        // Rate-limit retry/logging too when no device ACKs.
        lastFrameMs = millis();
    }
    yield();
}
