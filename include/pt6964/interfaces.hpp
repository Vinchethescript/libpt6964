#pragma once

#if defined(TARGET_RPI)
#include <cstdint>
#include <pigpio.h>

#if PT6964_USE_MUTEX && __has_include(<mutex>)
    #include <mutex>
#endif

#include <algorithm>
#include <chrono>
#include <vector>
#include <stdexcept>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    #define PT6964_ERROR(msg) throw std::invalid_argument(msg)
#else
    #include <cstdlib>
    #define PT6964_ERROR(msg) do { (void)(msg); std::abort(); } while(0)
#endif

namespace pt6964::interface {

    namespace detail {
        /**
         * it is okay to use different CS pins
         * while keeping CLK and DATA the same,
         * as long as only one interface is active at a time.
         * The static mutex in the main class makes sure that
         * chips coming from the same interface don't interfere with each other.
         */

        #if PT6964_USE_MUTEX && __has_include(<mutex>)
        inline std::mutex csMutex;
        inline std::vector<uint8_t> csPins; // if no mutex then why keep pins?
        #endif

    }

    // TODO: tune turnaroundDelayNs to the minimum value that works reliably
    template<unsigned int turnaroundDelayNs = 500>
    class PigpioInterface {
    private:
        static constexpr uint8_t INVALID = 255;
        uint8_t dataPinMode = 0;

        void cleanup() {
            if (csPin != INVALID) {
                #if PT6964_USE_MUTEX && __has_include(<mutex>)
                std::lock_guard<std::mutex> lock(detail::csMutex);

                detail::csPins.erase(std::remove(detail::csPins.begin(), detail::csPins.end(), csPin), detail::csPins.end());
                #endif
                csPin = INVALID;
            }
        }

    public:
        uint8_t csPin, clkPin, dataPin;

        PigpioInterface(uint8_t cs, uint8_t clk, uint8_t data): csPin(cs), clkPin(clk), dataPin(data) {
            if (cs == INVALID || clk == INVALID || data == INVALID || cs == clk || cs == data || clk == data) {
                PT6964_ERROR("Invalid pin number.");
            }
            if (gpioCfgGetInternals() != PI_INITIALISED) {
                PT6964_ERROR("pigpio is not initialized. Please call gpioInitialise() before creating a PigpioInterface instance.");
            }

            #if PT6964_USE_MUTEX && __has_include(<mutex>)
            std::lock_guard<std::mutex> lock(detail::csMutex);

            if (std::find(detail::csPins.begin(), detail::csPins.end(), cs) != detail::csPins.end()) {
                PT6964_ERROR("CS pin already in use by another PigpioInterface instance");
            }
            #endif
            
            // we don't set the DATA pin mode here, because it will be switched between input and output as needed
            if (gpioSetMode(csPin, PI_OUTPUT) != 0 ||
                gpioSetMode(clkPin, PI_OUTPUT) != 0
            ) {
                PT6964_ERROR("Failed to set pin modes");
            }
            #if PT6964_USE_MUTEX && __has_include(<mutex>)
            detail::csPins.push_back(cs);
            #endif
        }

        PigpioInterface(const PigpioInterface&) = delete;
        PigpioInterface& operator=(const PigpioInterface&) = delete;
        PigpioInterface(PigpioInterface&& other) noexcept: dataPinMode(other.dataPinMode), csPin(other.csPin), clkPin(other.clkPin), dataPin(other.dataPin) {
            other.csPin = INVALID;
        }

        PigpioInterface& operator=(PigpioInterface&& other) noexcept {
            if (this != &other) {
                cleanup(); 
                
                dataPinMode = other.dataPinMode;
                csPin = other.csPin;
                clkPin = other.clkPin;
                dataPin = other.dataPin;
                
                other.csPin = INVALID;
                other.dataPinMode = 0;
            }
            return *this;
        }

        ~PigpioInterface() {
            cleanup();
        }

        void setCS(bool high) {
            gpioWrite(csPin, high ? PI_HIGH : PI_LOW);
        }

        void setCLK(bool high) {
            gpioWrite(clkPin, high ? PI_HIGH : PI_LOW);
        }

        void setData(bool high) {
            if (dataPinMode != 1) {
                if (gpioSetMode(dataPin, PI_OUTPUT) != 0) {
                    PT6964_ERROR("Failed to set DATA pin mode to OUTPUT");
                }
                dataPinMode = 1;
                this->delay(turnaroundDelayNs);
            }
            gpioWrite(dataPin, high ? PI_HIGH : PI_LOW);
        }

        bool inputData() {
            if (dataPinMode != 2) {
                if (gpioSetMode(dataPin, PI_INPUT) != 0) {
                    PT6964_ERROR("Failed to set DATA pin mode to INPUT");
                }
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
}

#undef PT6964_ERROR

#endif // TARGET_RPI