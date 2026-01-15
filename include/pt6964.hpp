#ifndef PT6964_HPP
#define PT6964_HPP

#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <variant>
#include <optional>
#include <shared_mutex>

/**
 * Delay in nanoseconds between each CLK high and low.
 * This is a little more than the minimum the chip
 * can handle according to the datasheet (400ns).
 */
static inline constexpr int CLK_DELAY_NS = 500;
static inline constexpr size_t MEMORY_SIZE = 14; // bytes

enum class DisplayMode: uint8_t {
    D4S13 = 0b00,
    D5S12 = 0b01,
    D6S11 = 0b10,
    D8S10 = 0b11
};

enum class Command: uint8_t {
    ON   = 0b10001000, // Display on: last 3 bits are brightness
    OFF  = 0b10000000, // Display off. Brightness bits will be stored and used when turned on again.

    /**
     * Display mode settings. According to datasheet, last two bits are:
     * 00: 4 digits, 13 segments;
     * 01: 5 digits, 12 segments;
     * 10: 6 digits, 11 segments;
     * 11: 8 digits, 10 segments.
     * See also the DisplayMode enum and the getMode function.
     */
    MODE = 0b00000000,

    /**
     * Set memory address to start writing data from.
     * The last 4 bits are the address, which can range from 0 to 1101 (13).
     */
    ADDR = 0b11000000,

    /**
     * Set the action to perform.
     * The last 4 bits are:
     * 0b0100abc0
     *       |||
     *       ||+-- 0: Write data; 1: Read key data
     *       |+--- 0: Auto-increment RAM address; 1: Keep a fixed address
     *       +---- 0: Normal operation mode; 1: Test mode
     * See also the getAction function.
     */
    ACTION = 0b01000000,
};

using MemoryType = std::array<uint8_t, MEMORY_SIZE>;

uint8_t getAction(bool write, bool auto_inc, bool test);
uint8_t getMode(DisplayMode mode);


class BaseInterface {
public:
    BaseInterface();
    virtual ~BaseInterface() = default;
    virtual void setCS(bool high) = 0;
    virtual void setCLK(bool high) = 0;
    virtual void setData(bool high) = 0;
    virtual bool inputData() = 0;
    virtual void delay(int nsec);
};

class PigpioInterface: public BaseInterface {
    bool initialize;
    uint8_t dataPinMode = 0;
public:
    uint8_t csPin, clkPin, dataPin;
    // NOTE: initialize indicates whether to call gpioInitialise/gpioTerminate
    // pass false if pigpio is already initialized elsewhere or you need it with more control/pins
    PigpioInterface(uint8_t cs, uint8_t clk, uint8_t data, bool initialize = true);
    virtual ~PigpioInterface();
    virtual void setCS(bool high) override;
    virtual void setCLK(bool high) override;
    virtual void setData(bool high) override;
    virtual bool inputData() override;
    virtual void delay(int nsec) override;
};

class PT6964 {
private:
    MemoryType lastAddr = {0};
    bool lastMsgSet = false;
    std::optional<int> lastBrightness;
    std::optional<bool> lastDisp;
    std::shared_mutex mtx;
 
    bool first = true;
    DisplayMode mode;
    void sendBit(bool bit);
    void sendByte(uint8_t data);
    void sendAddress(uint8_t addr);
    void sendRawCommand(uint8_t command);
    uint8_t rwMode = 0;
public:
    BaseInterface& interface;
    bool testMode = false;
    PT6964(BaseInterface& iface, DisplayMode mode = DisplayMode::D8S10);
    void setBrightness(bool on, uint8_t brightness, bool force = false);

    bool writeMessage(const MemoryType msg,
        std::optional<bool> display_on = std::nullopt,
        std::optional<int> brightness = std::nullopt,
        bool force = false);
    
    void sendCommand(Command cmd, uint8_t data);
    uint16_t readKey();
    
};

#endif // PT6964_HPP
