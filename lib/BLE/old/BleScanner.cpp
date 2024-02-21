#include "Log.h"

#include "BleScanner.h"
#include "BleObserver.h"


void BleScanner::StartScanning(BleObserver *observer, uint8_t pctDuration, uint32_t periodMs)
{
    if (currentObserver_ == nullptr)
    {
        currentObserver_ = observer;

        // clamp to 100%
        pctDuration = min(pctDuration, (uint8_t)100);

        uint16_t interval = periodMs / 0.625;
        uint16_t window   = ((double)pctDuration / 100.0 * interval);

        Log("Starting BleScanner");

        bt_le_scan_param scanParam = {
            .type       = BT_LE_SCAN_TYPE_PASSIVE,          // don't do scan request for more info
            .options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
            .interval   = interval,
            .window     = window,
        };

        int err = bt_le_scan_start(&scanParam, OnDeviceFound);
        if (err) {
            Log("Starting scanning failed err ", err);
        }
    }
    else
    {
        Log("ERR: BleScanner StartScanning clash, new ",
            observer,
            " tried to scan while ",
            currentObserver_,
            " in use, ignoring");
    }
}

void BleScanner::StopScanning()
{
    currentObserver_ = nullptr;

    Log("Stopping BleScanner");

    bt_le_scan_stop();
}

void BleScanner::OnDeviceFound(const bt_addr_le_t *addr,
                               int8_t              rssi,
                               uint8_t             type,
                               net_buf_simple     *ad)
{
    if (currentObserver_)
    {
        currentObserver_->OnDeviceFound(addr, rssi, type, ad);
    }
}


BleObserver *BleScanner::currentObserver_ = nullptr;