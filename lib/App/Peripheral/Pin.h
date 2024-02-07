#pragma once

#include <stdint.h>

#include <cstdio>
#include <functional>
using namespace std;

#include "hardware/gpio.h"

// #include "Evm.h"


class Pin
{
    // friend class Evm;

public:
    
    enum class Type : uint8_t
    {
        OUTPUT,
        INPUT,
        INPUT_PULLUP,
        INPUT_PULLDOWN,
    };


public:

    Pin(uint8_t pin, Type type = Type::OUTPUT, uint8_t outputLevel = 0)
    : pin_(pin)
    {
        // printf("Pin::Pin(%i - instan): %li\n", pin_, this);

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
    Pin(const Pin& other)
    {
        // just copy members, the actual processor gpio state will already be set
        Assign(other);

        // printf("Pin::Pin(%i - copy): %li\n", pin_, this);

        IncrRef(pin_);
    }

    // copy operator
    Pin& operator=(const Pin& other)
    {
        // printf("Pin::operator=(%i from %i): %li\n", pin_, other.pin_, this);

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

    ~Pin()
    {
        if (PinValid(pin_))
        {
            // printf("Pin::~Pin(%i): %li\n", pin_, this);
            DecrRef(pin_);
        }
    }

    static bool Configure(uint8_t pin, Type type, uint8_t outputLevel = 0)
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

    bool DigitalWrite(int val) const
    {
        bool retVal = PinValid(pin_);

        if (retVal)
        {
            gpio_put(pin_, val ? 1 : 0);
        }

        return retVal;
    }

    inline bool GetDigitalWrite() const
    {
        bool retVal = PinValid(pin_);

        if (retVal)
        {
            retVal = gpio_get_out_level(pin_);
        }

        return retVal;
    }

    bool DigitalToggle() const
    {
        bool retVal = PinValid(pin_);

        if (retVal)
        {
            gpio_put(pin_, GetDigitalWrite() ? 0 : 1);
        }

        return retVal;
    }

    bool DigitalRead() const
    {
        bool retVal = PinValid(pin_);

        if (retVal)
        {
            retVal = gpio_get(pin_);
        }

        return retVal;
    }


    // enum class TriggerType : int
    // {
    //     RISING = GPIO_INT_EDGE_RISING,
    //     FALLING = GPIO_INT_EDGE_FALLING,
    //     BOTH = GPIO_INT_EDGE_BOTH,
    // };

    // void SetInterruptCallback(function<void()> cbFn, TriggerType triggerType)
    // {
    //     cbFn_        = cbFn;
    //     triggerType_ = triggerType;
    // }

    // void EnableInterrupt()
    // {
    //     if (PinValid(pin_) && intEnabled_ == false)
    //     {
    //         gpio_init_callback(&callbackData_, OnInterrupt, BIT(pin_));
    //         gpio_add_callback(port_, &callbackData_);
    //         callbackData_.pinPtr = this;

    //         gpio_pin_interrupt_configure(port_, pin_, (int)triggerType_);

    //         intEnabled_ = true;
    //     }
    // }

    // void DisableInterrupt();


private:

    

    inline static bool PinValid(uint8_t pin)
    {
        return pin < PIN_COUNT;
    }

    void Assign(const Pin& other)
    {
        PinData &pdThis  = GetPinData(pin_);
        PinData &pdOther = GetPinData(other.pin_);

        // copy id
        pin_ = other.pin_;

        // copy data
        pdThis = pdOther;
    }

    
    void IncrRef(uint8_t pin)
    {
        if (PinValid(pin))
        {
            ++pin__data_[pin].count;
            // printf("Pin(%i) Ref++ = %i\n", pin, pin__data_[pin].count);
        }
    }

    void DecrRef(uint8_t pin)
    {
        if (PinValid(pin))
        {
            --pin__data_[pin].count;
            // printf("Pin(%i) Ref-- = %i\n", pin, pin__data_[pin].count);

            if (pin__data_[pin].count == 0)
            {
                RefCountDestroy(pin);
            }
        }
    }

    void RefCountDestroy(uint8_t pin)
    {
        // printf("Pin::RefCountDestroy(%i)\n", pin);

        gpio_deinit(pin_);
        // DisableInterrupt();

        // Disable?
        // Makes passing by value (and subsequent destruct) break operation
        // gpio_pin_configure(port_, pin_, GPIO_DISCONNECTED);
    }


private:

    // bool intEnabled_ = false;
    // function<void()> cbFn_ = []{};
    // TriggerType triggerType_ = TriggerType::FALLING;

    // static void OnInterrupt(const device * /*port*/,
    //                         gpio_callback *gpioCallbackWrapped,
    //                         gpio_port_pins_t /*pins*/);



private:

    static const uint8_t PIN_COUNT = 30;

    struct PinData
    {
        // reference count
        uint8_t count = 0;

        // actual members
        Type     type_            = Type::OUTPUT;
        uint8_t  outputLevel_     = 0;
        uint64_t interruptTimeMs_ = 0;
    };

    static PinData pin__data_[PIN_COUNT];

    static PinData &GetPinData(uint8_t pin)
    {
        static PinData dummy;

        return PinValid(pin) ? pin__data_[pin] : dummy;
    }

    static const uint32_t DEBOUNCE_TIME_MS = 250;

    // the only data member, really just an id of data stored elsewhere
    uint8_t pin_ = PIN_COUNT;
};


