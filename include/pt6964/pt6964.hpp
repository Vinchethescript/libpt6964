#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <algorithm>
#include <stdexcept>

#include "interfaces.hpp"
#include "defs.hpp"

#if __cplusplus >= 202002L
    #include <concepts>
#endif


#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    #include <stdexcept>
    #define PT6964_ASSERT(cond, msg) do { if (!(cond)) { throw std::invalid_argument(msg); } } while(0)
#else
    #include <cstdlib>
    #define PT6964_ASSERT(cond, msg) do { if (!(cond)) { (void)(msg); std::abort(); } } while(0)
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
}
/**
 * Main PT6964 IC driver class.
 * @param HardwareInterface IC communication implementation class.
 * @param clkDelayNs Delay in nanoseconds between each CLK high and low [see below].
 *                   The PT6964 datasheet specifies a minimum of
 *                   400ns between each CLK high and low. The library
 *                   defaults to 500ns, which is a little more, but
 *                   you can tune it to whichever value that works reliably for your setup.
 * 
 * @param iface HardwareInterface instance to use for communication.
 * @param mode DisplayMode to use for the PT6964 IC [see DisplayMode enum].
 */
template<PT6964_INTERFACE_CONCEPT InterfaceT, unsigned int clkDelayNs = 500>
class PT6964 {
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

    /**
     * Toggle the display on/off and set the brightness.
     * @param on true to turn the display on, false to turn it off.
     * @param brightness Brightness level (0-7). 0 is dimmest (not off), 7 is brightest.
     * @param force Whether to force the command to be sent even if the state hasn't changed. Default: false
     */
    void setBrightness(bool on, uint8_t brightness, bool force = false) {
        PT6964_ASSERT(brightness <= MAX_BRIGHTNESS, "Brightness out of range.");

        doSetBrightness(on, brightness, force);
    }

    
    /**
     * Write to the PT6964's display memory.
     * @param mem MemoryType array containing the bytes to write.
     * @param display_on Optional boolean to control display state.
     * @param brightness Optional brightness level (0-7).
     * @param force Whether to perform the actions even if nothing has changed. Default: false
     * @return true if the message was written, false otherwise.
     */
    bool writeMessage(const MemoryType& mem,
        std::optional<bool> display_on = std::nullopt,
        std::optional<uint8_t> brightness = std::nullopt,
        bool force = false)
    {   
        // If we haven't written anything yet, we're still forcing
        force = force || first;

        bool disp = display_on.value_or(lastDisp.value_or(true));
        uint8_t bright = brightness.value_or(lastBrightness.value_or(4));
        PT6964_ASSERT(bright <= MAX_BRIGHTNESS, "Brightness out of range.");

        // If nothing has changed, then do not rewrite.
        if (!force && lastMsgSet &&
            (mem == lastAddr) &&
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
             * at least, it seems like the IC works that way as tested.
             */
            bool sendClose = true;
            if (force) {
                interface.setCS(false);
                sendAddress(0);
                for (uint8_t row : mem) {
                    for (int bit = 7; bit >= 0; --bit) {
                        sendBit(row & (1 << bit));
                    }
                }
            } else {
                // only send what effectively changed
                bool continuing = false;
                for (size_t i = 0; i < MEMORY_SIZE; ++i) {
                    if (mem[i] == lastAddr[i]) {
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
                            sendBit(mem[i] & (1 << bit));
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
        lastAddr = mem;
        lastMsgSet = true;

        first = false;
        return true;
    }
    
    void sendCommand(Command command, uint8_t data) {
        uint8_t cmd = static_cast<uint8_t>(command);
        PT6964_ASSERT(data <= 0b00111111, "Data too large.");
        sendRawCommand(cmd | data);
    }

    [[nodiscard]] uint16_t readKey() {
        uint16_t data = 0;

        /** 
         * NOTE: I didn't test if I have to always
         *       send the ACTION command before reading;
         *       just making sure it always works for now.
         */
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

        testMode = test;
        rwMode = RWMode::NONE; // send on the next read/possibly write
    }
};

} // namespace pt6964

#undef PT6964_INTERFACE_CONCEPT
#undef PT6964_ASSERT