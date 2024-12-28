#pragma once

#include "IDMaker.h"
#include "UART.h"

#include <functional>
#include <unordered_map>
#include <utility>
using namespace std;


class LineStreamDistributor
{
public:
    LineStreamDistributor(UART uart, uint16_t maxLineLen, const char *name)
    : uart_(uart)
    , maxLineLen_(maxLineLen)
    , name_(name)
    {
        // Reserve id of 0 for special purposes
        idMaker_.GetNextId();   // 0
    }

    pair<bool, uint8_t> AddLineStreamCallback(function<void(const string &line)> cbFn, bool hideBlankLines = true)
    {
        auto [ok, id] = idMaker_.GetNextId();

        if (ok)
        {
            id__data_[id] = Data{cbFn, hideBlankLines};
        }

        return { ok, id };
    }

    bool SetLineStreamCallback(uint8_t id, function<void(const string &line)> cbFn, bool hideBlankLines = true)
    {
        bool retVal = false;

        if (id__data_.contains(id) || id == 0)
        {
            id__data_[id] = Data{cbFn, hideBlankLines};

            retVal = true;
        }

        return retVal;
    }

    bool RemoveLineStreamCallback(uint8_t id)
    {
        idMaker_.ReturnId(id);
        
        return id__data_.erase(id);
    }

    void AddData(const vector<uint8_t> &data)
    {
        if (id__data_.empty()) { return; }

        for (int i = 0; i < (int)data.size(); ++i)
        {
            char c = data[i];

            if (c == '\n' || c == '\r' || inputStream_.size() == maxLineLen_)
            {
                // remember if max line len hit
                bool wasMaxLine = inputStream_.size() == maxLineLen_;

                // distribute

                // handle case where the callback leads to the caller
                // de-registering itself and invalidating the id and callback fn
                // itself.
                //
                // this was happening with gps getting a lock, bubbling the lock
                // event, which then says ok no need to listen anymore, which
                // found its way back to deregistering, all while the initial
                // callback was still executing
                auto tmp = id__data_;
                for (auto &[id, data] : tmp)
                {
                    if (inputStream_.size() || data.hideBlankLines == false)
                    {
                        // default to writing back to the uart that sent the data
                        UartTarget target(uart_);

                        // fire callback
                        data.cbFn(inputStream_);
                    }
                }

                // clear line cache
                inputStream_.clear();

                // wind forward to next newline if needed
                if (wasMaxLine)
                {
                    for (; i < (int)data.size(); ++i)
                    {
                        if (data[i] == '\n')
                        {
                            break;
                        }
                    }
                }
            }
            else if ((isprint(c) || c == ' ' || c == '\t'))
            {
                inputStream_.push_back(c);
            }
        }
    }

    uint8_t GetCallbackCount()
    {
        return id__data_.size();
    }

    uint32_t Size() const
    {
        return inputStream_.size();
    }

    uint32_t Clear()
    {
        uint32_t size = inputStream_.size();

        inputStream_.clear();

        return size;
    }

    uint32_t ClearInFlight()
    {
        return Evm::ClearLowPriorityWorkByLabel(name_);
    }

private:
    UART uart_;
    uint16_t maxLineLen_;
    const char *name_;
    IDMaker<32> idMaker_;
    struct Data
    {
        function<void(const string &)> cbFn = [](const string &){};
        bool hideBlankLines = true;
    };
    unordered_map<uint8_t, Data> id__data_;
    string inputStream_;
};