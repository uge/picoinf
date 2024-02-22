#pragma once

#include "BleAdvertisement.h"
#include "BlePeripheral.h"
#include "Log.h"
#include "Timeline.h"

#include "btstack.h"


class BleGap
{
public:

    static void Init(string name, vector<BlePeripheral> &periphList, string webAddress = "")
    {
        Timeline::Global().Event("BleGap::Init");

        // fill out actual advertisement data
        BleAdvertisement adv;
        adv.SetName(name);
        
        vector<string> uuidList;
        for (auto &p : periphList)
        {
            for (auto &[svcName, svc] : p.GetServiceList())
            {
                uuidList.push_back(svc.GetUuid());
            }
        }
        adv.SetAdvertisingUuidList(uuidList);

        byteListAdv_ = adv.GetRawAdvertisingDataStructure("Advertising");
        LogNL();

        // fill out scan response
        if (webAddress.size())
        {
            BleAdvertisement adv2;
            adv2.SetAdvertisingWebAddress(webAddress);

            byteListSr_ = adv2.GetRawAdvertisingDataStructure("Scan Response");

            LogNL();
        }
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
        gap_advertisements_set_data((uint8_t)byteListAdv_.size(), (uint8_t *)byteListAdv_.data());

        if (byteListSr_.size())
        {
            gap_scan_response_set_data((uint8_t)byteListSr_.size(), (uint8_t *)byteListSr_.data());
        }

        // enable
        gap_advertisements_enable(1);

        Log("BLE Advertising Started");
    }


private:
    
    inline static vector<uint8_t> byteListAdv_;
    inline static vector<uint8_t> byteListSr_;
};
