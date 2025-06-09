#pragma once

#include "Timeline.h"

#include <cstdint>
#include <functional>
#include <vector>


/////////////////////////////////////////////////////////////////////
// USB_CDC
/////////////////////////////////////////////////////////////////////

class USB;

class USB_CDC
{
    friend class USB;

public:
    USB_CDC(uint8_t instance, Timeline &t);

    // set buffering capacity, 0 to disable
    void SetSendBufCapacity(uint16_t capacity);
    void SetCallbackOnRx(std::function<void(std::vector<uint8_t> &byteList)> fn);
    bool GetDtr();
    uint16_t Send(const uint8_t *buf, uint16_t bufLen);
    void Clear();
    void ReportStats();


private:

    void SendFromQueue();
    uint16_t SendImmediate(const uint8_t *buf, uint16_t bufLen);


private:

    void tud_cdc_rx_cb();
    void tud_cdc_tx_complete_cb();
    void tud_cdc_line_state_cb(bool dtr, bool rts);


private:

    std::function<void(std::vector<uint8_t> &byteList)> cbFnRx_ = [](std::vector<uint8_t> &){};

    bool dtr_ = false;

    std::vector<uint8_t> sendBuf_;
    uint16_t sendBufCapacity_ = 1000;

    uint8_t instance_;

    Timeline &t_;

    struct Stats
    {
        uint32_t rxBytes = 0;

        uint32_t txBytes = 0;
        uint32_t txBytesQueuedTotal = 0;
        uint32_t txBytesQueuedMaxAtOnce = 0;
        uint32_t txBytesOverflow = 0;
    };

    Stats stats_;
};
