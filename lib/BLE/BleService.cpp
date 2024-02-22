#include "BleService.h"

#include "StrictMode.h"


BleService::BleService(string name, string uuid)
: name_(name)
, uuid_(uuid)
{
    // Nothing to do
}

BleCharacteristic &BleService::GetOrMakeCharacteristic(string name,
                                                       string uuid,
                                                       string properties)
{
    if (ctcMap_.contains(name) == false)
    {
        ctcMap_.emplace(name, BleCharacteristic{ name, uuid, properties} );
    }

    return ctcMap_.at(name);
}

map<string, BleCharacteristic> &BleService::GetCtcList()
{
    return ctcMap_;
}
