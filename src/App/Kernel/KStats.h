#pragma once

#include <cstdint>
#include <string>
#include <vector>


class KStats
{
public:
    struct KTaskStats
    {
        std::string    name;
        uint32_t       number;
        std::string    state;
        uint32_t       currentPriority;
        uint32_t       basePriority;
        uint64_t       totalRunDuration;
        uint32_t      *stackBase;
        uint32_t      *stackTop;
        uint32_t      *stackEnd;
        uint32_t       highWaterMark;

        void Print() const;
    };

public:
    static void Init();
    static void SetupShell();
    static std::vector<KTaskStats> GetTaskStats();
};