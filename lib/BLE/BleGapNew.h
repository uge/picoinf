#pragma once

#include "BleAdvertisement.h"

#include "btstack.h"


class BleGap
{
public:

    static void Init()
    {
        
    }

    static void OnReady()
    {
        // setup advertisements
        uint16_t adv_int_min = 800;
        uint16_t adv_int_max = 800;
        uint8_t adv_type = 0;
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);

        // fill out actual advertisement data
        BleAdvertisement adv;
        adv.SetName("GLOW_BIKE");
        adv.SetAdvertisingUuidList({
            "0x181A",
        });
        static const vector<uint8_t> byteList = adv.GetRawAdvertisingDataStructure();
        gap_advertisements_set_data(byteList.size(), (uint8_t *)byteList.data());

        // enable
        gap_advertisements_enable(1);
    }


private:
};
