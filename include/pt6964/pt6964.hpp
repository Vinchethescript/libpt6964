#pragma once

#if !defined(PT6964_USE_MUTEX)
    #if defined(__STDCPP_THREADS__)
        #define PT6964_USE_MUTEX 1
    #else
        #define PT6964_USE_MUTEX 0
    #endif
#endif

#include <cstdint>
#include <array>
#include <optional>
#include <algorithm>

#if PT6964_USE_MUTEX && __has_include(<mutex>)
    #include <mutex>
#endif

#include <stdexcept>
#include "interfaces.hpp"
#include "defs.hpp"

#if __cplusplus >= 202002L
    #include <concepts>
#endif

namespace pt6964 {

#if __cplusplus >= 202002L
    template<typename T>
    concept HardwareInterface = requires(T& interface, bool state, unsigned int delayNs) {
        { interface.inputData() } -> std::same_as<bool>;
        { interface.setCS(state) };
        { interface.setCLK(state) };
        { interface.setData(state) };
        { interface.delay(delayNs) };
    };
    #define PT6964_INTERFACE_CONCEPT HardwareInterface
#else
    #define PT6964_INTERFACE_CONCEPT typename
#endif


namespace utils {
    [[nodiscard]] inline constexpr uint8_t getAction(bool write, bool auto_inc, bool test) {
        uint8_t action = 0;
        if (!write) action |= 0b00000010;
        if (!auto_inc) action |= 0b00000100;
        if (test) action |= 0b00001000;
        return static_cast<uint8_t>(Command::ACTION) | action;
    }
    
    [[nodiscard]] inline constexpr uint8_t getMode(DisplayMode mode) {
        return static_cast<uint8_t>(Command::MODE) | static_cast<uint8_t>(mode);
    }

    struct DummyMutex {
        void lock() {}
        void unlock() {}
    };
}

#if !defined(PT6964_MUTEX)
    #if PT6964_USE_MUTEX && __has_include(<mutex>)
        #define PT6964_MUTEX std::mutex
    #else
        #define PT6964_MUTEX utils::DummyMutex
    #endif
#endif

namespace detail {
    template<PT6964_INTERFACE_CONCEPT InterfaceT, typename MutexT = PT6964_MUTEX>
    class PT6964Base {
    protected:
        #if PT6964_USE_MUTEX
        inline static MutexT mtx;
        #endif
    };
}
/**
 * Main PT6964 IC driver class.
 * Template parameters:
 * - HardwareInterface: IC communication implementation class.
 * - clkDelayNs: Delay in nanoseconds between each CLK high and low [see below].
 *               The PT6964 datasheet specifies a minimum of
 *               400ns between each CLK high and low. The library
 *               defaults to 500ns, which is a little more, but
 *               you can tune it to whichever value that works reliably for your setup.
 * - MutexT: Mutex type to use for thread safety.
 *           Defaults to std::mutex if available,
 *           otherwise a dummy mutex that does nothing.
 *           You can also provide your own mutex type
 *           that implements lock() and unlock() methods.
 * 
 * Instance parameters:
 * - iface: HardwareInterface instance to use for communication.
 * - mode: DisplayMode to use for the PT6964 IC [see DisplayMode enum].
 */
template<PT6964_INTERFACE_CONCEPT InterfaceT, unsigned int clkDelayNs = 500, typename MutexT = PT6964_MUTEX>
class PT6964: public detail::PT6964Base<InterfaceT, MutexT> {
private:
    MemoryType lastAddr = {0};
    bool lastMsgSet = false;
    std::optional<uint8_t> lastBrightness;
    std::optional<bool> lastDisp;

    bool first = true;
    DisplayMode mode;
    void sendBit(bool bit) {
        interface.setData(bit);
        interface.setCLK(true);
        interface.delay(clkDelayNs);
        interface.setCLK(false);
        interface.delay(clkDelayNs);
    }

    void sendByte(uint8_t data) {
        for (int i = 0; i < 8; i++) {
            sendBit(data & (1 << i));
        }
    }

    void sendAddress(uint8_t addr) {
        addr = std::min(addr, static_cast<uint8_t>(MEMORY_SIZE - 1));
        sendByte(static_cast<uint8_t>(Command::ADDR) | addr);
    }

    void sendRawCommand(uint8_t command) {
        interface.setCS(false);
        sendByte(command);
        interface.setCS(true);
    }

    void doSetBrightness(bool on, uint8_t brightness, bool force) {
        if (!force) {
            if (lastBrightness == brightness && lastDisp == on) {
                return;
            }
        }

        sendRawCommand(static_cast<uint8_t>(on ? Command::ON : Command::OFF) | brightness);

        lastBrightness = brightness;
        lastDisp = on;
    }

    RWMode rwMode = RWMode::NONE;
    bool testMode = false;
public:
    InterfaceT& interface;

