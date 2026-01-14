#include <mutex>
#include "pt6964.hpp"
#include <stdexcept>

std::mutex pt6964Mutex;

PT6964::PT6964(BaseInterface& iface, DisplayMode mode): interface(iface), mode(mode) {
    interface.setCS(true);
    interface.setCLK(false);
    interface.setData(false);
}

void PT6964::sendCommand(Command command, uint8_t data) {
    uint8_t cmd = static_cast<uint8_t>(command);
    if (data > 63) {
        throw std::invalid_argument("Invalid data.");
    }
    sendRawCommand(cmd | data);
}

void PT6964::setAddress(uint8_t addr) {
    if (addr > 13) {
        throw std::invalid_argument("Address must be between 0 and 13.");
    }
    sendCommand(Command::ADDR, addr);
}

void PT6964::sendAddress(uint8_t addr) {
    if (addr > 13) {
        throw std::invalid_argument("Address must be between 0 and 13.");
    }
    sendByte(static_cast<uint8_t>(Command::ADDR) | addr);
}

void PT6964::setBrightness(bool on, uint8_t brightness) {
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
            if (val > 127) val = 127;
            else if (val < 0) val = 0;

            ret.push_back(static_cast<unsigned int>(val));
        }
        else if (std::holds_alternative<std::string>(part)) {
            std::vector<unsigned int> alph = alphabetize(std::get<std::string>(part));
            ret.insert(ret.end(), alph.begin(), alph.end());
        }
    }
    return ret;
}

bool PT6964::writeMessage(const std::vector<MessagePart>& msg,
                    std::optional<bool> display_on,
                    std::optional<int> brightness,
                    bool force)
{
    // If we haven't written anything yet, we're still forcing
    force = force || first;

    std::shared_lock<std::shared_mutex> lock(mtx);
    bool disp = display_on.has_value()
        ? display_on.value()
        : (lastDisp.has_value() ? lastDisp.value() : true);

    int bright = brightness.has_value()
        ? brightness.value()
        : (lastBrightness.has_value() ? lastBrightness.value() : 4);

    if (bright < 0 || bright > 7)
        throw std::invalid_argument("Brightness must be between 0 and 7");

    std::vector<unsigned int> parsed = parseMessage(msg);
    std::array<uint8_t, 14> dt = alphabetToBits(parsed);

    // If nothing has changed, then do not rewrite.
    if (!force && lastMsgSet &&
        (dt == lastMsg) &&
        (lastBrightness.has_value() && lastBrightness.value() == bright) &&
        (lastDisp.has_value() && lastDisp.value() == disp))
    {
        return false;
    }

    bool mtxUnlocked = false;

    if (force ||
        !(lastBrightness.has_value() && lastBrightness.value() == bright) ||
        !(lastDisp.has_value() && lastDisp.value() == disp)
        || rwMode != 1
    ) {
        lock.unlock();
        mtxUnlocked = true;
        {
            std::lock_guard<std::mutex> lck(pt6964Mutex);

            // Write initialization commands.
            // TODO: generalize this so it works with other displays
            sendRawCommand(getAction(true, true, testMode));
            sendRawCommand(getMode(this->mode));
            setBrightness(disp, bright);
        }

        std::lock_guard<std::shared_mutex> uniqueLock(mtx);
        lastBrightness = bright;
        lastDisp = disp;
        rwMode = 1;
    }


    if (!mtxUnlocked)
        lock.unlock();


    {
        std::lock_guard<std::mutex> lck(pt6964Mutex);

        // TODO: detect which addresses need updating and only update those
        interface.setCS(false);
        sendByte(static_cast<uint8_t>(Command::ADDR));

        /**
         * NOTE: though the commands are sent LSB first, the
         * actual display data is sent MSB first per byte.
         * at least, it seems like the IC works that way.
         */
        for (uint8_t row : dt) {

            for (int bit = 7; bit >= 0; --bit) {
                sendBit(row & (1 << bit));
            }
        }
        interface.setData(false);
        interface.setCS(true);
        interface.setCLK(false);
    }

    std::lock_guard<std::shared_mutex> uniqueLock(mtx);
    lastMsg = dt;
    lastMsgSet = true;

    first = false;
    return true;
}

uint16_t PT6964::readKey() {
    uint16_t data = 0;
    {
        std::lock_guard<std::shared_mutex> lock(mtx);
        rwMode = 2;
    }

    std::lock_guard<std::mutex> lock(pt6964Mutex);
    interface.setCS(false);

    sendByte(getAction(false, true, testMode));

    // as per datasheet, wait at least 1us before reading
    interface.delay(1000);

    for (int i = 0; i < 16; ++i) {
        interface.setCLK(true);
        interface.delay(CLK_DELAY_NS); // wait for the chip to update the DATA line

        // we assume that the interface handles pin mode switching when needed
        if (interface.inputData()) {
            data |= (1 << i);
        }
        interface.setCLK(false);
    }
    interface.setCS(true);
    return data;
}
