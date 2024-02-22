#pragma once

#include "BleAdvertisement.h"
#include "Log.h"
#include "Timeline.h"

#include "btstack.h"


class BleGap
{
public:

    static void Init(string name)
    {
        Timeline::Global().Event("BleGap::Init");

        // fill out actual advertisement data
        BleAdvertisement adv;
        adv.SetName(name);
        adv.SetAdvertisingUuidList({
            "0x181A",
        });
        byteList_ = adv.GetRawAdvertisingDataStructure();
    }

    static void OnReady()
    {
        Timeline::Global().Event("BleGap::OnReady");
        
        StartAdvertising();
    }


private:

    static void StartAdvertising()
    {
        Timeline::Global().Event("BleGap::StartAdvertising");

        // setup advertisements
        uint16_t adv_int_min = 800;
        uint16_t adv_int_max = 800;
        uint8_t adv_type = 0;
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);

        // setup att database        
        gap_advertisements_set_data((uint8_t)byteList_.size(), (uint8_t *)byteList_.data());

        // enable
        gap_advertisements_enable(1);

        Log("BLE Advertising Started");
    }


private:
    
    inline static vector<uint8_t> byteList_;
};
