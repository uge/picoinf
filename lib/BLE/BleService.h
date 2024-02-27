#pragma once

#include "BleCharacteristic.h"

#include <string>
#include <map>
#include <memory>
using namespace std;


class BleService
{
public:

    BleService(string name, string uuid);
    BleCharacteristic &GetOrMakeCharacteristic(string name,
                                               string uuid,
                                               string properties);
    map<string, BleCharacteristic> &GetCtcList();

    string GetName()
    {
        return state_->name_;
    }

    string GetUuid()
    {
        return state_->uuid_;
    }

private:

    struct State
    {
        string name_;
        string uuid_;
        map<string, BleCharacteristic> ctcMap_;
    };

    shared_ptr<State> state_ = make_shared<State>();
};
