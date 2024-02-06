#pragma once

#include <functional>
#include <initializer_list>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "Evm.h"
#include "Log.h"

using namespace std;


class Pin
{
    friend class Evm;

public:
    
    enum class Type : int
    {
        OUTPUT,
        INPUT,
        INPUT_PULLUP,
        INPUT_PULLDOWN,
    };


public:

    Pin() { }
    
    Pin(uint8_t portNum, uint8_t pin, Type type = Type::OUTPUT, int outputLevel = 0)
    : portNum_(portNum)
    , pin_(pin)
    , type_(type)
    , outputLevel_(outputLevel)
    {
        if (portNum_ == 0 || portNum_ == 1)
        {
            port_ = GetPortDevice(portNum);

            if (port_)
            {
                Configure(portNum_, pin_, type_, outputLevel_);
            }
        }
    }

    Pin(std::initializer_list<int> &&initList, Type type = Type::OUTPUT, int outputLevel = 0)
    {
        if (initList.size() == 2)
        {
            auto ptr = initList.begin();

            portNum_ = (uint8_t)*ptr; ++ptr;
            pin_ = (uint8_t)*ptr; ++ptr;
            type_ = type;
            outputLevel_ = outputLevel;

            if (portNum_ == 0 || portNum_ == 1)
            {
                port_ = GetPortDevice(portNum_);

                if (port_)
                {
                    Configure(portNum_, pin_, type_, outputLevel_);
                }
            }
        }
    }

    void ReInit()
    {
        Configure(portNum_, pin_, type_, outputLevel_);
    }

    ~Pin()
    {
        if (port_)
        {
            DisableInterrupt();

            // Disable?
            // Makes passing by value (and subsequent destruct) break operation
            // gpio_pin_configure(port_, pin_, GPIO_DISCONNECTED);
        }
    }

    static bool Configure(uint8_t portNum, uint8_t pin, Type type, int outputLevel = 0)
    {
        bool retVal = false;

        const device *port = GetPortDevice(portNum);

        if (port)
        {
            switch (type)
            {
                case Type::OUTPUT:
                    retVal = !gpio_pin_configure(port,
                                                pin,
                                                outputLevel ?
                                                    GPIO_OUTPUT_HIGH :
                                                    GPIO_OUTPUT_LOW);
                break;
                
                case Type::INPUT:
                    retVal = !gpio_pin_configure(port, pin, GPIO_INPUT);
                break;

                case Type::INPUT_PULLUP:
                    retVal = !gpio_pin_configure(port, pin, GPIO_INPUT | GPIO_PULL_UP);
                break;

                case Type::INPUT_PULLDOWN:
                    retVal = !gpio_pin_configure(port, pin, GPIO_INPUT | GPIO_PULL_DOWN);
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
        return port_ && !gpio_pin_set(port_, pin_, val == 0 ? 0 : 1);
    }

    bool DigitalToggle() const
    {
        return port_ && !gpio_pin_toggle(port_, pin_);
    }

    bool DigitalRead() const
    {
        return port_ && gpio_pin_get_raw(port_, pin_);
    }


    enum class TriggerType : int
    {
        RISING = GPIO_INT_EDGE_RISING,
        FALLING = GPIO_INT_EDGE_FALLING,
        BOTH = GPIO_INT_EDGE_BOTH,
    };

    void SetInterruptCallback(function<void()> cbFn, TriggerType triggerType)
    {
        cbFn_        = cbFn;
        triggerType_ = triggerType;
    }

    void EnableInterrupt()
    {
        if (port_ && intEnabled_ == false)
        {
            gpio_init_callback(&callbackData_, OnInterrupt, BIT(pin_));
            gpio_add_callback(port_, &callbackData_);
            callbackData_.pinPtr = this;

            gpio_pin_interrupt_configure(port_, pin_, (int)triggerType_);

            intEnabled_ = true;
        }
    }

    void DisableInterrupt();

    uint8_t GetPort()
    {
        return portNum_;
    }

    uint8_t GetPin()
    {
        return pin_;
    }


private:

    static const device *GetPortDevice(uint8_t portNum)
    {
        static const device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

        return gpio0;
    }


private:

    bool intEnabled_ = false;
    function<void()> cbFn_;
    TriggerType triggerType_ = TriggerType::FALLING;


    // zephyr wants a structure for holding callback data, and gives it back to
    // you when interrupt fires.
    // I'm wrapping that structure with my own, and giving me a pointer back to
    // myself instead of using the trash interface they want and having to map
    // pin number back to some pointer I hold somewhere
    struct GpioCallbackWrapped
    : public gpio_callback
    {
        Pin *pinPtr = nullptr;
    };
    GpioCallbackWrapped callbackData_;

    static void OnInterrupt(const device * /*port*/,
                            gpio_callback *gpioCallbackWrapped,
                            gpio_port_pins_t /*pins*/);



private:

    uint8_t       portNum_ = 0;
    uint8_t       pin_     = 0;
    const device *port_    = nullptr;
    Type type_ = Type::OUTPUT;
    int outputLevel_ = 0;

    uint64_t interruptTimeMs = 0;

    static const uint32_t DEBOUNCE_TIME_MS = 250;
};


