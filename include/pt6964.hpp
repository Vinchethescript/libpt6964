#ifndef PT6964_HPP
#define PT6964_HPP

#include <map>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <variant>
#include <optional>

// Delay in microseconds between each CLK high and low.
// This is because C++ seems to be a bit faster than
// the PT6964 can handle, making it miss some bits.
// This delay might be subject to change, because it *may*
// still be too fast for the chip to handle.
// TODO: put this as a parameter in the constructor.
#define CLK_USEC 1

enum class DisplayMode: uint8_t {
    D4S13 = 0b00,
    D5S12 = 0b01,
    D6S11 = 0b10,
    D8S10 = 0b11
};

enum class Command: uint8_t {
    ON   = 0b10001000, // Display on: last 3 bits are brightness
    OFF  = 0b10000000, // Display off

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
     * The last 4 bits are the address, which can be from 0 to 1101 (13).
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

using MessagePart = std::variant<int, std::string>;

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
    virtual void delay(int usec);
};

class PigpioInterface: public BaseInterface {
    bool initialize;
public:
    uint8_t csPin, clkPin, dataPin;
    PigpioInterface(uint8_t cs, uint8_t clk, uint8_t data, bool initialize = true);
    virtual ~PigpioInterface();
    virtual void setCS(bool high) override;
    virtual void setCLK(bool high) override;
    virtual void setData(bool high) override;
    virtual bool inputData() override;
    virtual void delay(int usec) override;
};

class PT6964 {
private:
    static inline constexpr unsigned int COLON_LEFT  = 0x100;
    static inline constexpr unsigned int COLON_RIGHT = 0x101;
    std::array<uint8_t, 14> lastMsg = {0};
    bool lastMsgSet = false;
    std::optional<int> lastBrightness;
    std::optional<bool> lastDisp;
    bool isSetUp = false;
    
    bool first;
    int csPin, clkPin, dataPin;
    DisplayMode mode;
    static bool exists;
    void output(int pin, bool high);
    void sendBit(bool bit);
    void sendByte(uint8_t data);
    void sendAddress(uint8_t addr);
    void sendRawCommand(uint8_t command);
    void writeRaw(uint8_t data[7]);
    static bool isColonValid(const std::string &str);
    static std::vector<unsigned int> alphabetize(const std::string &s);
    static std::array<uint8_t, 14> alphabetToBits(const std::vector<unsigned int> &alphabetized);
    static std::vector<unsigned int> parseMessage(const std::vector<MessagePart>& msg_parts);
    void sendBrightness(bool on, uint8_t brightness);
public:
    BaseInterface& interface;
    /**
     * 7-segment display alphabet.
       One bit for each segment in the following order:
       |0|
       5 1
       |4|
       6 2
       |3|
     */
    static inline const std::map<char, uint8_t> ALPHABET = {
        {'0', 0b1111011},
        {'1', 0b0110000},
        {'2', 0b1101101},
        {'3', 0b1111100},
        {'4', 0b0110110},
        {'5', 0b1011110},
        {'6', 0b1011111},
        {'7', 0b1110000},
        {'8', 0b1111111},
        {'9', 0b1111110},
        {'A', 0b1110111},
        {'b', 0b0011111},
        {'C', 0b1001011},
        {'c', 0b0001101},
        {'d', 0b0111101},
        {'E', 0b1001111},
        {'F', 0b1000111},
        {'G', 0b1011111},
        {'H', 0b0110111},
        {'h', 0b0010111},
        {'I', 0b0110000},
        {'J', 0b0111001},
        {'L', 0b0001011},
        {'n', 0b0010101},
        {'O', 0b1111011},
        {'o', 0b0011101},
        {'P', 0b1100111},
        {'r', 0b0000101},
        {'S', 0b1011110},
        {'t', 0b0001111},
        {'U', 0b0111011},
        {'u', 0b0011001},
        {'y', 0b0111110},
        {'Z', 0b1101101},
        {'(', 0b1001011},
        {')', 0b1111000},
        {'-', 0b0000100},
        {'_', 0b0001000},
        {' ', 0b0000000}
    };

    static inline const std::map<char, uint8_t> EXTRAS = {
        {'c', 0b1000110},
        {'n', 0b1100010},
        {'o', 0b1100110},
        {'r', 0b1000010},
        {'u', 0b0100110},
        {'-', 0b1000000},
    };
    bool testMode = false;
    PT6964(BaseInterface& iface, DisplayMode mode = DisplayMode::D8S10);
    ~PT6964();

    void writeMessage(const std::vector<MessagePart>& msg,
        std::optional<bool> display_on = std::nullopt,
        std::optional<int> brightness = std::nullopt,
        bool force = false);
    
    void sendCommand(Command cmd, uint8_t data);
    uint16_t readKey();
    
};

#endif // PT6964_HPP
