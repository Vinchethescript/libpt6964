#include <iostream>
#include "pt6964.hpp"
#include <stdexcept>

bool PT6964::exists = false;

PT6964::PT6964(BaseInterface& iface, DisplayMode mode): interface(iface) {
    if (exists) {
        throw std::runtime_error("PT6964 instance already exists.");
    }
    exists = true;
    interface = iface;
    this->mode = mode;
    isSetUp = true;
}

PT6964::~PT6964() {
    writeMessage({}, false);
    isSetUp = false;
    delete &interface;
    exists = false;
}


void PT6964::sendCommand(Command command, uint8_t data) {
    uint8_t cmd = static_cast<uint8_t>(command);
    if (data > 63) {
        throw std::invalid_argument("Invalid data.");
    }
    sendRawCommand(cmd | data);
}

void PT6964::sendAddress(uint8_t addr) {
    if (addr > 13) {
        throw std::invalid_argument("Address must be between 0 and 13.");
    }
    sendCommand(Command::ADDR, addr);
}

void PT6964::writeRaw(uint8_t data[7]) {
    for (int i = 0; i < 7; i++) {
        for (int i = 7; i >= 0; i--) {
            sendBit(data[i] & (1 << i));
        }
        for (int i = 0; i < 7; i++) {
            sendBit(0);
        }
    }
}

void PT6964::sendBrightness(bool on, uint8_t brightness) {
    if (brightness > 7) {
        throw std::invalid_argument("Brightness must be between 0 and 7");
    }
    sendCommand(on ? Command::ON : Command::OFF, brightness);
}

std::vector<unsigned int> PT6964::parseMessage(const std::vector<MessagePart>& msg_parts) {
    std::vector<unsigned int> ret;
    for (const auto &part : msg_parts) {
        if (std::holds_alternative<int>(part)) {
            int val = std::get<int>(part);
            if (val > 128) continue;
            
            ret.push_back(static_cast<unsigned int>(val));
        }
        else if (std::holds_alternative<std::string>(part)) {
            std::vector<unsigned int> alph = alphabetize(std::get<std::string>(part));
            ret.insert(ret.end(), alph.begin(), alph.end());
        }
    }
    return ret;
}

void PT6964::writeMessage(const std::vector<MessagePart>& msg,
                    std::optional<bool> display_on,
                    std::optional<int> brightness,
                    bool force)
{
    // If we haven't written anything yet, we're still forcing
    force = force || first;

    bool disp = display_on.has_value()
        ? display_on.value()
        : (lastDisp.has_value() ? lastDisp.value() : true);

    int bright = brightness.has_value()
        ? brightness.value()
        : (lastBrightness.has_value() ? lastBrightness.value() : 4);

    if (bright < 0 || bright > 7)
        throw std::invalid_argument("Brightness must be between 0 and 7");

    if (!isSetUp)
        return;

    std::vector<unsigned int> parsed = parseMessage(msg);
    std::array<uint8_t, 14> dt = alphabetToBits(parsed);

    // If nothing has changed, then do not rewrite.
    if (!force && lastMsgSet &&
        (dt == lastMsg) &&
        (lastBrightness.has_value() && lastBrightness.value() == bright) &&
        (lastDisp.has_value() && lastDisp.value() == disp))
    {
        return;
    }
    
    // TODO: detect which addresses need updating and only update those
    interface.setCS(false);
    sendAddress(0);

    for (uint8_t row : dt) {

        for (int bit = 7; bit >= 0; --bit) {
            sendBit(row & (1 << bit));
        }
    }
    interface.setData(false);
    interface.setCS(true);
    interface.setCLK(false);

    lastMsg = dt;
    lastMsgSet = true;

    if (force ||
        !(lastBrightness.has_value() && lastBrightness.value() == bright) ||
        !(lastDisp.has_value() && lastDisp.value() == disp))
    {
        // Write initialization commands.
        // TODO: generalize this so it works with other displays
        sendRawCommand(getAction(true, true, testMode));
        sendRawCommand(getMode(this->mode));
        sendBrightness(disp, bright);

        lastBrightness = bright;
        lastDisp = disp;
    }

    first = false;
}

uint16_t PT6964::readKey() {
    uint16_t data = 0;
    interface.setCS(false);

    sendByte(getAction(false, true, testMode));

    for (int i = 0; i < 16; ++i) {
        interface.setCLK(true);
        interface.delay(CLK_USEC); // wait for the chip to update the DATA line

        if (interface.inputData()) {
            data |= (1 << i);
        }
        interface.setCLK(false);
    }
    interface.setCS(true);
    return data;
}
