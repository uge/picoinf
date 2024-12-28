#pragma once

#include "IDMaker.h"
#include "UART.h"

#include <functional>
#include <unordered_map>
#include <utility>
using namespace std;


class DataStreamDistributor
{
public:
    DataStreamDistributor(UART uart)
    : uart_(uart)
    {
        // Reserve id of 0 for special purposes
        idMaker_.GetNextId();   // 0
    }

    pair<bool, uint8_t> AddDataStreamCallback(function<void(const vector<uint8_t> &data)> cbFn)
    {
        auto [ok, id] = idMaker_.GetNextId();

        if (ok)
        {
            id__cbFn_[id] = cbFn;
        }

        return { ok, id };
    }

    bool SetDataStreamCallback(uint8_t id, function<void(const vector<uint8_t> &data)> cbFn)
    {
        bool retVal = false;

        if (id__cbFn_.contains(id) || id == 0)
        {
            id__cbFn_[id] = cbFn;

            retVal = true;
        }

        return retVal;
    }

    bool RemoveDataStreamCallback(uint8_t id)
    {
        idMaker_.ReturnId(id);

        return id__cbFn_.erase(id);
    }

    void Distribute(const vector<uint8_t> &data)
    {
        // distribute
        for (auto &[id, cbFn] : id__cbFn_)
        {
            // default to writing back to the uart that sent the data
            // UartTarget target(uart_);    // ehh, let's keep this functionality very simple

            // fire callback
            cbFn(data);
        }
    }

    uint8_t GetCallbackCount()
    {
        return id__cbFn_.size();
    }

private:

    UART uart_;
    IDMaker<32> idMaker_;
    unordered_map<uint8_t, function<void(const vector<uint8_t> &data)>> id__cbFn_;
};
