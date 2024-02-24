#pragma once

#include "BleGap.h"
#include "BleGatt.h"
#include "BlePeripheral.h"
#include "BleObserver.h"

#include <vector>
using namespace std;


class Ble
{
public:
    static void Init();
    static void SetupShell();
    static void SetDeviceName(string name);
    static BlePeripheral &CreatePeripheral(string name);
    // static BleBroadcaster &CreateBroadcaster();
    // static BleObserver &CreateObserver();
    // static void SetTxPowerAdvertising(uint8_t pct);

    using Gap = BleGap;


private:
    inline static string name_ = "DEFAULT";
    inline static vector<BlePeripheral>  peripheralList_;
    // inline static vector<BleBroadcaster> broadcasterList_;
    // inline static vector<BleObserver>    observerList_;
};
