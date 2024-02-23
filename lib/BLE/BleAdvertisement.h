#pragma once

#include <string>
#include <vector>
using namespace std;


class BleAdvertisement
{
public:

    BleAdvertisement &SetName(string name)
    {
        name_ = name;

        return *this;
    }

    const string &GetName()
    {
        return name_;
    }

    BleAdvertisement &SetIsConnectable(bool isConnectable)
    {
        isConnectable_ = isConnectable;

        return *this;
    }

    bool GetIsConnectable()
    {
        return isConnectable_;
    }

    BleAdvertisement &SetAdvertisingUuidList(vector<string> advUuidList)
    {
        advUuidList_ = advUuidList;

        return *this;
    }

    const vector<string> &GetAdvertisingUuidList()
    {
        return advUuidList_;
    }

    BleAdvertisement &SetAdvertisingMfrData(vector<uint8_t> mfrData)
    {
        mfrData_ = mfrData;

        return *this;
    }

    BleAdvertisement &SetAdvertisingMfrData(uint8_t *buf, uint8_t bufLen)
    {
        vector<uint8_t> mfrData;

        if (buf && bufLen)
        {
            for (int i = 0; i < bufLen; ++i)
            {
                mfrData.push_back(buf[i]);
            }
        }

        return SetAdvertisingMfrData(mfrData);
    }

    const vector<uint8_t> &GetAdvertisingMfrData()
    {
        return mfrData_;
    }

    // not the http(s):// prefix, just the stuff after
    BleAdvertisement &SetAdvertisingWebAddress(string webAddress)
    {
        webAddress_ = webAddress;

        return *this;
    }

    const string &GetAdvertisingWebAddress()
    {
        return webAddress_;
    }

    const vector<uint8_t> &GetRawAdvertisingDataStructure(string type);

    // runtime toggle
    void StartAdvertising()
    {
        if (!advStarted_)
        {
            advStarted_ = true;
        }
    }

    void StopAdvertising()
    {
        if (advStarted_)
        {
            advStarted_ = false;
        }
    }

    bool WantsAdvertisingToStart()
    {
        return advStarted_;
    }

    
protected:
    vector<string> advUuidList_;

private:
    string name_;
    bool isConnectable_ = false;
    vector<uint8_t> mfrData_;
    string webAddress_;

    // default behavior is advertising starts on startup unless overridden
    // subsequently it's a runtime decision
    bool advStarted_ = true;
};





