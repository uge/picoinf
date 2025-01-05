#include "DataStreamDistributor.h"

#include "StrictMode.h"

using namespace std;


DataStreamDistributor::DataStreamDistributor(UART uart)
: uart_(uart)
{
    // Reserve id of 0 for special purposes
    idMaker_.GetNextId();   // 0
}

pair<bool, uint8_t> DataStreamDistributor::AddDataStreamCallback(function<void(const vector<uint8_t> &data)> cbFn)
{
    auto [ok, id] = idMaker_.GetNextId();

    if (ok)
    {
        id__cbFn_[id] = cbFn;
    }

    return { ok, id };
}

bool DataStreamDistributor::SetDataStreamCallback(uint8_t id, function<void(const vector<uint8_t> &data)> cbFn)
{
    bool retVal = false;

    if (id__cbFn_.contains(id) || id == 0)
    {
        id__cbFn_[id] = cbFn;

        retVal = true;
    }

    return retVal;
}

bool DataStreamDistributor::RemoveDataStreamCallback(uint8_t id)
{
    idMaker_.ReturnId(id);

    return id__cbFn_.erase(id);
}

void DataStreamDistributor::Distribute(const vector<uint8_t> &data)
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

uint8_t DataStreamDistributor::GetCallbackCount()
{
    return (uint8_t)id__cbFn_.size();
}
