#include <Arduino.h>
#include <Simulation.h>

void printLine(const char* text) { Serial.println(text); }

void setup() {
    Serial.begin(115200);
    Simulation::begin(printLine);
}
void loop() {
    // Time advances only through commands in this simulation build.
    for (unsigned i = 0; i < 64 && Serial.available(); ++i) {
        Simulation::feed(static_cast<char>(Serial.read()));
    }
    yield();
}
