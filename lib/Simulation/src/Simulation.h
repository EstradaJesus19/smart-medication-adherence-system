#pragma once
#include <MedicationSystem.h>

// One simulated device, shared by desktop stdin and ESP32 Serial.
namespace Simulation {
using WriteLine = void (*)(const char* text);
void begin(WriteLine output);
void feed(char byte);
} // namespace Simulation
