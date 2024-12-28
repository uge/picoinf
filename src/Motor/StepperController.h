#pragma once

#include "PAL.h"
#include "Pin.h"


/*
 * This represents one of the two H-Bridges in the L293D chip.
 *
 * In this chip, per-H-Bridge, there is an Enable pin which affects whether
 * current flow is going to be affected by the two driver inputs (S1, S2).
 *
 * S1 and S2 are used to control the flow of current and should be used together
 * to allow for current flow or not.
 *
 * For example:
 * When S1 is enabled, current flows from it.
 * When S1 is disabled, current flows into it.
 *
 * Therefore, to get the functionality desired, S1 and S2 should always be
 * opposite of one another to allow for current flow in a given direction.
 *
 *     enabled ->
 * ------ S1 -----
 *               |
 * ------ S2 -----
 *    <- disabled
 *
 */
class HBridgeL293D
{
public:
    void Init(Pin pinEnable, Pin pinS1, Pin pinS2)
    {
        pinEnable_ = pinEnable;
        pinS1_ = pinS1;
        pinS2_ = pinS2;

        Disable();
        SetFlowDirectionTopToBottom();
    }

    void Enable()  { pinEnable_.DigitalWrite(1);  }
    void Disable() { pinEnable_.DigitalWrite(0);   }
    
    void SetFlowDirectionTopToBottom()
    {
        pinS1_.DigitalWrite(1);
        pinS2_.DigitalWrite(0);
    }
    
    void SetFlowDirectionBottomToTop()
    {
        pinS1_.DigitalWrite(0);
        pinS2_.DigitalWrite(1);
    }
    
    void SetFlowDisabled()
    {
        pinS1_.DigitalWrite(0);
        pinS2_.DigitalWrite(0);
    }

private:
    Pin pinEnable_;
    Pin pinS1_;
    Pin pinS2_;
};



//
// https://www.youtube.com/watch?v=Qc8zcst2blU
//
class StepperControllerBipolar
{
public:
    void Init(Pin pinEnable,
              Pin pinPhase1S2,
              Pin pinPhase1S1,
              Pin pinPhase2S1,
              Pin pinPhase2S2)
    {
        h1_.Init(pinEnable, pinPhase1S1, pinPhase1S2);
        h2_.Init(pinEnable, pinPhase2S1, pinPhase2S2);

        halfStepStateListIdx_ = 0;
        
        Disable();
    }

    void SetStepDelayUs(uint32_t stepDelayUs)
    {
        stepDelayUs_ = stepDelayUs;
    }

    void Enable()
    {
        h1_.Enable();
        h2_.Enable();
    }

    void Disable()
    {
        h1_.Disable();
        h2_.Disable();
    }
    
    void HalfStepCCW(uint32_t count = 1)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            // Get new index into half step array
            if (halfStepStateListIdx_ == 0) { halfStepStateListIdx_ = 14; }
            else                            { halfStepStateListIdx_ -= 2; }
            
            H1(halfStepStateList_[halfStepStateListIdx_ + 0]);
            H2(halfStepStateList_[halfStepStateListIdx_ + 1]);

            PAL.DelayBusyUs(stepDelayUs_);
        }
    }
    
    void HalfStepCW(uint32_t count = 1)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            // Get new index into half step array
            halfStepStateListIdx_ = (halfStepStateListIdx_ + 2) % 16;
            
            H1(halfStepStateList_[halfStepStateListIdx_ + 0]);
            H2(halfStepStateList_[halfStepStateListIdx_ + 1]);

            PAL.DelayBusyUs(stepDelayUs_);
        }
    }
    
    void FullStepCCW(uint32_t count = 1)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            // Get new index into half step array
            if (halfStepStateListIdx_ == 0) { halfStepStateListIdx_ = 14; }
            else                            { halfStepStateListIdx_ -= 2; }
            if (halfStepStateListIdx_ == 0) { halfStepStateListIdx_ = 14; }
            else                            { halfStepStateListIdx_ -= 2; }
            
            H1(halfStepStateList_[halfStepStateListIdx_ + 0]);
            H2(halfStepStateList_[halfStepStateListIdx_ + 1]);

            PAL.DelayBusyUs(stepDelayUs_);
        }
    }
    
    void FullStepCW(uint32_t count = 1)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            // Get new index into half step array
            halfStepStateListIdx_ = (halfStepStateListIdx_ + 2) % 16;
            halfStepStateListIdx_ = (halfStepStateListIdx_ + 2) % 16;
            
            H1(halfStepStateList_[halfStepStateListIdx_ + 0]);
            H2(halfStepStateList_[halfStepStateListIdx_ + 1]);
            
            PAL.DelayBusyUs(stepDelayUs_);
        }
    }

    enum struct HState : uint8_t
    {
        FLOW_TOP_TO_BOTTOM = 0,
        FLOW_BOTTOM_TO_TOP,
        FLOW_DISABLED
    };
    
