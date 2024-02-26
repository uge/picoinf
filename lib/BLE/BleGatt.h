#pragma once

#include "BleService.h"

class Ble;


class BleGatt
{
    friend class Ble;

public:
    static void Init(string name, vector<BleService> &periphList);
    static void SetupShell();

private:
    static void OnReady();
};
