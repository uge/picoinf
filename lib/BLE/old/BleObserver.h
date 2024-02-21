#pragma once

#include <functional>
#include <unordered_map>
using namespace std;

#include <bluetooth/bluetooth.h>

#include "Log.h"
#include "Ble.h"
#include "BleScanner.h"





/*

    // from gap.h
    // this is the type field

	// Scannable and connectable advertising.
	BT_GAP_ADV_TYPE_ADV_IND               = 0x00,
	// Directed connectable advertising.
	BT_GAP_ADV_TYPE_ADV_DIRECT_IND        = 0x01,
	// Non-connectable and scannable advertising.
	BT_GAP_ADV_TYPE_ADV_SCAN_IND          = 0x02,
	// Non-connectable and non-scannable advertising.
	BT_GAP_ADV_TYPE_ADV_NONCONN_IND       = 0x03,
	// Additional advertising data requested by an active scanner.
	BT_GAP_ADV_TYPE_SCAN_RSP              = 0x04,
	// Extended advertising, see advertising properties.
	BT_GAP_ADV_TYPE_EXT_ADV               = 0x05,







*/










/////////////////////////////////////////////////////////////////////
// AdElement and AdBody
/////////////////////////////////////////////////////////////////////

struct AdElement
{
    uint8_t type;
    vector<uint8_t> byteList;
};

class AdBody
{
public:

    // first byte is len
    // next len bytes:
    // - type (1 byte)
    // - data (len - 1 bytes)
    void ParseTraditional(const uint8_t *buf, uint8_t bufLen)
    {
        if (buf && bufLen)
        {
            adElementList_.clear();

            const uint8_t *p = buf;
            int bytesRemaining = bufLen;

            bool cont = true;
            while (cont && 1 <= bytesRemaining)
            {
                AdElement e;

                int len = p[0];
                if (len)
                {
                    if (1 + len <= bytesRemaining)
                    {
                        e.type = p[1];

                        for (int i = 0; i < len - 1; ++i)
                        {
                            e.byteList.push_back(p[2 + i]);
                        }

                        adElementList_.push_back(e);
                    }
                    else
                    {
                        cont = false;
                        Log("maybe a coding error, implies stated len exceeded remaining buffer");
                    }
                }
                else
                {
                    Log("zero-byte element - valid?");
                }

                bytesRemaining -= 1 + len;
                p += 1 + len;
            }
        }
        else
        {
            Log("empty body - valid?");
        }
    }

    const vector<AdElement> &GetAdElementList() const
    {
        return adElementList_;
    }

private:
    vector<AdElement> adElementList_;
};


/////////////////////////////////////////////////////////////////////
// Scanned Device
/////////////////////////////////////////////////////////////////////

class BleScannedDevice
{
public:

    BleScannedDevice() = default;
    BleScannedDevice(const bt_addr_le_t *addr,
                     int8_t              rssi,
                     uint8_t             type,
                     net_buf_simple     *ad)
    {
        Set((bt_addr_le_t *)addr, rssi, type, ad);
    }

    void Set(bt_addr_le_t  *addr,
             int8_t          rssi,
             uint8_t         type,
             net_buf_simple *ad)
    {
        addr_ = addr;
        rssi_ = rssi;
        type_ = type;
        ad_   = ad;

        Parse(ad_->data, ad_->len);
    }

    // Raw details
    const bt_addr_le_t   *GetRawAddr() const { return addr_; }
    int8_t                GetRawRssi() const { return rssi_; }
    uint8_t               GetRawType() const { return type_; }
    const net_buf_simple *GetRawAd()   const { return ad_;   }

    // Observed properties
    const string &GetAddr() const { return addrStr_;       }
    bool GetIsConnectable() const { return isConnectable_; }
    bool GetIsExtended()    const { return isExtended_;    }

    // Parsed Details
    const AdBody          &GetAdBody()     const { return adBody_;     }
    uint8_t                GetFlags()      const { return flags_;      }
    const string          &GetName()       const { return name_;       }
    const vector<string>  &GetUuidList()   const { return uuidList_;   }
    const vector<uint8_t> &GetMfrData()    const { return mfrData_;    }
    const string          &GetWebAddress() const { return webAddress_; }


    // Just show me
    void Dump() const
    {
        Log("Name (", GetName(), ") at Addr(", GetAddr(), ")");
        Log("RSSI: ", GetRawRssi());
        LogNNL("Connectable(", GetIsConnectable() ? "true" : "false", "), ");
        LogNNL("Extended(", GetIsExtended() ? "true" : "false", ")");
        LogNL();
        LogNNL("Flags: ");
        LogHex(&flags_, 1);
        const vector<string> &uuidList = GetUuidList();
        LogNNL("UUID List (", uuidList.size(), "):");
        for (auto &uuid : uuidList)
        {
            LogNNL(" ", uuid);
        }
        LogNL();
        const vector<uint8_t> &mfrData = GetMfrData();
        LogNNL("MfrData: (", mfrData.size(), "): ");
        LogHexNNL(mfrData.data(), mfrData.size());
        LogNL();
        Log("WebAddress: ", GetWebAddress());

        LogNNL("Raw: (", GetRawAd()->len, "): ");
        LogHex(GetRawAd()->data, GetRawAd()->len);
        for (auto &adEl : GetAdBody().GetAdElementList())
        {
            LogNNL("  ");
            LogHexNNL(&adEl.type, 1);
            LogNNL(": ");
            LogHexNNL(adEl.byteList.data(), adEl.byteList.size());
            LogNL();
        }
    }

private:

