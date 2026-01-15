// g++ -std=c++17 -o random_seg{,.cpp} -lpt6964 -lpigpio -lrt -lpthread
#include <iostream>
#include <bitset>
#include <thread>
#include <chrono>
#include <random>
#include <pt6964.hpp>

using namespace pt6964;

int main() {

    int sleep = 100; // milliseconds

    // adjust pins as needed.
    int cs   = 25;
    int clk  = 22;
    int data = 24;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::uniform_int_distribution<> brDis(0, 7);

    PigpioInterface iface(cs, clk, data);

    // adjust DisplayMode as needed
    PT6964 driver(iface, DisplayMode::D8S10);

    std::cout << "Press any key..." << std::endl;
    while (true) {

        uint16_t keys = driver.readKey();
        if (keys != 0) {
            std::cout << "Keys pressed: 0b" << std::bitset<16>(keys) << std::endl;

            MemoryType mem{};
            for (int i = 0; i < MEMORY_SIZE; i++) {
                mem[i] = dis(gen);
            }

            int brightness = brDis(gen);
            driver.writeMessage(mem, true, brightness);

            std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
        }
    }

    return 0;
}