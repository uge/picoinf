#pragma once

#include <string>
#include <vector>
using namespace std;


class BleAdvertisementSource
{
public:

    BleAdvertisementSource &SetName(string name)
    {
        name_ = name;

        return *this;
    }

    const string &GetName()
    {
        return name_;
    }

    BleAdvertisementSource &SetIsConnectable(bool isConnectable)
    {
        isConnectable_ = isConnectable;

        return *this;
    }

    bool GetIsConnectable()
    {
        return isConnectable_;
    }

    BleAdvertisementSource &SetAdvertisingUuidList(vector<string> advUuidList)
    {
        advUuidList_ = advUuidList;

        return *this;
    }

    const vector<string> &GetAdvertisingUuidList()
    {
        return advUuidList_;
    }

    BleAdvertisementSource &SetAdvertisingMfrData(vector<uint8_t> mfrData)
    {
        mfrData_ = mfrData;

        return *this;
    }

    BleAdvertisementSource &SetAdvertisingMfrData(uint8_t *buf, uint8_t bufLen)
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
    BleAdvertisementSource &SetAdvertisingWebAddress(string webAddress)
    {
        webAddress_ = webAddress;

        return *this;
    }

    const string &GetAdvertisingWebAddress()
    {
        return webAddress_;
    }

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