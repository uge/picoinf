#pragma once

#include <zephyr/device.h>


class ADCInternal
{
    inline static const device *dev_ = DEVICE_DT_GET(DT_NODELABEL(adc));

public:

    static uint16_t GetChannelRaw(uint8_t channel);

    static uint16_t RawToMv(uint16_t raw);

    // channels 0-4, where 3 == VSYS/3, 4 == temp sensor
    static uint16_t GetChannel(uint8_t channel);

    static void Test();


    static void SetupShell();
};