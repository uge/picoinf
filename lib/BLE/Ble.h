#pragma once

#include "BleGap.h"
#include "BleGatt.h"
// #include "BlePeripheral.h"


class Ble
{
public:
    static void Init();
    static void SetupShell();
    // static BlePeripheral &CreatePeripheral();
    // static BleBroadcaster &CreateBroadcaster();
    // static BleObserver &CreateObserver();
    // static void SetTxPowerAdvertising(uint8_t pct);


private:
    // inline static vector<BlePeripheral>  peripheralList_;
    // inline static vector<BleBroadcaster> broadcasterList_;
    // inline static vector<BleObserver>    observerList_;
};
