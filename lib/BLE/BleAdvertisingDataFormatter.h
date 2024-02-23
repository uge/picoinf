#pragma once

#include "Log.h"
#include "Utl.h"
#include "UUID.h"

#include <vector>
using namespace std;


///////////////////////////////////////////////////////////////////////////
// Advertising Data Packing
//
// format is STV (size, type, value)
// size does not count itself, but does count type + value
///////////////////////////////////////////////////////////////////////////

class BleAdvertisingData
{
public:

    static const uint8_t ADV_BYTES = 31;


public:
    BleAdvertisingData(uint8_t capacity = ADV_BYTES)
    : capacity_(capacity)
    {
        byteList_.reserve(ADV_BYTES);
    }

    bool AddRecord(uint8_t type, uint8_t dataLen, uint8_t *data)
    {
        bool retVal = CanFitRecordDataSize(dataLen);

        if (retVal)
        {
            byteList_.push_back(dataLen + 1);
            byteList_.push_back(type);

            for (int i = 0; i < dataLen; ++i)
            {
                byteList_.push_back(data[i]);
            }
        }

        // Log("Adding record type ", type, ", size ", dataLen, " - ", retVal ? "OK" : "ERR");
        // if (retVal)
        // {
        //     LogBlob(&byteList_.data()[byteList_.size() - (dataLen + 2)], dataLen + 2);
        // }

        return retVal;
    }

    const vector<uint8_t> &GetData()
    {
        return byteList_;
    }

    void Reset()
    {
        byteList_.clear();
    }

    uint8_t GetRecordDataRemaining()
    {
        uint8_t retVal = 0;

        if (BytesRemaining() >= 2)
        {
            retVal = BytesRemaining() - 2;
        }

        return retVal;
    }

private:

    bool CanFitRecordDataSize(uint8_t dataLen)
    {
        uint8_t totalSize = 2 + dataLen;

        return totalSize <= BytesRemaining();
    }

    uint8_t BytesRemaining()
    {
        uint8_t retVal = 0;

        if (byteList_.size() <= capacity_)
        {
            retVal = capacity_ - (uint8_t)byteList_.size();
        }

        return retVal;
    }


private:

    uint8_t capacity_;
    vector<uint8_t> byteList_;
};



///////////////////////////////////////////////////////////////////////////
// Advertising Data Packing
///////////////////////////////////////////////////////////////////////////

class BleAdvertisingDataFormatter
: private BleAdvertisingData
{
public:
    BleAdvertisingDataFormatter(uint8_t capacity = BleAdvertisingData::ADV_BYTES)
    : BleAdvertisingData(capacity)
    {
        // Nothing to do
    }

    // expose just this function from base
    using BleAdvertisingData::GetData;

    bool AddFlags()
    {
        static const uint8_t BT_LE_AD_GENERAL  = 0x02;
        static const uint8_t BT_LE_AD_NO_BREDR = 0x04;
        static const uint8_t BT_DATA_FLAGS     = 0x01;

        // Here's some flags explaining that I'm BLE and not Bluetooth.
        uint8_t flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
        
        return AddRecord(BT_DATA_FLAGS, 1, &flags);
    }

    bool AddName(string name)
    {
        static const uint8_t BT_DATA_NAME_COMPLETE = 0x09;

        return AddRecord(BT_DATA_NAME_COMPLETE,
                         (uint8_t)name.length(),
                         (uint8_t *)name.c_str());
    }

    // Assumes the uuidList is all the same UUID bit size
    bool PackUuids(vector<UUID> &uuidList,
                   uint8_t       typeIfAllFit,
                   uint8_t       typeIfSomeFit)
    {
        bool retVal = false;

        if (uuidList.size())
        {
            // Validate all UUIDs are the same bit size
            int byteCountPerUuid = uuidList[0].GetBitCount() / 8;
            for (auto &uuid : uuidList)
            {
                int byteCount = uuid.GetBitCount() / 8;

                if (byteCount != byteCountPerUuid)
                {
                    Log("ERR: PackUuids - Not all UUIDs the same size");
                    return false;
                }
            }

            // Determine how many can fit
            uint8_t countCanFit = GetRecordDataRemaining() / (uint8_t)(uuidList[0].GetBitCount() / 8);

            // Determine if they can all fit for flagging in advertising
            uint8_t type = (countCanFit >= uuidList.size()) ? typeIfAllFit : typeIfSomeFit;

            // Start packing them in
            int countToPack = min((uint8_t)uuidList.size(), countCanFit);
            vector<uint8_t> byteList;

            if (countToPack)
            {
                for (uint8_t i = 0; i < countToPack; ++i)
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

                retVal = AddRecord(type, (uint8_t)byteList.size(), byteList.data());
            }

            retVal = retVal && countToPack == (int)uuidList.size();
        }
        else
        {
            retVal = true;
        }

        return retVal;
    }

    bool AddUuidList(const vector<string> &advUuidStrList)
    {
        // size_t len = advUuidStrList.size();
        // if (len)
        // {
        //     LogNNL("  UUID List (", len, "):");
        //     for (auto &uuidStr : advUuidStrList)
        //     {
        //         LogNNL(" ", uuidStr);
        //     }
        //     LogNL();
        // }
        // else
        // {
        //     Log("  UUID List empty, not using");
        // }

        // separate them into two buckets
        vector<UUID>   advUuid16List;
        vector<UUID>   advUuid128List;

        // separate out into 16-bit and 128-bit
        for (const string &uuidStr : advUuidStrList)
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

        static const uint8_t BT_DATA_UUID16_ALL   = 0x03;
        static const uint8_t BT_DATA_UUID16_SOME  = 0x02;
        static const uint8_t BT_DATA_UUID128_ALL  = 0x07;
        static const uint8_t BT_DATA_UUID128_SOME = 0x06;

        // Load 16-bit UUIDs first
        bool ok = true;
        ok &= PackUuids(advUuid16List,
                        BT_DATA_UUID16_ALL,
                        BT_DATA_UUID16_SOME);


        // Load 128-bit UUIDs next
        ok &= PackUuids(advUuid128List,
                        BT_DATA_UUID128_ALL,
                        BT_DATA_UUID128_SOME);
        
        return ok;
    }

    // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers/
    // https://docs.silabs.com/bluetooth/2.13/code-examples/stack-features/adv-and-scanning/adv-manufacturer-specific-data
    //
    // Notably the first two bytes are treated as a 16-bit company identifier.
    // 0xFFFF can be used as a default if you really cared.
    // For the time being, the raw bytes provided are inserted, and too bad for
    // anyone looking at it that gets confused.
    bool AddMfrData(const vector<uint8_t> &byteList)
    {
        static const uint8_t BT_DATA_MANUFACTURER_DATA = 0xFF;

        return AddRecord(BT_DATA_MANUFACTURER_DATA,
                         (uint8_t)byteList.size(),
                         (uint8_t *)byteList.data());
    }

    bool AddWebAddress(const string &webAddress)
    {
        static const uint8_t BT_DATA_URI = 0x24;

        vector<uint8_t> byteList;

        byteList.push_back(0x17);    // means "https:"
        byteList.push_back((uint8_t)'/');
        byteList.push_back((uint8_t)'/');
        for (auto c : webAddress)
        {
            byteList.push_back((uint8_t)c);
        }

        return AddRecord(BT_DATA_URI,
                         (uint8_t)byteList.size(),
                         (uint8_t *)byteList.data());
    }
};
