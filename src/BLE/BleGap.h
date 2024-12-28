#pragma once

#include "BleAdvertisingDataFormatter.h"
#include "BleService.h"
#include "Log.h"
#include "Shell.h"
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

        OnStateChange();
    }


    /////////////////////////////////////////////////////////////////
    // Configure when to advertise
    /////////////////////////////////////////////////////////////////
    
    static void SetNonConnectedAdvertising(bool connectable, bool scannable)
    {
        connectable_ = connectable;
        scannable_   = scannable;
        advertise_   = connectable_ || scannable_;

        OnStateChange();
    }

    static void SetConnectedAdvertising(bool connectable, bool scannable)
    {
        connectableDuringConnection_ = connectable;
        scannableDuringConnection_   = scannable;
        advertiseDuringConnection_   = connectableDuringConnection_ || scannableDuringConnection_;

        OnStateChange();
    }


    /////////////////////////////////////////////////////////////////
    // Configure advertising rate
    /////////////////////////////////////////////////////////////////

    // Must be in the range of 20ms - 10.2s
    static void SetAdvertisingInterval(uint16_t intervalMinMs, uint16_t intervalMaxMs = 0)
    {
        static const uint16_t INTERVAL_MIN =     20;
        static const uint16_t INTERVAL_MAX = 10'200;

        intervalMinMs_ = Clamp(INTERVAL_MIN,   intervalMinMs, INTERVAL_MAX);
        intervalMaxMs_ = Clamp(intervalMinMs_, intervalMinMs, INTERVAL_MAX);

        Log("ATT Ad Rate Change: ", intervalMinMs_, " ms - ", intervalMaxMs_, " ms");
        LogNL();

        OnStateChange();
    }


    /////////////////////////////////////////////////////////////////
    // Configure how to populate advertisements
    /////////////////////////////////////////////////////////////////
    
    static void SetAutoPopulateAdvertisment(bool val)
    {
        autoPopulateAdvertisement_ = val;

        OnStateChange();
    }

    static void SetAutoPopulateScanResponseOnly(bool val)
    {
        autoPopulateScanResponseOnly_ = val;

        OnStateChange();
    }

    static void SetAdvertisementBuffer(const vector<uint8_t> &byteList)
    {
        byteListAdv_.reserve(BleAdvertisingDataFormatter::ADV_BYTES);
        
        uint8_t len = min(byteListAdv_.capacity(), byteList.size());

        byteListAdv_.clear();
        for (uint8_t i = 0; i < len; ++i)
        {
            byteListAdv_.push_back(byteList[i]);
        }

        OnStateChange();
    }

    static void SetScanResponseBuffer(const vector<uint8_t> &byteList)
    {
        byteListSr_.reserve(BleAdvertisingDataFormatter::ADV_BYTES);

        uint8_t len = min(byteListSr_.capacity(), byteList.size());

        byteListSr_.clear();
        for (uint8_t i = 0; i < len; ++i)
        {
            byteListSr_.push_back(byteList[i]);
        }

        OnStateChange();
    }


    /////////////////////////////////////////////////////////////////
    // Init
    /////////////////////////////////////////////////////////////////
    
    static void Init(string name, vector<BleService> &svcList)
    {
        Timeline::Global().Event("BleGap::Init");

        if (autoPopulateAdvertisement_)
        {
            byteListAdv_.clear();
            byteListSr_.clear();

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
                Log("ATT: Configuring ", srOnly ? "Advertising Scan Response (only)" : "Advertising");

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
                for (auto &svc : svcList)
                {
                    uuidList.push_back(svc.GetUuid());
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
            if (webAddress_ != "")
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

            OnStateChange();
        }
    }

    static void Report()
    {
        LogNL();
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


private:

    /////////////////////////////////////////////////////////////////
    // Runtime Events
    /////////////////////////////////////////////////////////////////

    static void OnReady()
    {
        Timeline::Global().Event("BleGap::OnReady");

        StartAdvertising();
        Report();
    }

    // assumes single connection
    static void OnConnection()
    {
        connectedNow_ = true;

        OnStateChange();

        // no idea why this is necessary
        Evm::QueueWork("", []{
            GapEnableHci();
        });
    }

    // assumes single connection
    static void OnDisconnection()
    {
        connectedNow_ = false;

        // no idea why this is necessary
        Evm::QueueWork("", []{
            OnStateChange();
        });
    }

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
    static void OnStateChange()
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

        SetParams();
        SetData();
    }

    static void SetParams()
    {
        uint16_t adv_int_min = (uint16_t)((double)intervalMinMs_ / 0.625);
        uint16_t adv_int_max = (uint16_t)((double)intervalMaxMs_ / 0.625);

        uint8_t adv_type = (uint8_t)advType_;

        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    }

    static void SetData()
    {
        gap_advertisements_set_data((uint8_t)byteListAdv_.size(), (uint8_t *)byteListAdv_.data());
        gap_scan_response_set_data((uint8_t)byteListSr_.size(), (uint8_t *)byteListSr_.data());
    }

    static void GapEnable()
    {
        if (advType_ == AdvertisingType::NONE) return;
        gap_advertisements_enable(true);
    }

    static void GapEnableHci()
    {
        if (advType_ == AdvertisingType::NONE) return;
        hci_send_cmd(&hci_le_set_advertise_enable, 1);
    }

    static void StartAdvertising()
    {
        Timeline::Global().Event("BleGap::StartAdvertising");

        SetParams();
        SetData();
        GapEnable();
    }

    static void StopAdvertising()
    {
        gap_advertisements_enable(false);
    }

    static void SetupShell()
    {
        // configure

        Shell::AddCommand("ble.gap.set.nc", [](vector<string> argList){
            SetNonConnectedAdvertising(argList[0] == "true", argList[1] == "true");
        }, { .argCount = 2, .help = "Set Non-Connected Advertising <connectable> <scannable>" });

        Shell::AddCommand("ble.gap.set.c", [](vector<string> argList){
            SetConnectedAdvertising(argList[0] == "true", argList[1] == "true");
        }, { .argCount = 2, .help = "Set Connected Advertising <connectable> <scannable>" });

        Shell::AddCommand("ble.gap.interval", [](vector<string> argList){
            uint16_t minMs = atoi(argList[0].c_str());
            uint16_t maxMs = atoi(argList[1].c_str());

            SetAdvertisingInterval(minMs, maxMs);
        }, { .argCount = 2, .help = "Set Ad Interval <minMs> [<maxMs>]" });


        // start / stop

        Shell::AddCommand("ble.gap.start", [](vector<string> argList){
            Log("BleGap::StartAdvertising");
            StartAdvertising();
        }, { .argCount = 0, .help = "Start Advertising" });

        Shell::AddCommand("ble.gap.stop", [](vector<string> argList){
            Log("BleGap::StopAdvertising");
            StopAdvertising();
        }, { .argCount = 0, .help = "Stop Advertising" });


        // change ad data at runtime

        Shell::AddCommand("ble.gap.pri.data", [](vector<string> argList){
            string &name = argList[0];
            
            BleAdvertisingDataFormatter f;
            f.AddFlags();
            // f.AddName(name);
            f.AddMfrData(ToByteList(name));
            byteListAdv_ = f.GetData();

            gap_advertisements_set_data((uint8_t)byteListAdv_.size(), (uint8_t *)byteListAdv_.data());
        }, { .argCount = 1, .help = "set primary name = <x>" });

        Shell::AddCommand("ble.gap.sec.data", [](vector<string> argList){
            string &webAddress = argList[0];
            
            BleAdvertisingDataFormatter f;
            f.AddWebAddress(webAddress);
            byteListSr_ = f.GetData();

            gap_scan_response_set_data((uint8_t)byteListSr_.size(), (uint8_t *)byteListSr_.data());
        }, { .argCount = 1, .help = "set secondary webpage = <x>" });
    }


private:

    // misc specific advertising
    inline static string webAddress_;

    // advertising interval
    inline static uint16_t intervalMinMs_ = 500;
    inline static uint16_t intervalMaxMs_ = 500;

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

    // the advertising method (in ble terms) to use
    inline static AdvertisingType advType_ = AdvertisingType::ADV_IND;

    // controls the auto-population of advertising data (default on)
    inline static bool autoPopulateAdvertisement_ = true;
    inline static bool autoPopulateScanResponseOnly_ = true;

    // buffers to hold onto advertising data used by btstack
    inline static vector<uint8_t> byteListAdv_;
    inline static vector<uint8_t> byteListSr_;
};





