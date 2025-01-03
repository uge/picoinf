#pragma once

#include "Log.h"
#include "Utl.h"

#include <vector>


class KStats
{
public:
    struct KTaskStats
    {
        string    name;
        uint32_t  number;
        string    state;
        uint32_t  currentPriority;
        uint32_t  basePriority;
        uint64_t  totalRunDuration;
        uint32_t *stackBase;
        uint32_t *stackTop;
        uint32_t *stackEnd;
        uint32_t  highWaterMark;

        void Print() const
        {
            Log("Task");
            Log("-----------------------------------------");
            Log("name            : ", name);
            Log("number          : ", number);
            Log("state           : ", state);
            Log("currentPriority : ", currentPriority);
            Log("basePriority    : ", basePriority);
            Log("totalRunDuration: ", Commas(totalRunDuration));
            Log("stackEnd        : ", ToHex((uint32_t)stackEnd));
            Log("stackTop        : ", ToHex((uint32_t)stackTop));
            Log("stackBase       : ", ToHex((uint32_t)stackBase));
            Log("stackSize       : ", Commas((uint32_t)stackEnd - (uint32_t)stackBase));
            Log("  stackUsed     : ", Commas((uint32_t)stackEnd - (uint32_t)stackTop));
            Log("  stackRem      : ", Commas((uint32_t)stackTop - (uint32_t)stackBase));
            Log("  stackUsedMax  : ", Commas((uint32_t)stackEnd - (uint32_t)stackBase - (uint32_t)(highWaterMark)));
            Log("  stackRemMin   : ", Commas((uint32_t)(highWaterMark)));
        }
    };

public:
    static void Init();
    static void SetupShell();
    static std::vector<KTaskStats> GetTaskStats();
};