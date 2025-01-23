#include "Blinker.h"
#include "Shell.h"

using namespace std;

#include "StrictMode.h"


Blinker::Blinker()
{
    SetName("TIMER_BLINKER");

    timer_.SetCallback([this]{
        OnTimeout();
    });
}

void Blinker::SetName(const char *name)
{
    timer_.SetName(name);
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
    timer_.TimeoutInMs(0);

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
    timer_.Cancel();

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

            timer_.TimeoutInMs(onMs_);
        }
    }
    else
    {
        Off();

        timer_.TimeoutInMs(offMs_);
    }
}

void Blinker::SetupShell()
{
    static Blinker blinker;

    blinker.SetName("TIMER_BLINKER_UTL");

    Shell::AddCommand("blinker.set.pin", [](vector<string> argList){
        uint8_t pin = (uint8_t)atoi(argList[0].c_str());

        blinker.SetPin({pin});
    }, { .argCount = 1, .help = "set <pin>"});

    Shell::AddCommand("blinker.set.onoff", [](vector<string> argList){
        uint32_t onMs = (uint32_t)atoi(argList[0].c_str());
        uint32_t offMs = (uint32_t)atoi(argList[1].c_str());

        blinker.SetBlinkOnOffTime(onMs, offMs);
    }, { .argCount = 2, .help = "set <on> <off> ms"});

    Shell::AddCommand("blinker.blink", [](vector<string> argList){
        uint32_t blinkCount = (uint32_t)atoi(argList[0].c_str());

        blinker.Blink(blinkCount);
    }, { .argCount = 1, .help = "blink <x> times"});

    Shell::AddCommand("blinker.on", [](vector<string> argList){
        blinker.On();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("blinker.off", [](vector<string> argList){
        blinker.Off();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("blinker.async.on", [](vector<string> argList){
        blinker.EnableAsyncBlink();
    }, { .argCount = 0, .help = ""});

    Shell::AddCommand("blinker.async.off", [](vector<string> argList){
        blinker.DisableAsyncBlink();
    }, { .argCount = 0, .help = ""});
}