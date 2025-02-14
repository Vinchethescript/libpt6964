#include <stdexcept>
#include <algorithm>
#include <pigpio.h>
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

bool PT6964::isColonValid(const std::string &str) {
    int colonCount = std::count(str.begin(), str.end(), ':');
    if (colonCount > 2)
        return false;

    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos = 0;
    while ((pos = str.find(':', start)) != std::string::npos) {
        parts.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    parts.push_back(str.substr(start));
    

    size_t totalChars = 0;
    for (const auto &p : parts)
        totalChars += p.size();
    if (totalChars != 5)
        return false;

    if (colonCount == 2) {
        return (parts.size() == 3 &&
                parts[0].size() == 1 &&
                parts[1].size() == 2 &&
                parts[2].size() == 2);
    } else if (colonCount == 1) {
        return (parts.size() == 2 &&
                ((parts[0].size() == 3 && parts[1].size() == 2) ||
                 (parts[0].size() == 1 && parts[1].size() == 4)));
    } else if (colonCount == 0) {
        return (parts.size() == 1 && parts[0].size() == 5);
    }
    return false;
}


std::vector<unsigned int> PT6964::alphabetize(const std::string &s) {
    size_t nonColonCount = 0;
    for (char c : s) {
        if (c != ':')
            ++nonColonCount;
    }
    if (nonColonCount > 5)
        throw std::invalid_argument("String is too long");

    size_t colonCount = std::count(s.begin(), s.end(), ':');
    if (colonCount > 2)
        throw std::invalid_argument("Too many colons");

    if (s.size() > 5 && !isColonValid(s))
        throw std::invalid_argument("Invalid colon placement");

    std::vector<unsigned int> ret;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (ALPHABET.find(c) != ALPHABET.end()) {
            ret.push_back(ALPHABET.at(c));
        } else if (ALPHABET.find(std::toupper(c)) != ALPHABET.end()) {
            ret.push_back(ALPHABET.at(std::toupper(c)));
        } else if (ALPHABET.find(std::tolower(c)) != ALPHABET.end()) {
            ret.push_back(ALPHABET.at(std::tolower(c)));
        } else {
            if (c == ':') {
                if (i > 2)
                    ret.push_back(COLON_RIGHT);
                else
                    ret.push_back(COLON_LEFT);
            }
            else {
                // replace unknown characters with a space.
                ret.push_back(ALPHABET.at(' '));
            }
        }
    }
    return ret;
}

std::array<uint8_t, 14> PT6964::alphabetToBits(const std::vector<unsigned int> &alphabetized) {
    std::array<uint8_t, 7> base = {
        0, // top center
        0, // top right
        0, // bottom right
        0, // bottom center
        0, // center
        0, // top left, "00000100" = first ":"
        0  // bottom left, "00000100" = second ":"
    };

    size_t actual = 0; // Counter

    for (unsigned int elem : alphabetized) {
        if (elem == COLON_LEFT || elem == COLON_RIGHT) {
            const uint8_t mask = 1 << 2;
            if (elem == COLON_LEFT) {
                base[6] |= mask;
            } else { // COLON_RIGHT
                base[5] |= mask;
            }
            continue;
        }
        for (int j = 0; j < 7; ++j) {
            int bit = (elem >> (6 - j)) & 0x1;
            if (bit) {
                uint8_t mask = 1 << (7 - actual);
                base[j] |= mask;
            }
        }
        ++actual;
    }
    std::array<uint8_t, 14> full;
    for (int i = 0; i < 14; i += 2) {
        full[i] = base[i / 2];
        full[i + 1] = 0;
    }
    return full;
}


void PT6964::sendBit(bool bit) {
    interface.setData(bit);
    interface.setCLK(true);
    interface.delay(CLK_USEC);
    interface.setCLK(false);
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