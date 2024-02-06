#pragma once

#include <string>
using namespace std;

#include "Log.h"
#include "Shell.h"
#include "UUID.h"
#include "Utl.h"
#include "Timeline.h"
#include "BleAdvertisementSource.h"
#include "BleConnectionManager.h"


// maybe look at this at some point
// https://github.com/google/eddystone


// forward decl
class Ble;

class BleAdvertiser
{
    friend class Ble;
    friend class BleConnectionManager;
    
    struct AdvData
    {
        BleAdvertisementSource *bleAdvert = nullptr;
        BleConnectionEndpoint *bce = nullptr;

        bt_le_adv_param adParam;

        uint8_t flags = 0;
        vector<uint8_t> byteUuid16List;
        vector<uint8_t> byteUuid128List;
        vector<uint8_t> bytesWebAddress;
    };

    struct ConnData
    {
        BleAdvertisementSource *bleAdvert = nullptr;
        bt_le_ext_adv *adv = nullptr;
    };

public:

    static void SetPowerAdv(double pct)
    {
        timeline_.Event(__func__);

        LogNL();
        Log("Applying tx power setting to Advertising");

        int dBmBefore = BleHci::GetTxPowerAdv();
        int dBmSet    = BleHci::SetTxPowerAdv(pct);
        int dBmAfter  = BleHci::GetTxPowerAdv();

        LogNNL("  Adv was(", dBmBefore, " dBm)");
        LogNNL(", req(", pct, "% = ", dBmSet, " dBm)");
        LogNNL(", now(", dBmAfter, " dBm)");
        LogNL();
    }

    static void Register(vector<pair<BleAdvertisementSource *, BleConnectionEndpoint *>> bleAdvertList)
    {
        timeline_.Event(__func__);

        for (size_t i = 0; i < bleAdvertList.size(); ++i)
        {
            auto [bleAdvert, bce] = bleAdvertList[i];

            RegisterAdvert(*bleAdvert, bce);
        }
    }


private:

    ///////////////////////////////////////////////////////////////////////////
    // Registration
    ///////////////////////////////////////////////////////////////////////////

    static void RegisterAdvert(BleAdvertisementSource &bleAdvert,
                               BleConnectionEndpoint  *bce)
    {
        LogNL();
        Log("Registering ", bleAdvert.GetName());

        int err;

        // Allocate a place to store this advertisement set
        bt_le_ext_adv **advPtr = advPool_.Borrow();
        if (advPtr == nullptr)
        {
            Log("ERR: RegisterAdvert: Cannot allocate advertising set for ", bleAdvert.GetName());
            return;
        }


        // Fill out advertising parameters
        uint32_t options = 0;
        if (bleAdvert.GetIsConnectable())
        {
            Log("  It's connectable");
            options |= BT_LE_ADV_OPT_CONNECTABLE;
        }
        else
        {
            Log("  It's not connectable");
        }
        bt_le_adv_param adParam = {
            .id = BT_ID_DEFAULT,
            .sid = 0,
            .secondary_max_skip = 0,
            .options = options,
            .interval_min = 160,
            .interval_max = 240,
            .peer = nullptr,
        };


        // create advertising set, using the allocated space
        err = bt_le_ext_adv_create(&adParam, &advCallbacks_, advPtr);
        if (err)
        {
            Log("ERR: RegisterAdvert could not create ADV: ", err);
        }


        // keep a mapping between the index and the object it came from, so
        // the connection manager can be notified as well as given an object
        // to work with
        bt_le_ext_adv *adv = *advPtr;
        AdvData &advData = advMap_[adv];
        advData = {
            .bleAdvert = &bleAdvert,
            .bce       = bce,

            .adParam = adParam,
        };


        // re-issue configuration, this time with storage which is per-advertiser.
        // ideally this would have been done sooner, but that wasn't possible
        // due to not yet having adv resolved at the time of initial configuration.
        bt_le_ext_adv_update_param(adv, &advData.adParam);


        // Create the content of the advertisement - 31 bytes of useful data.
        // Each record is 2 bytes plus payload size.
        // The data pointed to by the .data member must be long-living.
        vector<bt_data> advertisingData;
        uint8_t bytesRemainingAdvertisingData = 31;
        // vector<bt_data> scanResponseData;
        // uint8_t bytesRemainingScanResponse = 31;

        // put flags and name in primary advertising data
        FillOutFlags(advData, advertisingData, bytesRemainingAdvertisingData);
        FillOutName(advData, advertisingData, bytesRemainingAdvertisingData);

        // use scan response for uuids and other data
        FillOutUuid(advData, advertisingData, bytesRemainingAdvertisingData);
        FillOutMfrData(advData, advertisingData, bytesRemainingAdvertisingData);
        FillOutWebAddress(advData, advertisingData, bytesRemainingAdvertisingData);


        // set advertising data
        err = bt_le_ext_adv_set_data(adv,
                                     advertisingData.data(),
                                     advertisingData.size(),
                                    //  scanResponseData.data(),
                                    //  scanResponseData.size());
                                     nullptr,
                                     0);
        if (err)
        {
            Log("ERR: RegisterAdvert could not set ADV data: ", err);
        }

        // start advertising
        StartAdvertising(bleAdvert, adv);
    }

