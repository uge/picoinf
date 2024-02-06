#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "Log.h"

// https://docs.zephyrproject.org/latest/hardware/peripherals/sensor.html
// drivers/sensor/sensor/shell.c

class ZephyrSensor
{
public:
    ZephyrSensor(const char *deviceName)
    {
        dev_ = device_get_binding(deviceName);

        if (dev_ == nullptr)
        {
            Log("ZS could not get device ", deviceName);
        }
    }

    void Fetch()
    {
        if (dev_)
        {
            int err = sensor_sample_fetch(dev_);

            if (err != 0)
            {
                Log("ZS could not fetch sample: ", err);
            }
        }
    }

    double FetchAndGet(sensor_channel channel)
    {
        Fetch();

        return Get(channel);
    }

    double Get(sensor_channel channel)
    {
        double retVal = 0;

        if (dev_)
        {
            int ret = sensor_channel_get(dev_, channel, &sv_);

            if (ret == 0)
            {
                retVal = (double)sv_.val1 + ((double)sv_.val2 / 1'000'000);
            }
            else
            {
                Log("ZS could not get sensor channel ", channel, ": ", ret);
            }
        }

        return retVal;
    }

private:

    const device *dev_ = nullptr;
    sensor_value sv_;
};
