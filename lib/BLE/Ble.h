#pragma once

#include "BleGap.h"
#include "BleGatt.h"
#include "BleObserver.h"

#include <vector>
using namespace std;


class Ble
{
public:
    static void Init();
    static void SetupShell();
    static void SetDeviceName(string name);

    using Gap = BleGap;
    using Gatt = BleGatt;
};
