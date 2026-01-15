// g++ -std=c++17 -o all_segments{,.cpp} -lpt6964 -lpigpio -lrt -lpthread
#include <iostream>
#include <pt6964.hpp>

using namespace pt6964;

int main() {

    // adjust pins as needed.
    int cs   = 25;
    int clk  = 22;
    int data = 24;

    PigpioInterface iface(cs, clk, data);

    // adjust DisplayMode as needed
    PT6964 driver(iface, DisplayMode::D8S10);

    // light all segments
    MemoryType mem{};
    for (int i = 0; i < MEMORY_SIZE; i++) {
        mem[i] = 0b11111111;
    }

    driver.writeMessage(mem, true, 4);

    return 0;
}