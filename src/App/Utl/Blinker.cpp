using namespace std;

#include "Blinker.h"

#include "StrictMode.h"


Blinker::Blinker()
{
    SetName(name_);
}

void Blinker::SetName(string name)
{
    name_ = name;

    ted_.SetCallback([this]{
        OnTimeout();
    }, name_.c_str());
}

void Blinker::SetPin(Pin pin)
{
    pin_ = pin;
}

void Blinker::SetOnOffFunctions(function<void()> fnOn, function<void()> fnOff)
{
    fnOn_ = fnOn;
    fnOff_ = fnOff;
}

void Blinker::SetBlinkOnOffTime(uint64_t onMs, uint64_t offMs)
{
    onMs_ = onMs;
    offMs_ = offMs;
}

void Blinker::EnableAsyncBlink(uint32_t count)
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

void Blinker::DisableAsyncBlink()
{
    Off();
    ted_.DeRegisterForTimedEvent();

    asyncCountRemaining_ = -1;
}

void Blinker::Blink(uint32_t count)
{
    Blink(count, onMs_, offMs_);
}

void Blinker::Blink(uint32_t count, uint64_t onMs, uint64_t offMs)
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

void Blinker::On()
{
    on_ = true;
    fnOn_();
}

void Blinker::Off()
{
    on_ = false;
    fnOff_();
}

void Blinker::Toggle()
{
    if (on_) { Off(); }
    else     { On();  }
}



void Blinker::OnTimeout()
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
