#pragma once

#include "BleConnectionEndpoint.h"
#include "BleAdvertisementSource.h"
#include "BleGatt.h"


/////////////////////////////////////////////////////////////////////
// Peripheral
/////////////////////////////////////////////////////////////////////

class Ble;

class BlePeripheral
: public BleConnectionEndpoint
, public BleAdvertisementSource
, public BleGatt
{
    friend class Ble;

private:
    // just force it to be private to can't be instantiated outside Ble
    BlePeripheral()
    {
        // default for a peripheral
        SetIsConnectable(true);
    }

public:

    // handle the overlap in base class definitions
    BlePeripheral &SetName(string name)
    {
        BleAdvertisementSource::SetName(name);
        BleConnectionEndpoint::SetName(name);

        return *this;
    }

    // intercept gatt registration to keep track of uuids.
    // this is used to automatically advertis any registered
    // services
    BleService &GetOrMakeService(string name, string uuid)
    {
        // populate the base class' actual list, can be over-ridden later
        // by calling code if they don't like this behavior
        advUuidList_.push_back(uuid);

        return BleGatt::GetOrMakeService(name, uuid);
    }
};