    void Parse(const uint8_t *buf, uint8_t bufLen)
    {
        // convert address to human-readable
        addrStr_ = Ble::AddrToStr(addr_);

        // determine type properties
        isConnectable_ = (type_ == BT_GAP_ADV_TYPE_ADV_IND) ||
                         (type_ == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) ||
                         (type_ == BT_GAP_ADV_TYPE_EXT_ADV);

        isExtended_ = (type_ == BT_GAP_ADV_TYPE_EXT_ADV);

        // Parse the body structurally
        if (!isExtended_)
        {
            adBody_.ParseTraditional(buf, bufLen);
        }
        else
        {
            // not supported yet
        }

        // Walk it looking for familiar tags
        for (auto &adEl : adBody_.GetAdElementList())
        {
            if (adEl.type == BT_DATA_FLAGS)
            {
                flags_ = adEl.byteList[0];
            }
            else if (adEl.type == BT_DATA_NAME_COMPLETE)
            {
                for (char c : adEl.byteList)
                {
                    name_ += c;
                }
            }
            else if (adEl.type == BT_DATA_UUID16_ALL || adEl.type == BT_DATA_UUID16_SOME)
            {
                uint8_t bytesRemaining = adEl.byteList.size();

                const uint8_t *p = adEl.byteList.data();
                while (bytesRemaining >= 2)
                {
                    UUID uuid;
                    uuid.SetBytesReversed(p, 2);
                    uuidList_.push_back(uuid.GetUUID());

                    bytesRemaining -= 2;
                    p += 2;
                }
            }
            else if (adEl.type == BT_DATA_UUID128_ALL || adEl.type == BT_DATA_UUID128_SOME)
            {
                uint8_t bytesRemaining = adEl.byteList.size();

                const uint8_t *p = adEl.byteList.data();
                while (bytesRemaining >= 16)
                {
                    UUID uuid;
                    uuid.SetBytesReversed(p, 16);
                    uuidList_.push_back(uuid.GetUUID());

                    bytesRemaining -= 16;
                    p += 16;
                }
            }
            else if (adEl.type == BT_DATA_MANUFACTURER_DATA)
            {
                mfrData_ = adEl.byteList;
            }
            else if (adEl.type == BT_DATA_URI)
            {
                if (adEl.byteList.size())
                {
                    for (char c : adEl.byteList)
                    {
                        if (c == 0x17)
                        {
                            webAddress_ += "https:";
                        }
                        else if (isprint(c))
                        {
                            webAddress_ += c;
                        }
                    }
                }
            }
        }
    }


private:
    // raw details
    bt_addr_le_t   *addr_ = nullptr;
    int8_t          rssi_ = 0;
    uint8_t         type_ = 0;
    net_buf_simple *ad_   = nullptr;

    // observed properties
    string addrStr_;
    bool isConnectable_ = false;
    bool isExtended_ = false;

    // parsed details (assumes no more than one found apiece)
    AdBody adBody_;
    uint8_t flags_;
    string name_;
    vector<string> uuidList_;
    vector<uint8_t> mfrData_;
    string webAddress_;
};





/////////////////////////////////////////////////////////////////////
// Observer
/////////////////////////////////////////////////////////////////////

class BleObserver
{
    friend class Ble;
    friend class BleScanner;

private:
    // just force it to be private to can't be instantiated outside Ble
    BleObserver() = default;


public:
    void SetCbOnDeviceFound(function<void(const BleScannedDevice &bsd)> cbFnOnDeviceFound)
    {
        cbFnOnDeviceFound_ = cbFnOnDeviceFound;
    }

    void StartScanning(uint8_t pctTime = 100)
    {
        BleScanner::StartScanning(this, pctTime);
    }

    void StopScanning()
    {
        BleScanner::StopScanning();
    }


private:

    void OnDeviceFound(const bt_addr_le_t *addr,
                       int8_t              rssi,
                       uint8_t             type,
                       net_buf_simple     *ad)
    {
        BleScannedDevice bsd(addr, rssi, type, ad);

        cbFnOnDeviceFound_(bsd);
    }



private:
    function<void(const BleScannedDevice &)> cbFnOnDeviceFound_ = [](const BleScannedDevice &){};
};

