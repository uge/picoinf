#pragma once

#include "IDMaker.h"
#include "UART.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>


class LineStreamDistributor
{
public:
    LineStreamDistributor(UART uart, uint16_t maxLineLen, const char *name);
    std::pair<bool, uint8_t> AddLineStreamCallback(std::function<void(const std::string &line)> cbFn, bool hideBlankLines = true);
    bool SetLineStreamCallback(uint8_t id, std::function<void(const std::string &line)> cbFn, bool hideBlankLines = true);
    bool RemoveLineStreamCallback(uint8_t id);
    void AddData(const std::vector<uint8_t> &data);
    uint8_t GetCallbackCount();
    uint32_t Size() const;
    uint32_t Clear();
    uint32_t ClearInFlight();

private:
    UART uart_;
    uint16_t maxLineLen_;
    const char *name_;
    IDMaker<32> idMaker_;
    struct Data
    {
        std::function<void(const std::string &)> cbFn = [](const std::string &){};
        bool hideBlankLines = true;
    };
    std::unordered_map<uint8_t, Data> id__data_;
    std::string inputStream_;
};