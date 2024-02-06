#pragma once

#include <functional>
#include <string>
#include <map>
using namespace std;

#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/addr.h>
#include <bluetooth/services/nus.h>

#include "Log.h"
#include "UUID.h"


// Check this out for working out how the API works for various functions
// https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/bluetooth/shell/bt.c


class BlePeripheral;
class BleBroadcaster;
class BleObserver;


class Ble
{
public:
    static void Init();
    static void SetupShell();
    static BlePeripheral &CreatePeripheral();
    static BleBroadcaster &CreateBroadcaster();
    static BleObserver &CreateObserver();
    static void SetTxPowerAdvertising(uint8_t pct);
    static void Start();
    static void Stop();


    /////////////////////////////////////////////////////////////////////
    // Utilities
    /////////////////////////////////////////////////////////////////////

    static string AddrToStr(const bt_addr_le_t *address);


private:
    static vector<BlePeripheral>  peripheralList_;
    static vector<BleBroadcaster> broadcasterList_;
    static vector<BleObserver>    observerList_;

    inline static bool started_ = false;
};


#include "BlePeripheral.h"
#include "BleBroadcaster.h"
#include "BleObserver.h"




