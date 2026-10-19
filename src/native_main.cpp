#include <Simulation.h>
#include <cstdio>

void printLine(const char* text) { std::puts(text); }

int main() {
    Simulation::begin(printLine);
    int byte;
    while ((byte = std::getchar()) != EOF) {
        Simulation::feed(static_cast<char>(byte));
    }
    return 0;
}
