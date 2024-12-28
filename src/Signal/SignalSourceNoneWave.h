#pragma once

class SignalSourceNoneWave
{
    static const int8_t DEFAULT_VALUE = 0;
public:

    static inline int8_t GetSample(const uint8_t)
    {
        return DEFAULT_VALUE;
    }

};

