#include "BleService.h"

#include "StrictMode.h"


BleService::BleService(string name, string uuid)
{
    // Nothing to do

    state_->name_ = name;
    state_->uuid_ = uuid;
}

BleCharacteristic &BleService::GetOrMakeCharacteristic(string name,
                                                       string uuid,
                                                       string properties)
{
    if (state_->ctcMap_.contains(name) == false)
    {
        state_->ctcMap_.emplace(name, BleCharacteristic{ name, uuid, properties} );
    }

    return state_->ctcMap_.at(name);
}

map<string, BleCharacteristic> &BleService::GetCtcList()
{
    return state_->ctcMap_;
}
