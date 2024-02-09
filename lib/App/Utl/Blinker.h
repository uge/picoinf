#pragma once

#include "Evm.h"


class Blinker
{
public:
    Blinker()
    {
        ted_.SetCallback([this]{
            OnTimeout();
        }, name_.c_str());
    }

    void SetName(string name)
    {
        name_ = name;
    }

    void SetPin(Pin pin)
    {
        pin_ = pin;
    }

    void SetBlinkOnOffTime(uint64_t onMs, uint64_t offMs)
    {
        onMs_ = onMs;
        offMs_ = offMs;
    }

    void EnableAsyncBlink()
    {
        // could be running on timer or not
        // could be on or off at the moment

        // when you run this function, it should:
        // - go to initial off state
        //   - pin off
        //   - timer not running
        // - start

        Off();
        ted_.RegisterForTimedEvent(0);
    }

    void DisableAsyncBlink()
    {
        Off();
        ted_.DeRegisterForTimedEvent();
    }

    void Blink(uint32_t count)
    {
        Blink(count, onMs_, offMs_);
    }

    void Blink(uint32_t count, uint64_t onMs, uint64_t offMs)
    {
        uint32_t remaining = count;

        Off();

        while (remaining)
        {
            --remaining;

            On();
            PAL.Delay(onMs);
            Off();
            PAL.Delay(offMs);
        }
    }

    void On()
    {
        on_ = true;
        pin_.DigitalWrite(1);
    }

    void Off()
    {
        on_ = false;
        pin_.DigitalWrite(0);
    }

    void Toggle()
    {
        on_ = !on_;
        pin_.DigitalToggle();
    }

private:

    void OnTimeout()
    {
        if (on_ == false)
        {
            On();

            ted_.RegisterForTimedEvent(onMs_);
        }
        else
        {
            Off();

            ted_.RegisterForTimedEvent(offMs_);
        }
    }

private:
    string name_ = "TIMER_BLINKER";

    Pin pin_;

    uint64_t onMs_  = 100;
    uint64_t offMs_ = 100;

    bool on_ = false;

    TimedEventHandlerDelegate ted_;
};
