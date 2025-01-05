#include <algorithm>
using namespace std;

#include "Evm.h"
#include "Log.h"
#include "USB.h"
#include "USB_CDC.h"

#include "tusb.h"

#include "StrictMode.h"


/////////////////////////////////////////////////////////////////////
// Instance
/////////////////////////////////////////////////////////////////////

USB_CDC::USB_CDC(uint8_t instance, Timeline &t)
: instance_(instance)
, t_(t)
{
    // this doesn't seem to actually reserve, not sure why, maybe because it's
    // getting executed during static init and not working as a result?
    // sendBuf_.reserve(SEND_BUF_SIZE);
}

// set buffering capacity, 0 to disable
void USB_CDC::SetSendBufCapacity(uint16_t capacity)
{
    sendBufCapacity_ = capacity;
}

void USB_CDC::SetCallbackOnRx(std::function<void(vector<uint8_t> &byteList)> fn)
{
    cbFnRx_ = fn;
}

bool USB_CDC::GetDtr()
{
    return dtr_;
}


/////////////////////////////////////////////////////////////////////
// CDC Callback Handlers
/////////////////////////////////////////////////////////////////////

void USB_CDC::tud_cdc_rx_cb()
{
    Evm::QueueWork("USB_CDC::tud_cdc_rx_cb", [this]{
        uint32_t bytesAvailable = tud_cdc_n_available(instance_);

        if (bytesAvailable)
        {
            vector<uint8_t> byteList(bytesAvailable);

            tud_cdc_n_read(instance_, byteList.data(), bytesAvailable);

            cbFnRx_(byteList);
        }

        // update stats
        stats_.rxBytes += bytesAvailable;
    });
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
    SendFromQueue();
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

    if (dtr_ == false)
    {
        Clear();
    }
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

// Send will take as many bytes as possible to send, queuing if necessary.
// The queue can overflow.
//
// Send tries to send immediately, which TinyUSB can handle ~1,000 bytes of
// and will parcel out over usb in 64 byte chunks.
//
// TinyUSB will call back the application on send complete, at which point
// we send more data from our queue.
//
// The observed rate of getting callbacks from a not busy application looks
// to be around every ~200us. So 320,000 bytes/sec. Eyeballing the actual rate
// on a chunky send I see it's around 250,000 bytes/sec. Good enough.
// (that's running at 48MHz, and higher clock speeds didn't noticeably change).
//
// Recall that TinyUSB is a cooperative thread, so only can run when another
// thread is not running, so a busy application can slow down the rate that
// TinyUSB can send.
//
// SendFromQueue - More - SendFromQueue - More:   0 ms,     197 us - 0:03:54.626450
// SendFromQueue - More - SendFromQueue - More:   0 ms,     201 us - 0:03:54.626651
// SendFromQueue - More - SendFromQueue - More:   0 ms,     178 us - 0:03:54.626829
// SendFromQueue - More - SendFromQueue - More:   0 ms,     248 us - 0:03:54.627077
// SendFromQueue - More - SendFromQueue - More:   0 ms,     188 us - 0:03:54.627265
// SendFromQueue - More - SendFromQueue - More:   0 ms,     198 us - 0:03:54.627463
// SendFromQueue - More - SendFromQueue - More:   0 ms,     174 us - 0:03:54.627637
// SendFromQueue - More - SendFromQueue - Done:   0 ms,     204 us - 0:03:54.627841
uint16_t USB_CDC::Send(const uint8_t *buf, uint16_t bufLen)
{
    uint16_t bytesSent = 0;

    t_.Event("Send");

    // protect using DTR just for the purpose of avoiding any possible queuing.
    // the act of sending when not connected seems to work fine on the tusb side,
    // I'm strictly protecting against big writes possibly queuing (which would)
    // also probably be fine, didn't test whether I would get callbacks, but
    // maybe/probably would? Dunno, don't need to know.
    if (GetDtr() && buf && bufLen)
    {
        const uint8_t *bufQueue    = nullptr;
        uint16_t       bufQueueLen = 0;

        if (sendBuf_.size() == 0)
        {
            // try to send immediately
            bytesSent = SendImmediate(buf, bufLen);

            // if any data left over, queue it
            bufQueue    = buf    + bytesSent;
            bufQueueLen = bufLen - bytesSent;
        }
        else
        {
            // there is already data queued, put this on the queue
            bufQueue    = buf;
            bufQueueLen = bufLen;
        }

        // queue whatever remains
        if (bufQueue && bufQueueLen)
        {
            // this really only happens once
            sendBuf_.reserve(sendBufCapacity_);

            // calculate how much to queue
            uint16_t queueBytesAvailable = (uint16_t)(sendBuf_.capacity() - sendBuf_.size());
            uint16_t bytesToQueue = min(bufQueueLen, queueBytesAvailable);

            // queue
            for (int i = 0; i < bytesToQueue; ++i)
            {
                sendBuf_.push_back(bufQueue[i]);
            }

            bytesSent += bytesToQueue;

            // update stats
            stats_.txBytesQueuedTotal += bytesToQueue;
            if (sendBuf_.size() > stats_.txBytesQueuedMaxAtOnce)
            {
                stats_.txBytesQueuedMaxAtOnce = sendBuf_.size();
            }
            if (bytesToQueue < bufQueueLen)
            {
                stats_.txBytesOverflow += (bufQueueLen - bytesToQueue);
            }
        }
    }

    // update stats
    stats_.txBytes += bytesSent;

    return bytesSent;
}

void USB_CDC::Clear()
{
    tud_cdc_n_write_clear(instance_);
    sendBuf_.clear();
}

void USB_CDC::ReportStats()
{
    uint8_t pct = (uint8_t)round(stats_.txBytesQueuedMaxAtOnce * 100.0 / sendBufCapacity_);

    Log("RX Bytes: ", Commas(stats_.rxBytes));
    Log("TX Bytes: ", Commas(stats_.txBytes));
    Log("  Queued Total      : ", Commas(stats_.txBytesQueuedTotal));
    Log("  Queued Max At Once: ", Commas(stats_.txBytesQueuedMaxAtOnce), " / ", Commas(sendBufCapacity_), " (", pct, " %)");
    Log("  Overflow          : ", Commas(stats_.txBytesOverflow));
}


/////////////////////////////////////////////////////////////////////
// CDC Private Interface
/////////////////////////////////////////////////////////////////////

void USB_CDC::SendFromQueue()
{
    if (sendBuf_.size())
    {
        uint16_t bytesSent = SendImmediate(sendBuf_.data(), (uint16_t)sendBuf_.size());

        if (bytesSent == sendBuf_.size())
        {
            t_.Event("SendFromQueue - Done");

            sendBuf_.clear();
        }
        else if (bytesSent)
        {
            t_.Event("SendFromQueue - More");

            // remove bytesSent bytes from start of buffer
            sendBuf_.erase(sendBuf_.begin(), sendBuf_.begin() + bytesSent);
        }
    }
}

uint16_t USB_CDC::SendImmediate(const uint8_t *buf, uint16_t bufLen)
{
    uint16_t bufMin = (uint16_t)min((uint32_t)bufLen, tud_cdc_n_write_available(instance_));

    tud_cdc_n_write(instance_, buf, bufMin);
    tud_cdc_n_write_flush(instance_);

    return bufMin;
}