    static void StartAdvertising(BleAdvertisementSource &bleAdvert, bt_le_ext_adv *adv)
    {
        if (bleAdvert.WantsAdvertisingToStart())
        {
            bt_le_ext_adv_start_param limits = {
                .timeout    = 0,
                .num_events = 0,
            };

            int err = bt_le_ext_adv_start(adv, &limits);
            if (err)
            {
                Log("ERR: StartAdvertising could not start ADV: ", err);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Advertising Data Packing
    ///////////////////////////////////////////////////////////////////////////

    static void FillOutFlags(AdvData &advData, vector<bt_data> &btDataList, uint8_t &bytesRemaining)
    {
        // Here's some flags explaining that I'm BLE and not Bluetooth.
        advData.flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
        btDataList.push_back({
            .type = BT_DATA_FLAGS,
            .data_len = sizeof(advData.flags),
            .data = &advData.flags,
        });

        bytesRemaining -= 2 + 1;
    }

    static void FillOutName(AdvData &advData, vector<bt_data> &btDataList, uint8_t &bytesRemaining)
    {
        // Here's my display name
        uint8_t len = (uint8_t)advData.bleAdvert->GetName().length();
        btDataList.push_back({
            .type = BT_DATA_NAME_COMPLETE,
            .data_len = len,
            .data = (uint8_t *)advData.bleAdvert->GetName().c_str(),
        });

        bytesRemaining -= 2 + len;
    }

    static void FillOutUuid(AdvData &advData, vector<bt_data> &btDataList, uint8_t &bytesRemaining)
    {
        // get any configured UUIDs
        vector<string> advUuidStrList = advData.bleAdvert->GetAdvertisingUuidList();

        size_t len = advUuidStrList.size();
        if (len)
        {
            LogNNL("  UUID List (", len, "):");
            for (auto &uuidStr : advUuidStrList)
            {
                LogNNL(" ", uuidStr);
            }
            LogNL();
        }
        else
        {
            Log("  UUID List empty, not using");
        }

        // separate them into two buckets
        vector<UUID> advUuid16List;
        vector<UUID> advUuid128List;

        // separate out into 16-bit and 128-bit
        for (string &uuidStr : advUuidStrList)
        {
            UUID uuid(uuidStr);

            if (uuid.GetBitCount() == 16)
            {
                advUuid16List.push_back(uuid);
            }
            else if (uuid.GetBitCount() == 128)
            {
                advUuid128List.push_back(uuid);
            }
            else
            {
                Log("ERR: SetAdvertisingUuidList - UUID (", uuid.GetUUID(), ") not recognized");
            }
        }

        // Load 16-bit UUIDs first
        PackUuids(btDataList,
                  bytesRemaining,
                  advUuid16List,
                  BT_DATA_UUID16_ALL,
                  BT_DATA_UUID16_SOME,
                  advData.byteUuid16List);

        // Load 128-bit UUIDs next
        PackUuids(btDataList,
                  bytesRemaining,
                  advUuid128List,
                  BT_DATA_UUID128_ALL,
                  BT_DATA_UUID128_SOME,
                  advData.byteUuid128List);
    }

    // Assumes the uuidList is all the same UUID bit size
    static void PackUuids(vector<bt_data> &btDataList,
                          uint8_t         &bytesRemaining,
                          vector<UUID>    &uuidList,
                          uint8_t          typeIfAllFit,
                          uint8_t          typeIfSomeFit,
                          vector<uint8_t> &byteList)
    {
        if (uuidList.size())
        {
            // Validate all UUIDs are the same bit size
            int byteCountPerUuid = uuidList[0].GetBitCount() / 8;
            for (auto &uuid : uuidList)
            {
                int byteCount = uuid.GetBitCount() / 8;

                if (byteCount != byteCountPerUuid)
                {
                    return;
                }
            }

            // 2 header bytes, then the byte count for whichever size UUID this is
            int countCanFit = (bytesRemaining - 2) / byteCountPerUuid;

            // Determine if they can all fit for flagging in advertising
            uint8_t type;
            if (countCanFit >= (int)uuidList.size())
            {
                // Can fit them all
                type = typeIfAllFit;
            }
            else
            {
                // Can't fit them all
                type = typeIfSomeFit;
            }

            // Start packing them in
            byteList.clear();

            int countToPack = min((int)uuidList.size(), countCanFit);

            if (countToPack)
            {
                for (int i = 0; i < countToPack; ++i)
                {
                    UUID &uuid = uuidList[i];
                    
                    uuid.ReverseBytes();
                    uint8_t *p = uuid.GetBytes();

                    for (int i = 0; i < byteCountPerUuid; ++i)
                    {
                        byteList.push_back(p[i]);
                    }

                    uuid.ReverseBytes();
                }

                btDataList.push_back({
                    .type = type,
                    .data_len = (uint8_t)byteList.size(),
                    .data = byteList.data(),
                });

                bytesRemaining -= 2 + (byteList.size());
            }
        }
    }

    // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers/
    // https://docs.silabs.com/bluetooth/2.13/code-examples/stack-features/adv-and-scanning/adv-manufacturer-specific-data
    //
    // Notably the first two bytes are treated as a 16-bit company identifier.
    // 0xFFFF can be used as a default if you really cared.
    // For the time being, the raw bytes provided are inserted, and too bad for
    // anyone looking at it that gets confused.
    static void FillOutMfrData(AdvData &advData, vector<bt_data> &btDataList, uint8_t &bytesRemaining)
    {
        const vector<uint8_t> &mfrData = advData.bleAdvert->GetAdvertisingMfrData();
        uint8_t len = mfrData.size();

        if (len)
        {
            LogNNL("  MFR Data (", len, "): ");
            LogHex(mfrData.data(), mfrData.size());
        }
        else
        {
            Log("  MFR Data empty, not using");
        }

        if (len && len <= bytesRemaining - 2)
        {
            btDataList.push_back({
                .type = BT_DATA_MANUFACTURER_DATA,
                .data_len = len,
                .data = mfrData.data(),
            });

            bytesRemaining -= 2 + len;
        }
    }

    static void FillOutWebAddress(AdvData &advData, vector<bt_data> &btDataList, uint8_t &bytesRemaining)
    {
        const string &webAddress = advData.bleAdvert->GetAdvertisingWebAddress();
        uint8_t len = webAddress.length();

        if (len)
        {
            Log("  Web Address: ", webAddress);
        }
        else
        {
            Log("  Web Address empty, not using");
        }

        // need 2 bytes for the tag
        // need 1 byte for an https indicator
        // need 2 bytes for //
        // then the rest
        uint8_t totalLen = 1 + 2 + len;

        if (len)
        {
            if (totalLen <= bytesRemaining - 2)
            {
                advData.bytesWebAddress.push_back(0x17);    // means "https:"
                advData.bytesWebAddress.push_back((uint8_t)'/');
                advData.bytesWebAddress.push_back((uint8_t)'/');
                for (auto c : webAddress)
                {
                    advData.bytesWebAddress.push_back((uint8_t)c);
                }

                btDataList.push_back({
                    .type = BT_DATA_URI,
                    .data_len = totalLen,
                    .data = advData.bytesWebAddress.data(),
                });

                bytesRemaining -= 2 + totalLen;
            }
            else
            {
                Log("    Could not pack Web Address");
            }
        }
    }


    ///////////////////////////////////////////////////////////////////////////
    // Callbacks about connections and advertising events
    ///////////////////////////////////////////////////////////////////////////

    static void OnAdSent(bt_le_ext_adv *adv, bt_le_ext_adv_sent_info *info)
    {
        timeline_.Event(__func__);
    }

    static void OnAdConnected(bt_le_ext_adv *adv,
                              bt_le_ext_adv_connected_info *info)
    {
        timeline_.Event(__func__);
        
        // Trying to route event to the Connection Manager.
        //
        // The connection manager wants to know which endpoint object
        // a given connection is associated with.
        //
        // We know that the advertisement source is also that same endpoint.
        // Convert and send to manager.

        AdvData &advData = advMap_[adv];
        bt_conn *conn = info->conn;
        BleConnectionManager::OnConnectedFromAdvertisement(conn, advData.bce, [](bt_conn *conn){
            OnAdDisconnected(conn);
        });

        // keep track of the connection's mapping to the adv data, we'll get
        // a callback from the connection manager on disconnect so we can
        // re-enable advertising
        connMap_[conn] = {
            .bleAdvert = advData.bleAdvert,
            .adv = adv,
        };

        static TimedEventHandlerDelegate ted;

        // ted.SetCallback([&]{
        //     Log("Timeout to re-enable advertising");
        //     OnAdDisconnected(conn);
        // });
        // ted.RegisterForTimedEvent(10);
    }

    static void OnAdDisconnected(bt_conn *conn)
    {
        ConnData &connData = connMap_[conn];

        Log("Connection closed, re-enabling advertising");
        StartAdvertising(*connData.bleAdvert, connData.adv);

        connMap_.erase(conn);
    }

    static void OnAdScanned(bt_le_ext_adv *adv, bt_le_ext_adv_scanned_info *info)
    {
        timeline_.Event(__func__);
    }


private:

    inline static Timeline timeline_;
    inline static ObjectPool<bt_le_ext_adv *, CONFIG_BT_EXT_ADV_MAX_ADV_SET> advPool_;
    inline static unordered_map<bt_le_ext_adv *, AdvData> advMap_;
    inline static unordered_map<bt_conn *, ConnData> connMap_;
    inline static const struct bt_le_ext_adv_cb advCallbacks_ = {
        .sent      = OnAdSent,
        .connected = OnAdConnected,
        .scanned   = OnAdScanned,
    };
    inline static bool registered_ = false;


    static void Init()
    {
        SetupFatalHandler();
    }

    static void SetupShell()
    {
        Shell::AddCommand("ble.adv.stop", [](vector<string> argList){
            // StopAdvertising();
        }, { .argCount = 0, .help = "Stop advertising", });

        Shell::AddCommand("ble.adv.start", [](vector<string> argList){
            // StartAdvertising();
        }, { .argCount = 0, .help = "Start advertising", });

        Shell::AddCommand("ble.adv.pwr", [](vector<string> argList){
            if (argList.size() == 0)
            {
                int dBm = BleHci::GetTxPowerAdv();
                Log("Adv dBm: ", dBm);
            }
            else if (argList.size() == 1)
            {
                double pct = atof(argList[0].c_str());
                SetPowerAdv(pct);
            }
        }, { .argCount = -1, .help = "See dBm / Set pct pwr for advertisements" });

        Shell::AddCommand("ble.adv.report", [](vector<string> argList){
            timeline_.Report();
        }, { .argCount = 0, .help = "Start advertising", });
    }

private:

    static void SetupFatalHandler()
    {
        PAL.RegisterOnFatalHandler("BleAdvertiser Fatal Handler", []{
            timeline_.ReportNow();
        });
    }
};