private:

    static void SetHState(HBridgeL293D &h, HState state)
    {
        switch (state)
        {
        case HState::FLOW_TOP_TO_BOTTOM: h.SetFlowDirectionTopToBottom(); break;
        case HState::FLOW_BOTTOM_TO_TOP: h.SetFlowDirectionBottomToTop(); break;
        case HState::FLOW_DISABLED     : h.SetFlowDisabled();             break;
            
        default: break;
        }
    }
    
    void H1(HState state) { SetHState(h1_, state); }
    void H2(HState state) { SetHState(h2_, state); }
    
    HBridgeL293D h1_;
    HBridgeL293D h2_;
    
    uint8_t halfStepStateListIdx_;
    
    constexpr static const HState halfStepStateList_[16] = {
        HState::FLOW_TOP_TO_BOTTOM, HState::FLOW_BOTTOM_TO_TOP,
        HState::FLOW_TOP_TO_BOTTOM, HState::FLOW_DISABLED,
        HState::FLOW_TOP_TO_BOTTOM, HState::FLOW_TOP_TO_BOTTOM,
        HState::FLOW_DISABLED,      HState::FLOW_TOP_TO_BOTTOM,
        HState::FLOW_BOTTOM_TO_TOP, HState::FLOW_TOP_TO_BOTTOM,
        HState::FLOW_BOTTOM_TO_TOP, HState::FLOW_DISABLED,
        HState::FLOW_BOTTOM_TO_TOP, HState::FLOW_BOTTOM_TO_TOP,
        HState::FLOW_DISABLED,      HState::FLOW_BOTTOM_TO_TOP
    };

    uint32_t stepDelayUs_ = 3'000;
};



#if 0

/*
 *  Phase2  ------3
 *                3
 *             |--3    ( M )
 *             |  3
 *  Phase4  ---|--3  m m m m m
 *             |     |   |   |
 *             |     |   |   |
 *             /-----|---|   |
 *            /      |       |
 *     Ground     Phase1   Phase3
 *
 */
class StepperControllerUnipolar
{
public:
    StepperControllerUnipolar(uint8_t pinEnable,
                              uint8_t pinPhase1,
                              uint8_t pinPhase2,
                              uint8_t pinPhase3,
                              uint8_t pinPhase4)
    : pinEnable_(pinEnable, HIGH)
    , pinList_{
        { pinPhase1, LOW },
        { pinPhase2, LOW },
        { pinPhase3, LOW },
        { pinPhase4, LOW }
    }
    , stateListIdx_(0)
    {
        SetPinsAtIdx();
    }
   
    void HalfStepCCW()
    {
        stateListIdx_ = (stateListIdx_ == 0 ? 7 : stateListIdx_ - 1);
        
        SetPinsAtIdx();
    }
    
    void HalfStepCW()
    {
        stateListIdx_ = (stateListIdx_ + 1) % 8;
        
        SetPinsAtIdx();
    }
    
    void FullStepCCW()
    {
        stateListIdx_ = (stateListIdx_ == 0 ? 7 : stateListIdx_ - 1);
        stateListIdx_ = (stateListIdx_ == 0 ? 7 : stateListIdx_ - 1);
        
        SetPinsAtIdx();
    }
    
    void FullStepCW()
    {
        stateListIdx_ = (stateListIdx_ + 1) % 8;
        stateListIdx_ = (stateListIdx_ + 1) % 8;
        
        SetPinsAtIdx();
    }

private:

    void SetPinsAtIdx()
    {
        PAL.DigitalWrite(pinList_[0], !!(stateList_[0] & _BV(7 - stateListIdx_)));
        PAL.DigitalWrite(pinList_[1], !!(stateList_[1] & _BV(7 - stateListIdx_)));
        PAL.DigitalWrite(pinList_[2], !!(stateList_[2] & _BV(7 - stateListIdx_)));
        PAL.DigitalWrite(pinList_[3], !!(stateList_[3] & _BV(7 - stateListIdx_)));
    }
    
    Pin pinEnable_;
    Pin pinList_[4];
    
    /*
     * There are 8 half-step states
     *
     *    0   1   2   3   4   5   6   7
     * --------------------------------------
     * 0  x   x                       x
     * 1      x   x   x
     * 2              x   x   x
     * 3                      x   x   x
     *
     */

    constexpr static const uint8_t stateList_[4] = {
        //01234567 -- state indices, not bit positions, corrected for later
        0b11000001,
        0b01110000,
        0b00011100,
        0b00000111,
    };
    uint8_t stateListIdx_;
};

#endif


#if 0

