#pragma once

#include "BleAdvertisingDataFormatter.h"
#include "BlePeripheral.h"
#include "Log.h"
#include "Timeline.h"

#include "btstack.h"


class Ble;

class BleGap
{
    friend class Ble;

public:

    /////////////////////////////////////////////////////////////////
    // Configure some high-level display params
    /////////////////////////////////////////////////////////////////

    static void SetWebAddress(string webAddress)
    {
        webAddress_ = webAddress;

        OnChangeNowDetermineAdvertising();
    }

    /////////////////////////////////////////////////////////////////
    // Configure when to advertise
    /////////////////////////////////////////////////////////////////
    
    static void SetNonConnectedAdvertising(bool connectable, bool scannable)
    {
        connectable_ = connectable;
        scannable_   = scannable;
        advertise_   = connectable_ || scannable_;

        OnChangeNowDetermineAdvertising();
    }

    static void SetConnectedAdvertising(bool connectable, bool scannable)
    {
        connectableDuringConnection_ = connectable;
        scannableDuringConnection_   = scannable;
        advertiseDuringConnection_   = connectableDuringConnection_ || scannableDuringConnection_;

        OnChangeNowDetermineAdvertising();
    }

    /////////////////////////////////////////////////////////////////
    // Configure how to populate advertisements
    /////////////////////////////////////////////////////////////////
    
    static void SetAutoPopulateAdvertisment(bool val)
    {
        autoPopulateAdvertisement_ = val;

        OnChangeNowDetermineAdvertising();
    }

    static void SetAutoPopulateScanResponseOnly(bool val)
    {
        autoPopulateScanResponseOnly_ = val;

        OnChangeNowDetermineAdvertising();
    }

    static void SetAdvertisementBuffer(const vector<uint8_t> &byteList)
    {

        OnChangeNowDetermineAdvertising();
    }

// private:


    /////////////////////////////////////////////////////////////////
    // Pre-Runtime Init
    /////////////////////////////////////////////////////////////////
    
    static void Init(string name, vector<BlePeripheral> &periphList)
    {
        Timeline::Global().Event("BleGap::Init");

        if (autoPopulateAdvertisement_)
        {
            vector<uint8_t> *primaryTarget   = &byteListAdv_;
            vector<uint8_t> *secondaryTarget = &byteListSr_;

            BleAdvertisingDataFormatter f1;
            BleAdvertisingDataFormatter f2;
            BleAdvertisingDataFormatter *primaryFormatter   = &f1;
            BleAdvertisingDataFormatter *secondaryFormatter = &f2;

            if (autoPopulateScanResponseOnly_ == true)
            {
                primaryTarget    = &byteListSr_;
                primaryFormatter = &f2;
            }

            bool srOnly = primaryTarget == secondaryTarget;

            // Primary
            {
                Log("Configuring ", srOnly ? "Advertising Scan Response (only)" : "Advertising");

                BleAdvertisingDataFormatter &f = *primaryFormatter;
                
                bool ok;

                // Flags
                ok = f.AddFlags();
                
                if (name.size())
                {
                    ok = f.AddName(name);
                    Log("  Adding name: \"", name, "\"", ok ? "" : " (ERR)");
                }

                // UUIDs
                vector<string> uuidList;
                for (auto &p : periphList)
                {
                    for (auto &[svcName, svc] : p.GetServiceList())
                    {
                        uuidList.push_back(svc.GetUuid());
                    }
                }

                if (uuidList.size())
                {
                    ok = f.AddUuidList(uuidList);
                    Log("  Adding UUIDs: ", uuidList, ok ? "" : " (ERR)");
                }

                if (srOnly == false)
                {
                    LogNL();
                }

                Append(*primaryTarget, f.GetData());
            }

            if (srOnly)
            {
                secondaryFormatter->SetCapacity(secondaryFormatter->GetBytesRemaining());
                secondaryFormatter->Reset();
            }

            // Secondary
            {
                if (srOnly == false)
                {
                    Log("Configuring Advertising Scan Response");
                }

                BleAdvertisingDataFormatter &f = *secondaryFormatter;
                bool ok;

                // Web Address
                if (webAddress_.size())
                {
                    ok = f.AddWebAddress(webAddress_);
                    Log("  Adding Web Address: https://", webAddress_, ok ? "" : " (ERR)");
                }

                LogNL();

                Append(*secondaryTarget, f.GetData());
            }
        }
    }

