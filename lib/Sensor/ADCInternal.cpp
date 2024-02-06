#include "ADCInternal.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/device.h>

#include "Log.h"
#include "RP2040.h"
#include "Timeline.h"


uint16_t ADCInternal::GetChannelRaw(uint8_t channel)
{
    uint16_t retVal = 0;

    adc_channel_cfg cfg = {
        .gain             = ADC_GAIN_1,
        .reference        = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
        .channel_id       = channel,
        .differential     = 0,
    };

    int retSetup = adc_channel_setup(dev_, &cfg);

    if (retSetup != 0)
    {
        Log("Could not set up channel ", channel, ": ", retSetup);
    }

    adc_sequence_options options = {
        .interval_us = 0,
        .callback = nullptr,
        .extra_samplings = 0,
    };

    uint16_t buf;
    adc_sequence seq = {
        .options = &options,
        .channels = 1U << channel,
        .buffer = &buf,
        .buffer_size = sizeof(buf),
        .resolution = 12U,
        .oversampling = 3,  // average of 2^x samples
        .calibrate = false,
    };

    // VSYS/3 wasn't working until doing this
    RP2040::EnsureAdcChannelIsPinInput(channel);

    int retRead = adc_read(dev_, &seq);

    if (retRead != 0)
    {
        Log("Could not read channel ", channel, ": ", retRead);
    }
    else
    {
        retVal = buf;
    }

    return retVal;
}

uint16_t ADCInternal::RawToMv(uint16_t raw)
{
    // 12-bit, so out of 4096
    // referencing a 3.3v internal voltage
    return raw * 3300 / 4096;
}

// channels 0-4, where 3 == VSYS/3, 4 == temp sensor
uint16_t ADCInternal::GetChannel(uint8_t channel)
{
    return RawToMv(GetChannelRaw(channel));
}

void ADCInternal::Test()
{
    uint16_t val = adc_ref_internal(dev_);
    Log("ADC internal: ", val);

    for (uint8_t i = 0; i < 5; ++i)
    {
        uint16_t raw = GetChannelRaw(i);
        uint16_t mv = RawToMv(raw);

        if (i == 3)
        {
            uint16_t mvTimes3 = mv * 3;

            Log("Channel ", i, ": ", raw, " -> ", mv, " (VSYS = ", mvTimes3, ")");
        }
        else
        {
            Log("Channel ", i, ": ", raw, " -> ", mv);
        }
    }
}


void ADCInternal::SetupShell()
{
    Timeline::Global().Event("ADCInternal::SetupShell");

    Shell::AddCommand("adc.test", [](vector<string> argList) {
        Test();
    }, { .argCount = 0, .help = "" });

}

int ADCInternalSetupShell()
{
    ADCInternal::SetupShell();

    return 1;
}


#include <zephyr/init.h>
SYS_INIT(ADCInternalSetupShell, APPLICATION, 80);
