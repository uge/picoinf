#pragma once

#include "BleConnectionEndpoint.h"
#include "BleAdvertisementSource.h"
#include "BleGatt.h"


/////////////////////////////////////////////////////////////////////
// Broadcaster
/////////////////////////////////////////////////////////////////////

class Ble;

class BleBroadcaster
: public BleAdvertisementSource
{
    friend class Ble;

private:
    // just force it to be private to can't be instantiated outside Ble
    BleBroadcaster() = default;

    // hide other parts of the interface which don't apply
    using BleAdvertisementSource::SetIsConnectable;
    using BleAdvertisementSource::GetIsConnectable;
};


