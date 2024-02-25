#pragma once

#include "BleService.h"


class BleGatt
{
public:
    static void Init(string name, vector<BleService> &periphList);
    static void OnReady();
    static void SetupShell();
};
