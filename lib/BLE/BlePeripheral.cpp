#include "BlePeripheral.h"
#include "BleService.h"

#include "StrictMode.h"


BlePeripheral::BlePeripheral(string name)
: name_(name)
{
    // Nothing to do
}

BleService &BlePeripheral::GetOrMakeService(string name, string uuid)
{
    if (serviceMap_.contains(name) == false)
    {
        serviceMap_.emplace(name, BleService{ name, uuid });
    }

    return serviceMap_.at(name);
}

map<string, BleService> &BlePeripheral::GetServiceList()
{
    return serviceMap_;
}
