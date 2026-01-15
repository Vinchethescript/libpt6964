#include <stdexcept>
#include <algorithm>
#include <cctype>
#include "pt6964.hpp"

uint8_t getAction(bool write, bool auto_inc, bool test) {
    unsigned int action = 0;
    if (!write) action |= 0b00000010;
    if (!auto_inc) action |= 0b00000100;
    if (test) action |= 0b00001000;
    return static_cast<uint8_t>(Command::ACTION) | action;
}

uint8_t getMode(DisplayMode mode) {
    return static_cast<uint8_t>(Command::MODE) | static_cast<uint8_t>(mode);
}

void PT6964::sendBit(bool bit) {
    interface.setData(bit);
    interface.setCLK(true);
    interface.delay(CLK_DELAY_NS);
    interface.setCLK(false);
    interface.delay(CLK_DELAY_NS);
}

void PT6964::sendByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        sendBit(data & (1 << i));
    }
}

void PT6964::sendRawCommand(uint8_t command) {
    interface.setCS(false);
    sendByte(command);
    interface.setCS(true);
}