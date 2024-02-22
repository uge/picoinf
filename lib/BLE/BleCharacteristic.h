#pragma once

#include "UUID.h"

#include <functional>
#include <string>
using namespace std;


class BleService;

class BleCharacteristic
{
public:
    using CbRead      = function<void(vector<uint8_t> &byteList)>;
    using CbWrite     = function<void(vector<uint8_t> &byteList)>;
    using CbSubscribe = function<void()>;

public:

    BleCharacteristic(string name, string uuid, string properties)
    : name_(name)
    , uuid_(uuid)
    , properties_(properties)
    {
        // Nothing to do
    }

    BleCharacteristic &SetCallbackOnRead(CbRead cbFnRead)
    {
        cbFnRead_ = cbFnRead;

        return *this;
    }

    CbRead GetCallbackOnRead()
    {
        return cbFnRead_;
    }

    BleCharacteristic &SetCallbackOnWrite(CbWrite cbFnWrite)
    {
        cbFnWrite_ = cbFnWrite;

        return *this;
    }

    CbWrite GetCallbackOnWrite()
    {
        return cbFnWrite_;
    }

    BleCharacteristic &SetCallbackOnSubscribe(CbSubscribe cbFnSubscribe)
    {
        cbFnSubscribed_ = cbFnSubscribe;

        return *this;
    }

    string GetName()
    {
        return name_;
    }

    string GetUuid()
    {
        return uuid_;
    }

    string GetProperties()
    {
        return properties_;
    }


private:
    string  name_;
    string  uuid_;
    string  properties_;

    CbRead      cbFnRead_       = [](vector<uint8_t> &byteList){};
    CbWrite     cbFnWrite_      = [](vector<uint8_t> &byteList){};
    CbSubscribe cbFnSubscribed_ = []{};
};