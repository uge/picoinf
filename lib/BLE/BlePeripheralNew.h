#pragma once

#include <functional>
#include <string>
using namespace std;

/////////////////////////////////////////////////////////////////////
// Characteristic
/////////////////////////////////////////////////////////////////////

class BleCharacteristic
{
    friend class BleService;
    friend class BleGatt;

public:
    using CbRead      = function<void(bt_conn *conn, uint8_t *&buf, uint16_t &bufLen, uint16_t offset)>;
    using CbWrite     = function<void(bt_conn *conn, uint8_t *buf, uint16_t bufLen, uint16_t offset, uint8_t flags)>;
    using CbSubscribe = function<void(bool isSubscribed)>;

public:

    BleCharacteristic()
    {
        // protect against crash if handlers are never set
        cbFnRead_       = [](bt_conn *conn, uint8_t *&buf, uint16_t &bufLen, uint16_t offset){};
        cbFnWrite_      = [](bt_conn *conn, uint8_t *buf, uint16_t bufLen, uint16_t offset, uint8_t flags){};
        cbFnSubscribed_ = [](bool isSubscribed){};
    }
    
    void SetProperties(uint8_t properties)
    {
        enableCCC_  = (properties & BT_GATT_CHRC_INDICATE) || (properties & BT_GATT_CHRC_NOTIFY);
        properties_ = properties;
    }

    void SetPermissions(uint8_t permissions)
    {
        permissions_ = permissions;
    }

    void SetCbRead(CbRead cbFnRead)
    {
        cbFnRead_ = cbFnRead;
    }

    void SetCbWrite(CbWrite cbFnWrite)
    {
        cbFnWrite_ = cbFnWrite;
    }

    void SetCbSubscribe(CbSubscribe cbFnSubscribe)
    {
        cbFnSubscribed_ = cbFnSubscribe;
    }

    bool IsOk()
    {
        return uuid_.GetBitCount() != 0;
    }

    void Notify(uint8_t *buf, uint16_t bufLen)
    {
        // Calculate the smallest single-shot send size for all connected clients
        uint16_t smallestBuf = fnGetSmallestBuf_();

        if (smallestBuf >= bufLen)
        {
            int retVal = bt_gatt_notify(nullptr, attr_, buf, bufLen);

            if (retVal != 0)
            {
                Log("ERR: Notify - notify error ", retVal, ", restarting");
                PAL.Reset();
            }
        }
        else
        {
            // Log("ERR: Notify - bufLen(", bufLen, "), maxBufAllowed(", smallestBuf,")");
        }
    }

private:
    string  name_;
    UUID    uuid_;
    bool    enableCCC_ = false;
    uint8_t properties_ = 0;
    uint8_t permissions_ = 0;
    CbRead  cbFnRead_;
    CbWrite cbFnWrite_;
    CbSubscribe cbFnSubscribed_;
    bt_gatt_attr *attr_;

    function<uint16_t()> fnGetSmallestBuf_ = [](){ return 0; };
};

/////////////////////////////////////////////////////////////////////
// Peripheral
/////////////////////////////////////////////////////////////////////

class BleService
{
public:

    void Configure(string uuid)
    {
        uuid_.SetUUID(uuid);
    }

    bool IsOk()
    {
        return uuid_.GetBitCount() != 0;
    }

    BleCharacteristic &GetOrMakeCharacteristic(string name, string uuid)
    {
        auto &retVal = ctcMap_[name];

        retVal.name_ = name;
        retVal.uuid_.SetUUID(uuid);
        retVal.SetPermissions(BT_GATT_PERM_READ | BT_GATT_PERM_WRITE);
        retVal.fnGetSmallestBuf_ = fnGetSmallestBuf_;

        return retVal;
    }

private:
    string name_;

    UUID uuid_;

    map<string, BleCharacteristic> ctcMap_;

    function<uint16_t()> fnGetSmallestBuf_ = [](){ return 0; };

private:
    // persistent storage for zephyr to point to
    struct {
        bt_uuid_16 btUuid16;
        union
        {
            bt_uuid_16  btUuid16UserData;
            bt_uuid_128 btUuid128UserData;
        };
    } zstorage;
};

/////////////////////////////////////////////////////////////////////
// Peripheral
/////////////////////////////////////////////////////////////////////

class BlePeripheral
{
    // handle the overlap in base class definitions
    BlePeripheral &SetName(string name)
    {
        return *this;
    }

    BleService &GetOrMakeService(string name, string uuid)
    {
        return BleGatt::GetOrMakeService(name, uuid);
    }
};


