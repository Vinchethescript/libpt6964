#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

namespace pt6964 {

    inline constexpr size_t MEMORY_SIZE = 14; // bytes
    inline constexpr uint8_t MAX_BRIGHTNESS = 7; // 3 bits full

    /**
     * Valid display mode values (last two bits of the MODE command as of the datasheet).
     */
    enum class DisplayMode: uint8_t {
        D4S13 = 0b00, // 4 digits, 13 segments
        D5S12 = 0b01, // 5 digits, 12 segments
        D6S11 = 0b10, // 6 digits, 11 segments
        D8S10 = 0b11  // 8 digits, 10 segments
    };

    enum class Command: uint8_t {
        ON   = 0b10001000, // Display on: last 3 bits are brightness
        OFF  = 0b10000000, // Display off. Brightness bits will be ignored.

        /**
         * Display mode settings. See also the DisplayMode enum and the getMode function.
         */
        MODE = 0b00000000,

        /**
         * Set memory address to start writing data from.
         * The last 4 bits are the address, which can range from 0 to 1101 (13),
         * giving a total of 14 bytes of memory.
         */
        ADDR = 0b11000000,

        /**
         * Set the action to perform.
         * The last 4 bits are:
         * 0b0100abc0
         *       |||
         *       ||+-- 0: Write data; 1: Read key data
         *       |+--- 0: Auto-increment RAM address; 1: Keep a fixed address
         *       +---- 0: Normal operation mode; 1: Test mode (not sure what this does)
         * See also the getAction function.
         */
        ACTION = 0b01000000,
    };

    enum class RWMode {
        NONE = 0,
        WRITE = 1,
        READ = 2
    };

    // array representing a copy of the PT6964's internal memory, which is 14 bytes long
    using MemoryType = std::array<uint8_t, MEMORY_SIZE>;
}