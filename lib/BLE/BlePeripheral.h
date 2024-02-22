#pragma once

#include "BleService.h"

#include <string>
#include <map>
using namespace std;


class BlePeripheral
{
public:

    BlePeripheral(string name);
    BleService &GetOrMakeService(string name, string uuid);
    map<string, BleService> &GetServiceList();

    string GetName()
    {
        return name_;
    }

private:

    string name_;
    map<string, BleService> serviceMap_;
};


