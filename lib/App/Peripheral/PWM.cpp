#include "PWM.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#include "StrictMode.h"


PWM::PWM(uint8_t pin)
: pin_(pin)
, slice_((uint8_t)pwm_gpio_to_slice_num(pin_))
, channel_((uint8_t)pwm_gpio_to_channel(pin_))
{
    gpio_set_function(pin_, GPIO_FUNC_PWM);
    pwm_set_wrap(slice_, TOP);
}

void PWM::SetPulseWidthPct(uint8_t pct)
{
    pct = Clamp(0, pct, 100);

    if (pct == 0)
    {
        counterVal_ = 0;
    }
    else if (pct == 100)
    {
        counterVal_ = TOP + 1;
    }
    else
    {
        counterVal_ = (uint16_t)round((double)pct * MAX_COUNTER / 100.0);
    }

    ApplyIfOn();
}

void PWM::On()
{
    isOn_ = true;

    ApplyIfOn();
}

void PWM::Off()
{
    pwm_set_chan_level(slice_, channel_, 0);

    isOn_ = false;
}

void PWM::ApplyIfOn()
{
    if (isOn_)
    {
        pwm_set_chan_level(slice_, channel_, counterVal_);
        pwm_set_enabled(slice_, true);
    }
}

void PWM::SetupShell()
{
    Timeline::Global().Event("PWM::SetupShell");

    Shell::AddCommand("pwm.pin", [](vector<string> argList){
        uint8_t pin = (uint8_t)atoi(argList[0].c_str());
        uint8_t pct = (uint8_t)atoi(argList[1].c_str());

        PWM pwm(pin);
        pwm.SetPulseWidthPct(pct);
        pwm.On();
    }, { .argCount = 2, .help = "pwm pin <x> to <y> pct" });

    Shell::AddCommand("pwm.info", [](vector<string> argList){
        Log("CLOCK_FREQ    : ", Commas(CLOCK_FREQ));
        Log("NS_PER_TICK   : ", Commas(NS_PER_TICK));
        Log("MAX_COUNTER   : ", Commas(MAX_COUNTER));
        Log("TOP           : ", Commas(TOP));
        Log("NS_PER_PERIOD : ", Commas(NS_PER_PERIOD));
        Log("NS_PER_COUNTER: ", Commas(NS_PER_COUNTER));
        Log("NS_PER_PCT    : ", Commas(NS_PER_PCT));
    }, { .argCount = 0, .help = "" });
}
