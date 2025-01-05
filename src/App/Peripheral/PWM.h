#pragma once

#include <cstdint>


class PWM
{
    static const uint32_t CLOCK_FREQ     =   125'000'000;
    static const uint8_t  NS_PER_TICK    = 1'000'000'000 / CLOCK_FREQ;      // 8 ns per tick
    static const uint16_t MAX_COUNTER    =        5'001;
    static const uint16_t TOP            = MAX_COUNTER - 1;
    static const uint32_t NS_PER_PERIOD  = NS_PER_TICK * TOP;               // 40,000 ns = 40 us
    static const uint32_t PERIOD_FREQ    = 1'000'000'000 / NS_PER_PERIOD;   // 25,000 (25 kHz)
    static const uint8_t  NS_PER_COUNTER = TOP * NS_PER_TICK / MAX_COUNTER; //       7 ns
    static const uint16_t NS_PER_PCT     = 0.01 * NS_PER_PERIOD;            //   400 ns = 0.4 us


public:

    PWM(uint8_t pin);
    void SetPulseWidthPct(double pct);
    void On();
    void Off();


public:

    static void SetupShell();


private:

    void ApplyIfOn();


private:

    uint8_t pin_;
    uint8_t slice_;
    uint8_t channel_;

    uint16_t counterVal_ = 0;

    bool isOn_ = false;
};

