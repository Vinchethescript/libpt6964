#include "pt6964.hpp"
#include <stdexcept>

BaseInterface::BaseInterface() {}

void BaseInterface::delay(int usec) {
    // TODO: add this as base
}

#ifdef TARGET_RPI
#include <pigpio.h>

PigpioInterface::PigpioInterface(uint8_t cs, uint8_t clk, uint8_t data, bool initialize) {
    if (initialize && gpioInitialise() < 0) {
        throw std::runtime_error("Failed to initialize pigpio");
    }
    gpioSetMode(cs, PI_OUTPUT);
    gpioSetMode(clk, PI_OUTPUT);
    gpioSetMode(data, PI_OUTPUT);
    this->csPin = cs;
    this->clkPin = clk;
    this->dataPin = data;
    this->initialize = initialize;
}

PigpioInterface::~PigpioInterface() {
    if (initialize)
        gpioTerminate();
}

void PigpioInterface::delay(int usec) {
    gpioDelay(usec);
}

void PigpioInterface::setCS(bool high) {
    gpioWrite(csPin, high ? PI_HIGH : PI_LOW);
}

void PigpioInterface::setCLK(bool high) {
    gpioWrite(clkPin, high ? PI_HIGH : PI_LOW);
}

void PigpioInterface::setData(bool high) {
    gpioWrite(dataPin, high ? PI_HIGH : PI_LOW);
}

bool PigpioInterface::inputData() {
    gpioSetMode(dataPin, PI_INPUT);
    this->delay(CLK_USEC);
    bool val = gpioRead(dataPin) == PI_HIGH;
    gpioSetMode(dataPin, PI_OUTPUT);
    return val;
}
#endif // TARGET_RPI
