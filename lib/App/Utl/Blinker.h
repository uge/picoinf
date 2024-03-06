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

    void SetOnOffFunctions(function<void()> fnOn, function<void()> fnOff)
    {
        fnOn_ = fnOn;
        fnOff_ = fnOff;
    }

    void SetBlinkOnOffTime(uint64_t onMs, uint64_t offMs)
    {
        onMs_ = onMs;
        offMs_ = offMs;
    }

    void EnableAsyncBlink(uint32_t count = 0)
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

        if (count == 0)
        {
            asyncCountRemaining_ = -1;
        }
        else
        {
            asyncCountRemaining_ = count + 1;
        }
    }

    void DisableAsyncBlink()
    {
        Off();
        ted_.DeRegisterForTimedEvent();

        asyncCountRemaining_ = -1;
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
        fnOn_();
    }

    void Off()
    {
        on_ = false;
        fnOff_();
    }

    void Toggle()
    {
        if (on_) { Off(); }
        else     { On();  }
    }

private:

    void OnTimeout()
    {
        if (on_ == false)
        {
            bool takeAction = true;

            if (asyncCountRemaining_ != -1)
            {
                --asyncCountRemaining_;

                takeAction = asyncCountRemaining_ != 0;
            }

            if (takeAction)
            {
                On();

                ted_.RegisterForTimedEvent(onMs_);
            }
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

    uint64_t onMs_  = 250;
    uint64_t offMs_ = 750;

    function<void()> fnOn_  = [this]{ pin_.DigitalWrite(1); };
    function<void()> fnOff_ = [this]{ pin_.DigitalWrite(0); };

    int64_t asyncCountRemaining_ = -1;

    bool on_ = false;

    TimedEventHandlerDelegate ted_;
};
