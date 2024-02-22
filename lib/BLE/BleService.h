#pragma once

#include "BleCharacteristic.h"

#include <string>
#include <map>
using namespace std;


class BleService
{
public:

    BleService(string name, string uuid);
    BleCharacteristic &GetOrMakeCharacteristic(string name,
                                               string uuid,
                                               string properties);
    map<string, BleCharacteristic> &GetCtcList();

    string GetName()
    {
        return name_;
    }

    string GetUuid()
    {
        return uuid_;
    }

private:

    string name_;
    string uuid_;
    map<string, BleCharacteristic> ctcMap_;
};
