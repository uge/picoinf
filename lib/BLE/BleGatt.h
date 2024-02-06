#pragma once

#include <map>
#include <string>
#include <vector>
#include <utility>

using namespace std;

#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/addr.h>
#include <bluetooth/services/nus.h>

#include "Log.h"
#include "PAL.h"
#include "UUID.h"



/////////////////////////////////////////////////////////////////////
// BleCharacteristic
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

private:
    // persistent storage for zephyr to point to
    struct {
        // for record 1 of 3
        bt_uuid_16 btUuid16;

        bt_gatt_chrc btGattChrcUserData;

        union
        {
            bt_uuid_16  btUuid16UserData;
            bt_uuid_128 btUuid128UserData;
        };

        // for record 2 of 3
        union
        {
            bt_uuid_16  btUuid16UserData_2;
            bt_uuid_128 btUuid128UserData_2;
        };

        // for record 3 of 3
        bt_uuid_16  btUuid16UserData_3;
        _bt_gatt_ccc btGattCCC_3;
    } zstorage;
};


/////////////////////////////////////////////////////////////////////
// BleService
/////////////////////////////////////////////////////////////////////

class BleService
{
    friend class BleGatt;

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
// BleGatt
/////////////////////////////////////////////////////////////////////


// forward decl
class Ble;
class BleConnectionManager;

class BleGatt
{
    friend class Ble;
    friend class BleConnectionManager;

public:
    static BleService &GetOrMakeService(string name, string uuid)
    {
        auto &retVal = svcMap_[name];

        retVal.name_ = name;
        retVal.Configure(uuid);
        retVal.fnGetSmallestBuf_ = [](){ return GetSmallestSendDataLen(); };

        return retVal;
    }

private:
    static void SetSmallestSendDataLen(uint8_t smallestSendDataLen)
    {
        smallestSendDataLen_ = smallestSendDataLen;
    }

    static uint16_t GetSmallestSendDataLen()
    {
        return smallestSendDataLen_;
    }

    static void Register()
    {
        for (auto &[name, svc] : svcMap_)
        {
            if (svc.IsOk())
            {
                RegisterService(svc);

                for (auto &[name, ctc] : svc.ctcMap_)
                {
                    if (ctc.IsOk())
                    {
                        RegisterCharacteristic(ctc);
                    }
                    else
                    {
                        Log("ERR: BleCharacteristic ", svc.name_, ".", ctc.name_, " is not ok, not registering to gatt");
                    }
                }
            }
            else
            {
                Log("ERR: BleService ", svc.name_, " is not ok, not registering to gatt");
            }
        }

        // Log(gattAttrList_.size(), " elements, registering");

        static bt_gatt_service zGattSvc = {
            .attrs = gattAttrList_.data(),
            .attr_count = gattAttrList_.size(),
        };

        if (gattAttrList_.size())
        {
            bt_gatt_service_register(&zGattSvc);
        }
    }
    
    static void RegisterCharacteristic(BleCharacteristic &ctc)
    {
        //
        // Section 1 of possibly 3
        //

        // indicate this is a characteristic description
        ctc.zstorage.btUuid16 = {
            .uuid = { BT_UUID_TYPE_16 },
            .val = 0x2803,
        };
        
        // Set up attribute user data
        bt_uuid *btUuidPtr = nullptr;
        if (ctc.uuid_.GetBitCount() == 16)
        {
            ctc.zstorage.btUuid16UserData = {
                .uuid = { BT_UUID_TYPE_16 },
                .val = ctc.uuid_.GetUint16(),
            };

            btUuidPtr = (bt_uuid *)&ctc.zstorage.btUuid16UserData;
        }
        else if (ctc.uuid_.GetBitCount() == 128)
        {
            ctc.zstorage.btUuid128UserData = {
                .uuid = { BT_UUID_TYPE_128 },
            };

            // required values in little-endian
            ctc.uuid_.ReverseBytes();
            memcpy(ctc.zstorage.btUuid128UserData.val, ctc.uuid_.GetBytes(), 16);
            ctc.uuid_.ReverseBytes();

            btUuidPtr = (bt_uuid *)&ctc.zstorage.btUuid128UserData;
        }

        ctc.zstorage.btGattChrcUserData = {
            .uuid = btUuidPtr,
            .value_handle = 0U,
            .properties = ctc.properties_,
        };

        // assemble together into single record
        bt_gatt_attr attr = {
            .uuid = (bt_uuid *)&ctc.zstorage.btUuid16,
            .read = bt_gatt_attr_read_chrc,
            .write = nullptr,
            .user_data = &ctc.zstorage.btGattChrcUserData,
            .handle = 0,
            .perm = BT_GATT_PERM_READ,
        };

        // store it
        gattAttrList_.push_back(attr);


        //
        // Section 2 of possibly 3
        //

        // Set up attribute user data
        bt_uuid *btUuidPtr_2 = nullptr;
        if (ctc.uuid_.GetBitCount() == 16)
        {
            ctc.zstorage.btUuid16UserData_2 = {
                .uuid = { BT_UUID_TYPE_16 },
                .val = ctc.uuid_.GetUint16(),
            };

            btUuidPtr_2 = (bt_uuid *)&ctc.zstorage.btUuid16UserData_2;
        }
        else if (ctc.uuid_.GetBitCount() == 128)
        {
            ctc.zstorage.btUuid128UserData_2 = {
                .uuid = { BT_UUID_TYPE_128 },
            };

            // required values in little-endian
            ctc.uuid_.ReverseBytes();
            memcpy(ctc.zstorage.btUuid128UserData_2.val, ctc.uuid_.GetBytes(), 16);
            ctc.uuid_.ReverseBytes();

            btUuidPtr_2 = (bt_uuid *)&ctc.zstorage.btUuid128UserData_2;
        }

        // assemble together into single record
        bt_gatt_attr attr_2 = {
            .uuid = btUuidPtr_2,
            .read = OnCtcReadValueRequested,
            .write = OnCtcWriteValueRequested,
            .user_data = (void *)&ctc,
            .handle = 0,
            .perm = ctc.permissions_,
        };

        // store it
        gattAttrList_.push_back(attr_2);

        // update the characteristic to know where this attr value is, since
        // it is later needed on notifications
        ctc.attr_ = &gattAttrList_[gattAttrList_.size() - 1];

        //
        // Section 3 of possibly 3
        //
        if (ctc.enableCCC_)
        {
            ctc.zstorage.btUuid16UserData_3 = {
                .uuid = { BT_UUID_TYPE_16 },
                .val = (BT_UUID_GATT_CCC_VAL),
            };

            ctc.zstorage.btGattCCC_3 = {
                .cfg = {},
                .cfg_changed = OnCtcChangeCCC,
                .cfg_write = nullptr,
                .cfg_match = nullptr,
            };

            bt_gatt_attr attr_3 = {
                .uuid = (bt_uuid *)&ctc.zstorage.btUuid16UserData_3,
                .read = bt_gatt_attr_read_ccc,
                .write = bt_gatt_attr_write_ccc,
                .user_data = (void *)&ctc.zstorage.btGattCCC_3,
                .handle = 0,
                .perm = ctc.permissions_,
            };

            // store it
            gattAttrList_.push_back(attr_3);
        }
    }
    
