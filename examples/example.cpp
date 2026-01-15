// g++ -std=c++17 -o example{,.cpp} -lpt6964 -lpigpio -lrt -lpthread
#include <iostream>
#include <bitset>
#include <thread>
#include <chrono>
#include <pt6964.hpp>

using namespace pt6964;

int main() {

    // adjust pins as needed.
    // all the examples here will use pigpio
    int cs   = 25;
    int clk  = 22;
    int data = 24;

    PigpioInterface iface(cs, clk, data);

    // adjust DisplayMode as needed
    PT6964 driver(iface, DisplayMode::D8S10);

    /**
     * each byte is one address in the PT6964 RAM (14 bytes total).
     * this will light some segments. you may try to mess up here to see how it works.
     * to make it easier, you may also make a function that generates this mem array based on
     * the digits/characters you want to show.
     */
    MemoryType mem{};
    mem[0] = 0b01111110;
    mem[1] = 0b00110000;
    mem[2] = 0b11101101;
    mem[3] = 0b01111001;
    mem[4] = 0b00110011;
    mem[5] = 0b01011011;
    mem[6] = 0b01011111;
    mem[7] = 0b01110000;
    mem[8] = 0b11111111;
    mem[9] = 0b01111011;
    mem[10] = 0b00000110;
    mem[11] = 0b11100100;
    mem[12] = 0b00010000;
    mem[13] = 0b00011110;

    int displayOn = true;
    int brightness = 4; // 0 (dim) to 7 (max)
    driver.writeMessage(mem, displayOn, brightness);

    std::cout << "Sleeping for 5 seconds. Hold a key..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // read keys, if any. you may make an enum for easier handling of key bits.
    uint16_t keys = driver.readKey();
    std::cout << "Keys: 0b" << std::bitset<16>(keys) << std::endl;


    // update brightness only
    driver.setBrightness(true, 2);

    return 0;
}