#pragma once

#include "BleGap.h"
#include "BleGatt.h"
#include "BleObserver.h"

#include <vector>
using namespace std;


class Ble
{
public:
    static void Init();
    static void SetupShell();
    static void SetDeviceName(string name);

    using Gap = BleGap;
    using Gatt = BleGatt;

private:

    static void OnHciReady();
    static void PacketHandlerHCI(uint8_t   packet_type,
                                 uint16_t  channel,
                                 uint8_t  *packet,
                                 uint16_t  size);

};