    static void RegisterService(BleService &svc)
    {
        // indicate that this is a service description
        svc.zstorage.btUuid16 = {
            .uuid = { BT_UUID_TYPE_16 },
            .val = BT_UUID_GATT_PRIMARY_VAL,
        };

        // fill out user-specified service uuid
        bt_uuid *btUuidPtr = nullptr;
        if (svc.uuid_.GetBitCount() == 16)
        {
            svc.zstorage.btUuid16UserData = {
                .uuid = { BT_UUID_TYPE_16 },
                .val = svc.uuid_.GetUint16(),
            };

            btUuidPtr = (bt_uuid *)&svc.zstorage.btUuid16UserData;
        }
        else if (svc.uuid_.GetBitCount() == 128)
        {
            svc.zstorage.btUuid128UserData = {
                .uuid = { BT_UUID_TYPE_128 },
            };

            // required values in little-endian
            svc.uuid_.ReverseBytes();
            memcpy(svc.zstorage.btUuid128UserData.val, svc.uuid_.GetBytes(), 16);
            svc.uuid_.ReverseBytes();

            btUuidPtr = (bt_uuid *)&svc.zstorage.btUuid128UserData;
        }

        // assemble together into single record
        bt_gatt_attr attrSvc = {
            .uuid = (bt_uuid *)&svc.zstorage.btUuid16,
            .read = bt_gatt_attr_read_service,
            .write = nullptr,
            .user_data = btUuidPtr,
            .handle = 0,
            .perm = BT_GATT_PERM_READ,
        };

        // store it
        gattAttrList_.push_back(attrSvc);
    }

private:

    static ssize_t OnCtcReadValueRequested(bt_conn *conn,
                                           const bt_gatt_attr *attr,
                                           void *buf,
                                           uint16_t len,
                                           uint16_t offset)
    {
        BleCharacteristic &ctc = *(BleCharacteristic *)attr->user_data;

        // Log("OnCtcReadValueRequested (", ctc.name_, ")");
        // Log("  conn(", (uint32_t)conn, "), attr(", (uint32_t)attr, "), buf(", (uint32_t)buf, "), len(", len, "), offset(", offset, ")");

        uint8_t *bufRetVal = nullptr;
        uint16_t bufLenRetVal = 0;

        ctc.cbFnRead_(conn, bufRetVal, bufLenRetVal, offset);
        // Log("  bufRetVal(", (uint32_t)bufRetVal, "), bufLenRetVal(", bufLenRetVal, ")");

        return bt_gatt_attr_read(conn, attr, buf, len, offset, bufRetVal, bufLenRetVal);
    }

    static ssize_t OnCtcWriteValueRequested(bt_conn *conn,
                                            const bt_gatt_attr *attr,
                                            const void *buf,
                                            uint16_t len,
                                            uint16_t offset,
                                            uint8_t flags)
    {
        BleCharacteristic &ctc = *(BleCharacteristic *)attr->user_data;

        // Log("OnCtcWriteValueRequested (", ctc.name_, ")");
        // Log("  conn(", (uint32_t)conn,  "), attr(", (uint32_t)attr, "), buf(", (uint32_t)buf, "), len(", len, "), offset(", offset, "), flags(", flags, ")");

        if (buf && len)
        {
            ctc.cbFnWrite_(conn, (uint8_t *)buf, len, offset, flags);
        }

        return len;
    }

    static void OnCtcChangeCCC(const struct bt_gatt_attr *attr, uint16_t value)
    {
        // The pointer to the Ctc is one record back
        BleCharacteristic &ctc = *(BleCharacteristic *)((attr - 1)->user_data);

        // Log("OnCtcChangeCCC (", ctc.name_, ")");
        // Log("  attr(", (uint32_t)attr, "), value(", value, ")");

        ctc.cbFnSubscribed_(BT_GATT_CCC_NOTIFY == value);
    }



private:
    inline static map<string, BleService> svcMap_;

    inline static vector<bt_gatt_attr> gattAttrList_;

    inline static uint8_t smallestSendDataLen_ = 0;
};



