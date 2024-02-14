#include "USB.h"

#include "tusb.h"

#include <algorithm>
using namespace std;


/////////////////////////////////////////////////////////////////////
// CDC Callback Handlers
/////////////////////////////////////////////////////////////////////

void USB_CDC::tud_cdc_rx_cb()
{
    uint32_t bytesAvailable = tud_cdc_n_available(instance_);

    if (bytesAvailable)
    {
        vector<uint8_t> byteList(bytesAvailable);

        tud_cdc_n_read(instance_, byteList.data(), bytesAvailable);

        cbFnRx_(byteList);
    }
}

void USB::tud_cdc_rx_cb(uint8_t itf)
{
    if (itf < cdcList_.size())
    {
        cdcList_[itf].tud_cdc_rx_cb();
    }
}

// Invoked when received new data
void tud_cdc_rx_cb(uint8_t itf)
{
    // Log("tud_cdc_rx_cb");
    USB::tud_cdc_rx_cb(itf);
}

void USB_CDC::tud_cdc_tx_complete_cb()
{
    // nothing to do
}

void USB::tud_cdc_tx_complete_cb(uint8_t itf)
{
    if (itf < cdcList_.size())
    {
        cdcList_[itf].tud_cdc_tx_complete_cb();
    }
}

// Invoked when a TX is complete and therefore space becomes available in TX buffer
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    // Log("tud_cdc_tx_complete_cb");
    USB::tud_cdc_tx_complete_cb(itf);
}

void USB_CDC::tud_cdc_line_state_cb(bool dtr, bool rts)
{
    dtr_ = dtr;
}

void USB::tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    if (itf < cdcList_.size())
    {
        cdcList_[itf].tud_cdc_line_state_cb(dtr, rts);
    }
}

// Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    // Log("tud_cdc_line_state_cb");
    USB::tud_cdc_line_state_cb(itf, dtr, rts);
}


/////////////////////////////////////////////////////////////////////
// CDC Interface
/////////////////////////////////////////////////////////////////////

void USB_CDC::Send(const uint8_t *buf, uint16_t bufLen)
{
    uint16_t bufMin = min((uint32_t)bufLen, tud_cdc_n_write_available(instance_));
    tud_cdc_n_write(instance_, buf, bufMin);
    tud_cdc_n_write_flush(instance_);
}

void USB_CDC::Clear()
{
    tud_cdc_n_write_clear(instance_);
}
