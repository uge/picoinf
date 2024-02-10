#include "Pin.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"


Pin::PinData Pin::pin__data_[Pin::PIN_COUNT];


void Pin::InterruptHandler(uint pin, uint32_t eventMask)
{
    PinData &pd = GetPinData(pin);

    uint64_t timeNow = PAL.Millis();
    if (timeNow - pd.interruptTimeMs_ > DEBOUNCE_TIME_MS)
    {
        // Pass off to evm
        Evm::QueueWork("PIN_EVM_QUEUE", [&]{
            if (pd.intEnabled_)
            {
                pd.cbFn_();
            }
        });

        pd.interruptTimeMs_ = timeNow;
    }
}



////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void PinSetupShell()
{
    Timeline::Global().Event("PinSetupShell");

    Shell::AddCommand("pin.time.out", [](vector<string> argList){
        uint8_t pin  = atoi(argList[0].c_str());
        uint32_t us  = atoi(argList[1].c_str());

        // make sure in a known starting state
        Pin::Configure(pin, Pin::Type::OUTPUT, 0);

        // put high
        Pin::Configure(pin, Pin::Type::OUTPUT, 1);

        // wait
        PAL.DelayBusyUs(us);

        // put low again
        Pin::Configure(pin, Pin::Type::OUTPUT, 0);
    }, { .argCount = 2, .help = "Pin <pin> output HIGH for <durationUs>" });

    Shell::AddCommand("pin.out", [](vector<string> argList){
        uint8_t pin  = atoi(argList[0].c_str());
        uint8_t val  = atoi(argList[1].c_str());

        // apply the configuration
        Pin::Configure(pin, Pin::Type::OUTPUT, val);
    }, { .argCount = 2, .help = "Pin output <pin> <1=HIGH/0=LOW>" });

    Shell::AddCommand("pin.in", [](vector<string> argList){
        uint8_t pin  = atoi(argList[0].c_str());
        string &type = argList[2];

        Pin::Type t;

        if      (type == "-")  { t = Pin::Type::INPUT; }
        else if (type == "up") { t = Pin::Type::INPUT_PULLUP; }
        else                   { t = Pin::Type::INPUT_PULLDOWN; }

        // apply the configuration
        Pin p(pin, t);

        bool level = p.DigitalRead();

        Log("Level: ", level ? "HIGH" : "LOW");
    }, { .argCount = 2, .help = "Pin input with pull <pin> <-/up/down>" });
}