    PT6964(InterfaceT& iface, DisplayMode mode = DisplayMode::D7S10): interface(iface), mode(mode) {
        interface.setCS(true);
        interface.setCLK(false);
        interface.setData(false);
    }
    
    PT6964(const PT6964&) = delete;
    PT6964& operator=(const PT6964&) = delete;
    PT6964(PT6964&&) = delete;
    PT6964& operator=(PT6964&&) = delete;

    void setBrightness(bool on, uint8_t brightness, bool force = false) {
        brightness = std::min(brightness, MAX_BRIGHTNESS);

        #if PT6964_USE_MUTEX
        std::lock_guard<MutexT> lock(mtx);
        #endif

        doSetBrightness(on, brightness, force);
    }

    bool writeMessage(const MemoryType& addr,
        std::optional<bool> display_on = std::nullopt,
        std::optional<uint8_t> brightness = std::nullopt,
        bool force = false)
    {
        #if PT6964_USE_MUTEX
        std::lock_guard<MutexT> lock(mtx);
        #endif
        
        // If we haven't written anything yet, we're still forcing
        force = force || first;

        bool disp = display_on.value_or(lastDisp.value_or(true));
        uint8_t bright = std::min(brightness.value_or(lastBrightness.value_or(4)), MAX_BRIGHTNESS);

        // If nothing has changed, then do not rewrite.
        if (!force && lastMsgSet &&
            (addr == lastAddr) &&
            (lastBrightness.has_value() && lastBrightness.value() == bright) &&
            (lastDisp.has_value() && lastDisp.value() == disp))
        {
            return false;
        }


        if (force || rwMode != RWMode::WRITE) {
            // Write initialization commands.
            sendRawCommand(utils::getAction(true, true, testMode));
            sendRawCommand(utils::getMode(this->mode));

            rwMode = RWMode::WRITE;
        }

        doSetBrightness(disp, bright, force);

        {
            /**
             * NOTE: though the commands are sent LSB first, the
             * actual display data is sent MSB first per byte.
             * at least, it seems like the IC works that way.
             */
            bool sendClose = true;
            if (force) {
                interface.setCS(false);
                sendAddress(0);
                for (uint8_t row : addr) {
                    for (int bit = 7; bit >= 0; --bit) {
                        sendBit(row & (1 << bit));
                    }
                }
            } else {
                // only send what effectively changed
                bool continuing = false;
                for (size_t i = 0; i < MEMORY_SIZE; ++i) {
                    if (addr[i] == lastAddr[i]) {
                        if (continuing) {
                            continuing = false;
                            interface.setCS(true);
                            interface.setData(false);
                            interface.setCLK(false);
                        }
                        sendClose = false;
                    } else {
                        if (!continuing) {
                            interface.setCS(false);
                            sendAddress(static_cast<uint8_t>(i));
                            continuing = true;
                            sendClose = true;
                        }
                        for (int bit = 7; bit >= 0; --bit) {
                            sendBit(addr[i] & (1 << bit));
                        }
                    }
                }
            }

            if (sendClose) {
                interface.setCS(true);
                interface.setData(false);
                interface.setCLK(false);
            }
        }
        lastAddr = addr;
        lastMsgSet = true;

        first = false;
        return true;
    }
    
    void sendCommand(Command command, uint8_t data) {
        uint8_t cmd = static_cast<uint8_t>(command);
        data &= 0b00111111;
        
        #if PT6964_USE_MUTEX
        std::lock_guard<MutexT> lock(mtx);
        #endif

        sendRawCommand(cmd | data);
    }

    [[nodiscard]] uint16_t readKey() {
        uint16_t data = 0;

        #if PT6964_USE_MUTEX
        std::lock_guard<MutexT> lock(mtx);
        #endif

        rwMode = RWMode::READ;
        interface.setCS(false);

        sendByte(utils::getAction(false, true, testMode));

        // as per datasheet, wait at least 1us before reading
        interface.delay(1000);

        for (int i = 0; i < 16; ++i) {
            interface.setCLK(true);
            interface.delay(clkDelayNs); // wait for the chip to update the DATA line

            // we assume that the HardwareInterface instance handles pin mode switching when needed
            if (interface.inputData()) {
                data |= (1 << i);
            }
            interface.setCLK(false);
        }
        interface.setCS(true);
        return data;
    }
    
    bool getTestMode() const {
        return testMode;
    }

    void setTestMode(bool test) {
        if (testMode == test) {
            return;
        }

        #if PT6964_USE_MUTEX
        std::lock_guard<MutexT> lock(mtx);
        #endif

        testMode = test;
        rwMode = RWMode::NONE; // send on the next write/read
    }
};

#undef PT6964_INTERFACE_CONCEPT

} // namespace pt6964