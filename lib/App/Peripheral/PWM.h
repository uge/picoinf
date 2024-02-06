#pragma once

#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>





class PWMChannel
{
    friend class PWMBank;

private:
    PWMChannel(int bank, int channel, const device *pwmBank)
    : channel_(channel)
    , pwmBank_(pwmBank)
    {
        // char deviceName[7];
        // snprintf(deviceName, sizeof(deviceName), "pwm%i_%i", bank, channel);

        // pwmChannel_ = device_get_binding(deviceName);
    }

public:

    // Adjustments to configuration are are both:
    // - cached
    // - applied in real time if currently "on"

    void SetPeriodNs(uint32_t ns)
    {
        periodNs_ = ns;
        
        ApplyIfOn();
    }

    void SetPeriodUs(uint32_t us)
    {
        SetPeriodNs(us * 1000);
    }

    void SetPeriodMs(uint32_t ms)
    {
        SetPeriodUs(ms * 1000);
    }


    void SetPulseWidthNs(uint32_t ns)
    {
        pulseNs_ = ns;

        ApplyIfOn();
    }

    void SetPulseWidthUs(uint32_t us)
    {
        SetPulseWidthNs(us * 1000);
    }

    void SetPulseWidthMs(uint32_t ms)
    {
        SetPulseWidthUs(ms * 1000);
    }

    // eg 50 = 50%
    void SetPulseWidthPct(double pct)
    {
        SetPulseWidthNs(pct / 100.0 * periodNs_);
    }


    void On()
    {
        isOn_ = true;

        ApplyIfOn();
    }

    void Off()
    {
        pwm_set(pwmBank_, channel_, 0, 0, PWM_POLARITY_NORMAL);

        isOn_ = false;
    }


private:
    void ApplyIfOn()
    {
        if (isOn_)
        {
            pwm_set(pwmBank_, channel_, periodNs_, pulseNs_, PWM_POLARITY_NORMAL);
        }
    }


private:
    int channel_ = 0;
    const device *pwmChannel_ = nullptr;

    const device *pwmBank_ = nullptr;

    uint32_t periodNs_ = 0;
    uint32_t pulseNs_  = 0;

    bool isOn_ = false;
};




class PWMBank
{
public:

    PWMBank(int bank)
    : bank_(bank)
    {

        // 4 banks supported
        if (0 <= bank_ && bank_ <= 3)
        {
            char deviceName[6];
            snprintf(deviceName, sizeof(deviceName), "PWM_%i", bank_);
            pwmBank_ = device_get_binding(deviceName);
        }
    }

    ~PWMBank()
    {
        for (auto &pwmChannel : pwmChannelList_)
        {
            if (pwmChannel != nullptr)
            {
                delete pwmChannel;
                pwmChannel = nullptr;
            }
        }
    }

    PWMChannel *GetChannel(int channel)
    {
        PWMChannel *retVal = nullptr;

        // 4 channels supported per bank
        if (0 <= channel && channel <= 3)
        {
            if (pwmChannelList_[channel] == nullptr)
            {
                pwmChannelList_[channel] = new PWMChannel(bank_, channel, pwmBank_);
            }

            retVal = pwmChannelList_[channel];
        }

        return retVal;
    }

private:
    int bank_;

    const device *pwmBank_ = nullptr;

    PWMChannel *pwmChannelList_[3] = { nullptr };

};

