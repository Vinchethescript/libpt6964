#include <mutex>
#include "pt6964.hpp"
#include <stdexcept>

namespace {
    std::mutex pt6964Mutex;
}

namespace pt6964 {

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

void PT6964::sendAddress(uint8_t addr) {
    if (addr > 13) {
        throw std::invalid_argument("Address must be between 0 and 13.");
    }
    sendByte(static_cast<uint8_t>(Command::ADDR) | addr);
}

void PT6964::setBrightness(bool on, uint8_t brightness, bool force) {
    if (brightness > 7) {
        throw std::invalid_argument("Brightness must be between 0 and 7");
    }
    if (!force) {
        std::shared_lock<std::shared_mutex> lock(mtx);
        if (lastBrightness.has_value() && lastBrightness.value() == brightness &&
            lastDisp.has_value() && lastDisp.value() == on)
        {
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lck(pt6964Mutex);
        sendCommand(on ? Command::ON : Command::OFF, brightness);
    }

    {
        std::lock_guard<std::shared_mutex> lock(mtx);
        lastBrightness = brightness;
        lastDisp = on;
    }
}

bool PT6964::writeMessage(const MemoryType addr,
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
    
    // If nothing has changed, then do not rewrite.
    if (!force && lastMsgSet &&
        (addr == lastAddr) &&
        (lastBrightness.has_value() && lastBrightness.value() == bright) &&
        (lastDisp.has_value() && lastDisp.value() == disp))
    {
        return false;
    }

    bool mtxUnlocked = false;

    if (force || rwMode != 1) {
        lock.unlock();
        mtxUnlocked = true;
        {
            std::lock_guard<std::mutex> lck(pt6964Mutex);

            // Write initialization commands.
            sendRawCommand(getAction(true, true, testMode));
            sendRawCommand(getMode(this->mode)); 
        }

        std::lock_guard<std::shared_mutex> uniqueLock(mtx);
        rwMode = 1;
    }


    if (!mtxUnlocked)
        lock.unlock();

    setBrightness(disp, static_cast<uint8_t>(bright), force);

    {
        std::lock_guard<std::mutex> lck(pt6964Mutex);

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
                    continuing = false;
                    interface.setCS(true);
                    interface.setData(false);
                    interface.setCLK(false);
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
    std::lock_guard<std::shared_mutex> uniqueLock(mtx);
    lastAddr = addr;
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

} // namespace pt6964