#include "pt6964.hpp"
#include <stdexcept>
#include <ctime>
#include <chrono>
#include <cerrno>

BaseInterface::BaseInterface() {}

void fallbackBusySleep(int nsec) {
    // busy wait for very short delays
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count() >= nsec)
            break;
    }
}

void BaseInterface::delay(int nsec) {
    if (nsec < 100000)
        fallbackBusySleep(nsec);
    else {
        struct timespec req, rem;
        req.tv_sec = nsec / 1000000000L;
        req.tv_nsec = nsec % 1000000000L;
        while (clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem) == -1 && errno == EINTR) {
            req = rem;
        }
    }
}

#ifdef TARGET_RPI
#include <pigpio.h>
#include <mutex>
#include <algorithm>

/**
 * it is okay to use different CS pins
 * while keeping CLK and DATA the same,
 * as long as only one interface is active at a time.
 */

std::mutex csMutex;
std::vector<uint8_t> csPins;
bool beenInitializedInternally = false;

PigpioInterface::PigpioInterface(uint8_t cs, uint8_t clk, uint8_t data, bool initialize): initialize(initialize), csPin(cs), clkPin(clk), dataPin(data) {
    std::lock_guard<std::mutex> lock(csMutex);
    if (initialize && !beenInitializedInternally) {
        if (gpioInitialise() < 0) {
            throw std::runtime_error("Failed to initialize pigpio");
        }
        beenInitializedInternally = true;
    }
    if (std::find(csPins.begin(), csPins.end(), cs) != csPins.end()) {
        throw std::runtime_error("CS pin already in use by another PigpioInterface instance");
    }
    if (gpioSetMode(csPin, PI_OUTPUT) != 0 ||
        gpioSetMode(clkPin, PI_OUTPUT) != 0 ||
        gpioSetMode(dataPin, PI_OUTPUT) != 0
    ) {
        throw std::runtime_error("Failed to set pin modes");
    }
    csPins.push_back(cs);

}

PigpioInterface::~PigpioInterface() {
    // we assume that if we initialized pigpio, we should also terminate it
    std::lock_guard<std::mutex> lock(csMutex);
    if (beenInitializedInternally && csPins.size() == 1) {
        gpioTerminate();
        beenInitializedInternally = false;
    }

    csPins.erase(std::remove(csPins.begin(), csPins.end(), csPin), csPins.end());
}

void PigpioInterface::delay(int nsec) {
    // pigpio does not support nanosecond delays
    // also, gpioDelay also uses a busy-wait for delays under 100us
    if (nsec < 1000)
        fallbackBusySleep(nsec);
    else
        gpioDelay(nsec / 1000);
}

void PigpioInterface::setCS(bool high) {
    gpioWrite(csPin, high ? PI_HIGH : PI_LOW);
}

void PigpioInterface::setCLK(bool high) {
    gpioWrite(clkPin, high ? PI_HIGH : PI_LOW);
}

void PigpioInterface::setData(bool high) {
    if (dataPinMode != 1) {
        if (gpioSetMode(dataPin, PI_OUTPUT) != 0) {
            throw std::runtime_error("Failed to set DATA pin mode to OUTPUT");
        }
        dataPinMode = 1;
        this->delay(CLK_DELAY_NS);
    }
    gpioWrite(dataPin, high ? PI_HIGH : PI_LOW);
}

bool PigpioInterface::inputData() {
    if (dataPinMode != 2) {
        if (gpioSetMode(dataPin, PI_INPUT) != 0) {
            throw std::runtime_error("Failed to set DATA pin mode to INPUT");
        }
        dataPinMode = 2;
        this->delay(CLK_DELAY_NS);
    }
    bool val = gpioRead(dataPin) == PI_HIGH;
    return val;
}
#endif // TARGET_RPI
