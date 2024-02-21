#pragma once

#include <bluetooth/bluetooth.h>


/////////////////////////////////////////////////////////////////////
// Scanner
/////////////////////////////////////////////////////////////////////

class BleObserver;

class BleScanner
{
public:
    // call after Ble Start
    static void StartScanning(BleObserver *observer, uint8_t pctDuration = 100, uint32_t periodMs = 60);
    static void StopScanning();

private:
    static void OnDeviceFound(const bt_addr_le_t *addr,
                              int8_t              rssi,
                              uint8_t             type,
                              net_buf_simple     *ad);

private:
    static BleObserver *currentObserver_;
};


