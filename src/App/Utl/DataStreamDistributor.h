#pragma once

#include "IDMaker.h"
#include "UART.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>


class DataStreamDistributor
{
public:
    DataStreamDistributor(UART uart);

    std::pair<bool, uint8_t> AddDataStreamCallback(std::function<void(const std::vector<uint8_t> &data)> cbFn);
    bool SetDataStreamCallback(uint8_t id, std::function<void(const std::vector<uint8_t> &data)> cbFn);
    bool RemoveDataStreamCallback(uint8_t id);
    void Distribute(const std::vector<uint8_t> &data);
    uint8_t GetCallbackCount();


private:

    UART uart_;
    IDMaker<32> idMaker_;
    std::unordered_map<uint8_t, std::function<void(const std::vector<uint8_t> &data)>> id__cbFn_;
};
