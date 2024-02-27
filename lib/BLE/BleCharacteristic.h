#pragma once

#include "UUID.h"

#include <functional>
#include <string>
#include <memory>
using namespace std;


class BleService;

class BleCharacteristic
{
public:
    using CbRead          = function<void(vector<uint8_t> &byteList)>;
    using CbWrite         = function<void(vector<uint8_t> &byteList)>;
    using CbSubscribe     = function<void(bool enabled)>;
    using CbTriggerNotify = function<void()>;

public:

    BleCharacteristic(string name, string uuid, string properties)
    {
        state_->name_       = name;
        state_->uuid_       = uuid;
        state_->properties_ = properties;
    }

    BleCharacteristic &SetCallbackOnRead(CbRead cbFnRead)
    {
        state_->cbFnRead_ = cbFnRead;

        return *this;
    }

    CbRead GetCallbackOnRead()
    {
        return state_->cbFnRead_;
    }

    BleCharacteristic &SetCallbackOnWrite(CbWrite cbFnWrite)
    {
        state_->cbFnWrite_ = cbFnWrite;

        return *this;
    }

    CbWrite GetCallbackOnWrite()
    {
        return state_->cbFnWrite_;
    }

    BleCharacteristic &SetCallbackOnSubscribe(CbSubscribe cbFnSubscribe)
    {
        state_->cbFnSubscribed_ = cbFnSubscribe;

        return *this;
    }

    CbSubscribe GetCallbackOnSubscribe()
    {
        return [this](bool enabled){ OnSubscribe(enabled); };
    }

    BleCharacteristic &SetCallbackTriggerNotify(CbTriggerNotify cbFnTriggerNotify)
    {
        state_->cbFnTriggerNotify_ = cbFnTriggerNotify;

        return *this;
    }

    void Notify()
    {
        state_->cbFnTriggerNotify_();
    }

    string GetName()
    {
        return state_->name_;
    }

    string GetUuid()
    {
        return state_->uuid_;
    }

    string GetProperties()
    {
        return state_->properties_;
    }

    bool GetIsSubscribed()
    {
        return state_->subscriptionEnabled_;
    }


private:

    void OnSubscribe(bool enabled)
    {
        state_->subscriptionEnabled_ = enabled;

        state_->cbFnSubscribed_(enabled);
    }


private:

    struct State
    {
        string  name_;
        string  uuid_;
        string  properties_;

        bool subscriptionEnabled_ = false;

        CbRead          cbFnRead_          = [](vector<uint8_t> &byteList){};
        CbWrite         cbFnWrite_         = [](vector<uint8_t> &byteList){};
        CbSubscribe     cbFnSubscribed_    = [](bool enabled){};
        CbTriggerNotify cbFnTriggerNotify_ = []{};
    };

    shared_ptr<State> state_ = make_shared<State>();
};