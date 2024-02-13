#pragma once

#include "Evm.h"
#include "KTask.h"
#include "Log.h"

#include "tusb.h"

#include <functional>
using namespace std;


class USB;

class USB_CDC
{
    friend class USB;

public:
    USB_CDC(uint8_t instance)
    : instance_(instance)
    {
        // nothing to do
    }

    void SetCallbackOnRx(function<void(vector<uint8_t> &byteList)> fn)
    {
        cbFnRx_ = fn;
    }

    bool GetDtr()
    {
        return dtr_;
    }

    void Send(const uint8_t *buf, uint16_t bufLen);
    void Clear();

private:

    void tud_cdc_rx_cb();
    void tud_cdc_tx_complete_cb();
    void tud_cdc_line_state_cb(bool dtr, bool rts);


private:
    function<void(vector<uint8_t> &byteList)> cbFnRx_ = [](vector<uint8_t> &){};

    bool dtr_ = false;

    uint8_t instance_;
};


class USB
{
public:

    static USB_CDC *GetCdcInstance(uint8_t instance)
    {
        USB_CDC *retVal = nullptr;

        if (instance < cdcList_.size())
        {
            retVal = &cdcList_[instance];
        }

        return retVal;
    }

public:

    static void tud_cdc_rx_cb(uint8_t itf);
    static void tud_cdc_tx_complete_cb(uint8_t itf);
    static void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts);



private:
    inline static vector<USB_CDC> cdcList_ = { 0 };

};

