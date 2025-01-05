#include "Evm.h"
#include "Pin.h"
#include "PAL.h"
#include "Shell.h"
#include "Timeline.h"


#include "StrictMode.h"


Pin::PinData Pin::pin__data_[Pin::PIN_COUNT];


Pin::Pin()
{
    // Nothing to do
}

Pin::Pin(uint8_t pin, Type type, uint8_t outputLevel)
: pin_(pin)
{
    if (PinValid(pin_))
    {
        PinData &pd = GetPinData(pin_);
        pd.type_ = type;
        pd.outputLevel_ = outputLevel;

        Configure(pin_, pd.type_, pd.outputLevel_);

        IncrRef(pin_);
    }
}

// copy ctor
Pin::Pin(const Pin& other)
{
    // just copy members, the actual processor gpio state will already be set
    Assign(other);

    IncrRef(pin_);
}

// copy operator
Pin& Pin::operator=(const Pin& other)
{
    // cache current pin for (maybe) destruction
    uint8_t thisPin = pin_;

    // do assignment
    Assign(other);

    // only if different pins do ref adjustment
    if (thisPin != other.pin_)
    {
        // do reference counting and (maybe) destruction
        DecrRef(thisPin);
        IncrRef(other.pin_);
    }

    return *this;
}

void Pin::ReInit()
{
    Configure(pin_, pin__data_[pin_].type_, pin__data_[pin_].outputLevel_);
}

Pin::~Pin()
{
    if (PinValid(pin_))
    {
        DecrRef(pin_);
    }
}

uint8_t Pin::GetPin() const
{
    return pin_;
}

bool Pin::Configure(uint8_t pin, Type type, uint8_t outputLevel)
{
    bool retVal = false;

    if (PinValid(pin))
    {
        retVal = true;

        switch (type)
        {
            case Type::OUTPUT:
                gpio_init(pin);
                gpio_disable_pulls(pin);
                gpio_set_dir(pin, GPIO_OUT);
                gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
                gpio_put(pin, outputLevel ? 1 : 0);
            break;
            
            case Type::INPUT:
                gpio_init(pin);
                gpio_set_dir(pin, GPIO_IN);
                gpio_disable_pulls(pin);
            break;

            case Type::INPUT_PULLUP:
                gpio_init(pin);
                gpio_set_dir(pin, GPIO_IN);
                gpio_pull_up(pin);
            break;

            case Type::INPUT_PULLDOWN:
                gpio_init(pin);
                gpio_set_dir(pin, GPIO_IN);
                gpio_pull_down(pin);
            break;

            default:
                // nothing
            break;
        }
    }

    return retVal;
}

bool Pin::DigitalWrite(int val) const
{
    bool retVal = PinValid(pin_);

    if (retVal)
    {
        gpio_put(pin_, val ? 1 : 0);
    }

    return retVal;
}

bool Pin::GetDigitalWrite() const
{
    bool retVal = PinValid(pin_);

    if (retVal)
    {
        retVal = gpio_get_out_level(pin_);
    }

    return retVal;
}

bool Pin::DigitalToggle() const
{
    bool retVal = PinValid(pin_);

    if (retVal)
    {
        gpio_put(pin_, GetDigitalWrite() ? 0 : 1);
    }

    return retVal;
}

bool Pin::DigitalRead() const
{
    bool retVal = PinValid(pin_);

    if (retVal)
    {
        retVal = gpio_get(pin_);
    }

    return retVal;
}

void Pin::SetInterruptCallback(function<void()> cbFn, TriggerType triggerType)
{
    PinData &pd = GetPinData(pin_);

    pd.cbFn_        = cbFn;
    pd.triggerType_ = triggerType;
}

void Pin::EnableInterrupt()
{
    PinData &pd = GetPinData(pin_);

    if (PinValid(pin_))
    {
        // establish (once) the GPIO interrupt callback handler, for this core
        static bool doneOnce = false;
        if (!doneOnce)
        {
            gpio_set_irq_callback(&InterruptHandler);
            irq_set_enabled(IO_IRQ_BANK0, true);

            doneOnce = true;
        }

        // enable interrupts on this specific pin
        gpio_set_irq_enabled(pin_, (uint32_t)pd.triggerType_, true);

        // keep track that this pin has an interrupt enabled
        pd.intEnabled_ = true;
    }
}

void Pin::DisableInterrupt()
{
    if (PinValid(pin_))
    {
        PinData &pd = GetPinData(pin_);

        // disable interrupts on this specific pin.
        // if the triggerType changed since enable, this won't work
        // as expected.
        gpio_set_irq_enabled(pin_, (uint32_t)pd.triggerType_, false);

        pd.intEnabled_ = false;
    }
}

void Pin::InterruptHandler(uint pin, uint32_t eventMask)
{
    PinData &pd = GetPinData((uint8_t)pin);

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

bool Pin::PinValid(uint8_t pin)
{
    return pin < PIN_COUNT;
}

void Pin::Assign(const Pin& other)
{
    PinData &pdThis  = GetPinData(pin_);
    PinData &pdOther = GetPinData(other.pin_);

    // copy id
    pin_ = other.pin_;

    // copy data
    pdThis = pdOther;
}

void Pin::IncrRef(uint8_t pin)
{
    if (PinValid(pin))
    {
        ++pin__data_[pin].count;
    }
}

void Pin::DecrRef(uint8_t pin)
{
    if (PinValid(pin))
    {
        --pin__data_[pin].count;

        if (pin__data_[pin].count == 0)
        {
            RefCountDestroy(pin);
        }
    }
}

void Pin::RefCountDestroy(uint8_t pin)
{
    gpio_deinit(pin_);
    DisableInterrupt();

    PinData &pd = GetPinData(pin);
    pd = PinData{};
}

Pin::PinData &Pin::GetPinData(uint8_t pin)
{
    static PinData dummy;

    return PinValid(pin) ? pin__data_[pin] : dummy;
}


////////////////////////////////////////////////////////////////////////////////
// Initilization
////////////////////////////////////////////////////////////////////////////////

void Pin::SetupShell()
{
    Timeline::Global().Event("Pin::SetupShell");

    Shell::AddCommand("pin.time.out", [](vector<string> argList){
        uint8_t pin  = (uint8_t)atoi(argList[0].c_str());
        uint32_t us  = (uint32_t)atoi(argList[1].c_str());

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
        uint8_t pin  = (uint8_t)atoi(argList[0].c_str());
        uint8_t val  = (uint8_t)atoi(argList[1].c_str());

        // apply the configuration
        Pin::Configure(pin, Pin::Type::OUTPUT, val);
    }, { .argCount = 2, .help = "Pin output <pin> <1=HIGH/0=LOW>" });

    Shell::AddCommand("pin.in", [](vector<string> argList){
        uint8_t pin  = (uint8_t)atoi(argList[0].c_str());
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

