#pragma once

#include <cstdint>
#include <pigpio.h>
#include <algorithm>
#include <chrono>
#include <vector>

namespace pt6964::interface {
    // TODO: tune turnaroundDelayNs to the minimum value that works reliably
    inline constexpr unsigned int DEFAULT_TURNAROUND_DELAY_NS = 500;
    template<unsigned int turnaroundDelayNs = DEFAULT_TURNAROUND_DELAY_NS>
    class RealPigpioInterface {
    private:
        uint8_t dataPinMode = 0;
    public:
        const uint8_t csPin, clkPin, dataPin;

        RealPigpioInterface(uint8_t cs, uint8_t clk, uint8_t data): csPin(cs), clkPin(clk), dataPin(data) {
            PT6964_ASSERT(cs != clk && cs != data && clk != data, "Invalid pin number.");
            PT6964_ASSERT(gpioCfgGetInternals() == PI_INITIALISED, "pigpio is not initialized. Please call gpioInitialise() before creating a PigpioInterface instance.");

            // we don't set the DATA pin mode here, because it will be switched between input and output as needed
            PT6964_ASSERT(gpioSetMode(csPin, PI_OUTPUT) == 0 && gpioSetMode(clkPin, PI_OUTPUT) == 0, "Failed to set pin modes");
        }

        RealPigpioInterface(const RealPigpioInterface&) = delete;
        RealPigpioInterface& operator=(const RealPigpioInterface&) = delete;

        void setCS(bool high) {
            gpioWrite(csPin, high ? PI_HIGH : PI_LOW);
        }

        void setCLK(bool high) {
            gpioWrite(clkPin, high ? PI_HIGH : PI_LOW);
        }

        void setData(bool high) {
            if (dataPinMode != 1) {
                PT6964_ASSERT(gpioSetMode(dataPin, PI_OUTPUT) == 0, "Failed to set DATA pin mode to OUTPUT");
                dataPinMode = 1;
                this->delay(turnaroundDelayNs);
            }
            gpioWrite(dataPin, high ? PI_HIGH : PI_LOW);
        }

        bool inputData() {
            if (dataPinMode != 2) {
                PT6964_ASSERT(gpioSetMode(dataPin, PI_INPUT) == 0, "Failed to set DATA pin mode to INPUT");
                dataPinMode = 2;
                this->delay(turnaroundDelayNs);
            }
            bool val = gpioRead(dataPin) == PI_HIGH;
            return val;
        }

        void delay(unsigned int nsec) {
            // pigpio does not support nanosecond delays
            // also, gpioDelay also uses a busy-wait for delays under 100us
            if (nsec < 1000) {
                // busy wait for very short delays
                auto start = std::chrono::steady_clock::now();
                const auto target = std::chrono::nanoseconds(nsec);
                while (std::chrono::steady_clock::now() - start < target) {
                    #if defined(__arm__) || defined(__aarch64__)
                        asm volatile("yield");
                    #endif
                }
            } else gpioDelay(nsec / 1000);
        }
    };

    using PigpioInterface = RealPigpioInterface<DEFAULT_TURNAROUND_DELAY_NS>;
}