#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <ClockDisplay.h>
#include <cstring>

// Drawing is local RAM only. Never use display.display() in this adapter.
class ClockCanvas : public Adafruit_GFX {
public:
    ClockCanvas() : Adafruit_GFX(128,64) {}
    void drawPixel(int16_t x,int16_t y,uint16_t color) override {
        if (x<0 || x>=128 || y<0 || y>=64) return;
        const unsigned i = x + (y/8)*128;
        const uint8_t mask = 1U << (y%8);
        if (color) pixels[i] |= mask; else pixels[i] &= ~mask;
    }
    uint8_t pixels[1024]{};
};
class IncrementalClockOled {
public:
    void begin(bool healthy,uint32_t seconds) {
        draw(healthy,seconds);
        lastHealthy_=healthy; lastSeconds_=seconds;
        transfer_.begin(); faultReported_=false; injectFailure_=false;
    }
    void service(bool healthy,uint32_t seconds) {
        if (transfer_.idle() && (healthy!=lastHealthy_ || (healthy && seconds!=lastSeconds_))) {
            draw(healthy,seconds); lastHealthy_=healthy; lastSeconds_=seconds;
            transfer_.request();
        }
        transfer_.service(canvas_.pixels,[this](uint8_t control,const uint8_t* data,size_t size) {
            const uint32_t started = micros();
            bool ok = false;
            if (injectFailure_) injectFailure_=false;
            else {
                Wire.beginTransmission(0x3c);
                const bool queued = Wire.write(control)==1 && Wire.write(data,size)==size;
                const uint8_t result = Wire.endTransmission();
                ok = queued && result==0;
            }
            const uint32_t elapsed = micros()-started;
            if (elapsed>maxTransactionUs_) maxTransactionUs_=elapsed;
            return ok;
        });
        if (transfer_.failed() && !faultReported_) {
            faultReported_=true;
            Serial.println("# OLED_FAULT: updates stopped; image may be stale; retry only DISARMED/IDLE");
        }
    }
    void injectFailure() {
        injectFailure_=true;
        if (transfer_.idle()) transfer_.request();
    }
    void status() const {
        Serial.printf("# oled=%s frames=%lu max_oled_tx_us=%lu contrast=0x7F chunk=16\n",
            transfer_.failed()?"FAULT":transfer_.idle()?"idle":"updating",
            static_cast<unsigned long>(transfer_.frames()),static_cast<unsigned long>(maxTransactionUs_));
        Serial.println("# Transfer ACK/timing is not proof of correct display or a hard latency guarantee");
    }
private:
    void draw(bool healthy,uint32_t seconds) {
        std::memset(canvas_.pixels,0,sizeof(canvas_.pixels));
        canvas_.setTextColor(1); canvas_.setTextWrap(false); canvas_.setTextSize(1);
        canvas_.drawRect(0,0,128,64,1);
        canvas_.setCursor(4,3); canvas_.print("RTC CLOCK");
        ClockDisplay::Calendar t{};
        if (healthy && ClockDisplay::calendar(seconds,t)) {
            char text[24];
            snprintf(text,sizeof(text),"%04u-%02u-%02u",t.year,t.month,t.day);
            canvas_.setCursor(4,15); canvas_.print(text);
            snprintf(text,sizeof(text),"%02u:%02u:%02u",t.hour,t.minute,t.second);
            canvas_.setTextSize(2); canvas_.setCursor(4,28); canvas_.print(text);
            canvas_.setTextSize(1); canvas_.setCursor(4,49); canvas_.print("RTC OK  OSF=0");
        } else {
            canvas_.setCursor(4,25); canvas_.print("RTC UNTRUSTED");
            canvas_.setCursor(4,42); canvas_.print("Check serial status");
        }
    }
    ClockCanvas canvas_;
    ClockDisplay::Transfer transfer_;
    bool lastHealthy_=false,faultReported_=false,injectFailure_=false;
    uint32_t lastSeconds_=0,maxTransactionUs_=0;
};
