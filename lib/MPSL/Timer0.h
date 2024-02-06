#pragma once

#include <hal/nrf_timer.h>


///////////////////////////////////////////////////////////////////////////////
// Timer Control
///////////////////////////////////////////////////////////////////////////////

// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Ftimer.html&cp=4_0_0_5_29

// I want no-skew interval support.
// So I should not lose track of when the prior event fired, so when I get a
// new interval, I'm able to offset its endpoint exactly from the prior.
class Timer0
{
public:

    static volatile uint32_t Chan0GetTimerValue()
    {
        nrf_timer_task_trigger(NRF_TIMER0, NRF_TIMER_TASK_CAPTURE0);
        return nrf_timer_cc_get(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0);
    }

    // assumes there's an interrupt going to be triggered by doing this
    static void Chan0TriggerNow()
    {
        nrf_timer_task_trigger(NRF_TIMER0, NRF_TIMER_TASK_CAPTURE0);
        uint32_t timerCurrentValue = nrf_timer_cc_get(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0);
        nrf_timer_cc_set(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0, timerCurrentValue + 5);
    }

    static bool Chan0IsTriggered()
    {
        // return nrf_timer_event_check(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0);
        return NRF_TIMER0->EVENTS_COMPARE[0];
    }

    static void Chan0ClearInterrupt()
    {
        nrf_timer_int_disable(NRF_TIMER0, NRF_TIMER_INT_COMPARE0_MASK);
        nrf_timer_event_clear(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0);
    }

    static void Chan0SetByIncr(uint32_t us)
    {
        // get the prior CC[1] value
        uint32_t priorFireTime = nrf_timer_cc_get(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0);
        // add us to it
        uint32_t nextTime = priorFireTime + us;
        // enable
        nrf_timer_cc_set(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0, nextTime);
        nrf_timer_int_enable(NRF_TIMER0, NRF_TIMER_INT_COMPARE0_MASK);
    }


    // assumes there's an interrupt going to be triggered by doing this
    static void Chan1TriggerNow()
    {
        nrf_timer_task_trigger(NRF_TIMER0, NRF_TIMER_TASK_CAPTURE1);
        uint32_t timerCurrentValue = nrf_timer_cc_get(NRF_TIMER0, NRF_TIMER_CC_CHANNEL1);
        nrf_timer_cc_set(NRF_TIMER0, NRF_TIMER_CC_CHANNEL1, timerCurrentValue + 5);
    }

    static bool Chan1IsTriggered()
    {
        return nrf_timer_event_check(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE1);
    }

    static void Chan1ClearInterrupt()
    {
        nrf_timer_int_disable(NRF_TIMER0, NRF_TIMER_INT_COMPARE1_MASK);
        nrf_timer_event_clear(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE1);
    }

    static void Chan1SetByIncr(uint32_t us)
    {
        // get the prior CC[1] value
        uint32_t priorFireTime = nrf_timer_cc_get(NRF_TIMER0, NRF_TIMER_CC_CHANNEL1);
        // add us to it
        uint32_t nextTime = priorFireTime + us;
        // enable
        nrf_timer_cc_set(NRF_TIMER0, NRF_TIMER_CC_CHANNEL1, nextTime);
        nrf_timer_int_enable(NRF_TIMER0, NRF_TIMER_INT_COMPARE1_MASK);
    }
};

