



#include "Pin.h"

Pin::PinData Pin::pin__data_[Pin::PIN_COUNT];




#if 0
#include "Evm.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"


void Pin::DisableInterrupt()
{
    if (port_ && intEnabled_)
    {
        gpio_pin_interrupt_configure(port_, pin_, GPIO_INT_DISABLE);

        gpio_remove_callback(port_, &callbackData_);

        intEnabled_ = false;
    }
}

void Pin::OnInterrupt(const device * /*port*/,
                    gpio_callback *gpioCallbackWrapped,
                    gpio_port_pins_t /*pins*/)
{
    // Convert their pointer into my wrapped type
    GpioCallbackWrapped *callbackData = (GpioCallbackWrapped *)gpioCallbackWrapped;

    // Pull out the pin pointer
    Pin *pin = callbackData->pinPtr;

    uint64_t timeNow = PAL.Millis();
    if (timeNow - pin->interruptTimeMs_ > DEBOUNCE_TIME_MS)
    {
        // Pass off to evm
        Evm::QueueWork("PIN_EVM_QUEUE", [=](){
            if (pin->intEnabled_)
            {
                pin->cbFn_();
            }
        });

        pin->interruptTimeMs_ = timeNow;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

int PinSetupShell()
{
    Timeline::Global().Event("PinSetupShell");

    Shell::AddCommand("pin.time.out", [](vector<string> argList){
        uint8_t port = atoi(argList[0].c_str());
        uint8_t pin  = atoi(argList[1].c_str());
        uint32_t us  = atoi(argList[2].c_str());

        // make sure in a known starting state
        Pin::Configure(port, pin, Pin::Type::OUTPUT, 0);

        // put high
        Pin::Configure(port, pin, Pin::Type::OUTPUT, 1);

        // wait
        PAL.DelayBusyUs(us);

        // put low again
        Pin::Configure(port, pin, Pin::Type::OUTPUT, 0);
    }, { .argCount = 3, .help = "Pin <port> <pin> output HIGH for <durationUs>" });

    Shell::AddCommand("pin.out", [](vector<string> argList){
        uint8_t port = atoi(argList[0].c_str());
        uint8_t pin  = atoi(argList[1].c_str());
        uint8_t val  = atoi(argList[2].c_str());

        // apply the configuration
        Pin::Configure(port, pin, Pin::Type::OUTPUT, val);
    }, { .argCount = 3, .help = "Pin output <port> <pin> <1=HIGH/0=LOW>" });

    Shell::AddCommand("pin.in", [](vector<string> argList){
        uint8_t port = atoi(argList[0].c_str());
        uint8_t pin  = atoi(argList[1].c_str());
        string &type = argList[2];

        Pin::Type t;

        if      (type == "-")  { t = Pin::Type::INPUT; }
        else if (type == "up") { t = Pin::Type::INPUT_PULLUP; }
        else                   { t = Pin::Type::INPUT_PULLDOWN; }

        // apply the configuration
        Pin p(port, pin, t);

        bool level = p.DigitalRead();

        Log("Level: ", level ? "HIGH" : "LOW");
    }, { .argCount = 3, .help = "Pin input with pull <port> <pin> <-/up/down>" });

    return 1;
}


#endif