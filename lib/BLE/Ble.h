#pragma once

#include "BleGap.h"
#include "BleGatt.h"
#include "BlePeripheral.h"

#include <vector>
using namespace std;


class Ble
{
public:
    static void Init();
    static void SetDeviceName(string name);
    static BlePeripheral &CreatePeripheral(string name);
    // static BleBroadcaster &CreateBroadcaster();
    // static BleObserver &CreateObserver();
    // static void SetTxPowerAdvertising(uint8_t pct);


private:
    inline static string name_;
    inline static vector<BlePeripheral>  peripheralList_;
    // inline static vector<BleBroadcaster> broadcasterList_;
    // inline static vector<BleObserver>    observerList_;
};
