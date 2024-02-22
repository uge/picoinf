#pragma once

#include "BlePeripheral.h"


class BleGatt
{
public:
    static void Init(string name, vector<BlePeripheral> &periphList);
    static void OnReady();
};