template <typename T>
class StepperControllerAsync
{
public:
    StepperControllerAsync(T &sc)
    : sc_(sc)
    , mode_(Mode::FOREVER)
    , stepSize_(StepSize::HALF)
    , stepCount_(0)
    , stepDurationUs_(0)
    {
        ted_.SetCallback([this](){ OnTimeout(); });
    }
    
    
    // Half Steps
    
    void HalfStepCCW(uint32_t         stepCount,
                     uint32_t         stepDurationMs,
                     function<void()> cbFnOnComplete = [](){})
    {
        StartStepLimit(Direction::CCW,
                       StepSize::HALF,
                       stepCount,
                       stepDurationMs,
                       cbFnOnComplete);
    }
    
    void HalfStepForeverCCW(uint32_t stepDurationMs)
    {
        StartStepForever(Direction::CCW,
                         StepSize::HALF,
                         stepDurationMs);
    }
    
    void HalfStepCW(uint32_t         stepCount,
                    uint32_t         stepDurationMs,
                    function<void()> cbFnOnComplete = [](){})
    {
        StartStepLimit(Direction::CW,
                       StepSize::HALF,
                       stepCount,
                       stepDurationMs,
                       cbFnOnComplete);
    }
    
    void HalfStepForeverCW(uint32_t stepDurationMs)
    {
        StartStepForever(Direction::CW,
                         StepSize::HALF,
                         stepDurationMs);
    }
    
    
    // Full Steps
    
    void FullStepCCW(uint32_t         stepCount,
                     uint32_t         stepDurationMs,
                     function<void()> cbFnOnComplete = [](){})
    {
        StartStepLimit(Direction::CCW,
                       StepSize::FULL,
                       stepCount,
                       stepDurationMs,
                       cbFnOnComplete);
    }
    
    void FullStepForeverCCW(uint32_t stepDurationMs)
    {
        StartStepForever(Direction::CCW,
                         StepSize::FULL,
                         stepDurationMs);
    }
    
    void FullStepCW(uint32_t         stepCount,
                    uint32_t         stepDurationMs,
                    function<void()> cbFnOnComplete = [](){})
    {
        StartStepLimit(Direction::CW,
                       StepSize::FULL,
                       stepCount,
                       stepDurationMs,
                       cbFnOnComplete);
    }
    
    void FullStepForeverCW(uint32_t stepDurationMs)
    {
        StartStepForever(Direction::CW,
                         StepSize::FULL,
                         stepDurationMs);
    }
    
    
    
    void Stop()
    {
        ted_.DeRegisterForIdleTimeHiResTimedEvent();
        
        cbFnOnComplete_ = [](){};
    }
    
private:

    enum struct Direction : uint8_t
    {
        CCW = 0,
        CW
    };
    
    enum struct Mode : uint8_t
    {
        FOREVER = 0,
        STEP_LIMIT
    };
    
    enum struct StepSize : uint8_t
    {
        FULL = 0,
        HALF
    };
    
    void StartStepLimit(Direction        direction,
                        StepSize         stepSize,
                        uint32_t         stepCount,
                        uint32_t         stepDurationMs,
                        function<void()> cbFnOnComplete)
    {
        Stop();
        
        mode_ = Mode::STEP_LIMIT;
        
        if (stepCount)
        {
            direction_      = direction;
            stepSize_       = stepSize;
            stepCount_      = stepCount;
            stepDurationUs_ = stepDurationMs * 1000;
            
            cbFnOnComplete_ = cbFnOnComplete;
            
            OnTimeout();
        }
        else
        {
            Finished();
        }
    }
    
    void Finished()
    {
        cbFnOnComplete_();
        
        Stop();
    }
    
    void StartStepForever(Direction direction,
                          StepSize  stepSize,
                          uint32_t  stepDurationMs)
    {
        Stop();
        
        mode_ = Mode::FOREVER;
        
        direction_      = direction;
        stepSize_       = stepSize;
        stepDurationUs_ = stepDurationMs * 1000;
        
        OnTimeout();
    }

    void OnTimeout()
    {
        ted_.RegisterForIdleTimeHiResTimedEventInterval(stepDurationUs_);
        
        if (stepSize_ == StepSize::FULL)
        {
            if (direction_ == Direction::CCW) { sc_.FullStepCCW(); }
            else                              { sc_.FullStepCW();  }
        }
        else
        {
            if (direction_ == Direction::CCW) { sc_.HalfStepCCW(); }
            else                              { sc_.HalfStepCW();  }
        }
        
        if (mode_ == Mode::STEP_LIMIT)
        {
            --stepCount_;
            
            if (!stepCount_)
            {
                Finished();
            }
        }
    }

    T &sc_;
    
    Direction direction_;
    Mode      mode_;
    StepSize  stepSize_;
    uint32_t  stepCount_;
    uint32_t  stepDurationUs_;
    
    function<void()> cbFnOnComplete_;
    
    IdleTimeHiResTimedEventHandlerDelegate ted_;
};

#endif









