    static void OnReady()
    {
        Timeline::Global().Event("BleGap::OnReady");
        
        StartAdvertising();

        // OnChangeNowDetermineAdvertising();
    }





private:

    enum class AdvertisingType : uint8_t
    {
                                  //  Connectable  |  Scannable   |  Directed  |  Notes
        ADV_IND           = 0,    //      X        |      X       |      -     | typical
        ADV_DIRECT_IND_HD = 1,    //      X        |      -       |      X     | (uncommon) high duty cycle
        ADV_SCAN_IND      = 2,    //      -        |      X       |      -     | typical non-connectable
        ADV_NONCONN_IND   = 3,    //      -        |      -       |      -     | 
        ADV_DIRECT_IND_LD = 4,    //      X        |      -       |      X     | (uncommon) low duty cycle
        NONE,                     //      -        |      -       |      -     | don't advertise
    };

    // determine the correct state of being given all the factors.
    // if we are not in that correct state, get there.
    static void OnChangeNowDetermineAdvertising()
    {
        AdvertisingType advTypeNew = AdvertisingType::ADV_IND;

        if (advertise_ && connectedNow_ == false)
        {
            if (connectable_ && scannable_)         { advTypeNew = AdvertisingType::ADV_IND;           }
            else if (connectable_)                  { advTypeNew = AdvertisingType::ADV_DIRECT_IND_HD; }
            else if (scannable_)                    { advTypeNew = AdvertisingType::ADV_SCAN_IND;      }
            else                                    { advTypeNew = AdvertisingType::ADV_NONCONN_IND;   }
        }
        else if (advertiseDuringConnection_ && connectedNow_)
        {
            if (connectableDuringConnection_ && 
                scannableDuringConnection_)         { advTypeNew = AdvertisingType::ADV_IND;           }
            else if (connectableDuringConnection_)  { advTypeNew = AdvertisingType::ADV_DIRECT_IND_HD; }
            else if (scannableDuringConnection_)    { advTypeNew = AdvertisingType::ADV_SCAN_IND;      }
            else                                    { advTypeNew = AdvertisingType::ADV_NONCONN_IND;   }
        }
        else
        {
            advTypeNew = AdvertisingType::NONE;
        }

        advType_ = advTypeNew;

        if (advertisingNow_)
        {
            StopAdvertising();
            StartAdvertising();
        }
    }

    





    static void StartAdvertising()
    {
        Timeline::Global().Event("BleGap::StartAdvertising");

        // setup advertisements
        uint16_t adv_int_min = 200;
        uint16_t adv_int_max = 200;
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
        gap_advertisements_enable(true);

        Log("BLE Advertising Started");
        Log("Connected?  |  Connectable  |  Scannable");
        Log("    No      |       ",
            connectable_ ? 'X' : '-',
            "       |      ",
            scannable_ ? 'X' : '-',
            "    ");
        Log("    Yes     |       ",
            connectableDuringConnection_ ? 'X' : '-',
            "       |      ",
            scannableDuringConnection_ ? 'X' : '-',
            "    ");
        LogNL();
    }

    static void StopAdvertising()
    {
        gap_advertisements_enable(false);
    }


private:

    // misc specific advertising
    inline static string webAddress_;

    // control whether and what advertising is done when not connected
    inline static bool advertise_ = true;
    inline static bool connectable_ = true;
    inline static bool scannable_ = true;

    // control whether and what advertising is done when connected
    inline static bool advertiseDuringConnection_ = false;
    inline static bool connectableDuringConnection_ = false;
    inline static bool scannableDuringConnection_ = false;

    // state keeping about whether connected and whether currently advertising
    inline static bool connectedNow_ = false;
    inline static bool advertisingNow_ = false;

    // the advertising method (in ble terms) to use
    inline static AdvertisingType advType_ = AdvertisingType::ADV_IND;

    // controls the auto-population of advertising data (default on)
    inline static bool autoPopulateAdvertisement_ = true;
    inline static bool autoPopulateScanResponseOnly_ = true;

    // buffers to hold onto advertising data used by btstack
    inline static vector<uint8_t> byteListAdv_;
    inline static vector<uint8_t> byteListSr_;
};