/*

Testing notes

I want to be able to:

- have dynamic primary data (sync)
- have dynamic secondary data (name change)
- stay advertising during connection


on run, init does nothing, OnReady does nothing
- this leaves no ad data configured
- this leaves advertising not started



test1.1 - can you have dynamic primary data?
-------------------------------------------
pri.data DOUG
params
enable true
[check ad working] - yes
pri.data DOUG2
[check if ad changed] - yes!


test1.2 - continue from test1 - have dyn sec data?
---------------------------------------------------
sec.data glow.bike
[did this show up?] - yes
sec.data reuters.com
[did this change?] - yes


test 1.3 - resume ad from connection?
-------------------------------------
[connect]
[does ad stop?] - yes (expected)
enable true
[does ad start?] - no (expected)
enable.hci true
[does ad start?] - yes (expected)


test 1.4 - dynamic data still during connection?
------------------------------
pri.data DOUG3
sec.data theonion.com
[did ad change?] - yes


test 1.5 - dynamic data still after disconnect?
-------------------------------
pri.data DOUG4
sec.data twitter.com
[did ad change?] - yes

test 1.6 - process works through another connection?
-------------------------------
[connect]
enable.hci true
pri.data DOUG5
[working still?] - yes


test 2.1 - change the connection ad type dynamically?
-------------------------------
[want to continue advertising, but not connectable]
sys.reset
pri.data DOUG6
params
enable true
[connect]
params2
enable.hci true
[ad back with no connect?] - yes!


test 2.2 - change back to connectable on disconnect?
--------------------------------
[disconnect]
params
[did that change it?] - yes, so can change any time it seems



test 3.1 - does advertising come back automatically on disconnect?
--------------------
sys.reset
pri.data DOUG7
params
enable true
[connect]
[disconnect]
[is ad back?] - yes


test 3.2 - does ad come back automatically even if you forced it back on previously?
----------------------
[connect]
enable.hci true
[disconnect]
[connect]
[disconnect]
[is ad back?] - yes




--------------------------------------------




how timely is change
  I don't want leader to change and a delay to when followers do
does everyone receive at the same time?
  I want all follower lights to change at the same time



I need to build observer functionality and test
  do I need to do anything in observer to get scan response?









*/