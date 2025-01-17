#pragma once

#include "Evm.h"

#include <cstdint>
#include <functional>
#include <string>


class Blinker
{
public:
    Blinker();

    void SetName(std::string name);
    void SetPin(Pin pin);
    void SetOnOffFunctions(std::function<void()> fnOn, std::function<void()> fnOff);
    void SetBlinkOnOffTime(uint64_t onMs, uint64_t offMs);
    void EnableAsyncBlink(uint32_t count = 0);
    void DisableAsyncBlink();
    void Blink(uint32_t count);
    void Blink(uint32_t count, uint64_t onMs, uint64_t offMs);
    void On();
    void Off();
    void Toggle();

    static void SetupShell();

private:

    void OnTimeout();


private:
    std::string name_ = "TIMER_BLINKER";

    Pin pin_;

    uint64_t onMs_  = 250;
    uint64_t offMs_ = 750;

    std::function<void()> fnOn_  = [this]{ pin_.DigitalWrite(1); };
    std::function<void()> fnOff_ = [this]{ pin_.DigitalWrite(0); };

    int64_t asyncCountRemaining_ = -1;

    bool on_ = false;

    Timer ted_;
};
