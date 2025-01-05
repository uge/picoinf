#pragma once

#include <cstdint>
#include <functional>

#include "hardware/gpio.h"


class Pin
{
    friend class Evm;

public:
    
    enum class Type : uint8_t
    {
        OUTPUT,
        INPUT,
        INPUT_PULLUP,
        INPUT_PULLDOWN,
    };


public:

    Pin();
    Pin(uint8_t pin, Type type = Type::OUTPUT, uint8_t outputLevel = 0);
    Pin(const Pin& other); // copy ctor
    Pin& operator=(const Pin& other); // copy operator
    void ReInit();
    ~Pin();
    uint8_t GetPin() const;
    static bool Configure(uint8_t pin, Type type, uint8_t outputLevel = 0);
    bool DigitalWrite(int val) const;
    bool GetDigitalWrite() const;
    bool DigitalToggle() const;
    bool DigitalRead() const;

    enum class TriggerType : uint32_t
    {
        RISING  = GPIO_IRQ_EDGE_RISE,
        FALLING = GPIO_IRQ_EDGE_FALL,
        BOTH    = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
    };

    void SetInterruptCallback(std::function<void()> cbFn, TriggerType triggerType);
    void EnableInterrupt();
    void DisableInterrupt();

    static void SetupShell();


private:

    static void InterruptHandler(uint pin, uint32_t eventMask);
    static bool PinValid(uint8_t pin);
    void Assign(const Pin& other);
    void IncrRef(uint8_t pin);
    void DecrRef(uint8_t pin);
    void RefCountDestroy(uint8_t pin);


private:

    static const uint8_t PIN_COUNT = 30;

    struct PinData
    {
        // reference count
        uint8_t count = 0;

        // digital pin members
        Type     type_            = Type::OUTPUT;
        uint8_t  outputLevel_     = 0;
        uint64_t interruptTimeMs_ = 0;

        // interrupt members
        bool                  intEnabled_  = false;
        std::function<void()> cbFn_        = []{};
        TriggerType           triggerType_ = TriggerType::FALLING;
    };

    static PinData pin__data_[PIN_COUNT];

    static PinData &GetPinData(uint8_t pin);

    static const uint32_t DEBOUNCE_TIME_MS = 250;

    // the only data member, really just an id of data stored elsewhere
    uint8_t pin_ = PIN_COUNT;
};


