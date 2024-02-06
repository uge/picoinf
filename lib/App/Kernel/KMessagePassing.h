#pragma once

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

// #include "KTime.h"


// Thoughts on ISRs and message passing
// https://developer.nordicsemi.com/nRF_Connect_SDK/doc/1.4.2/zephyr/reference/kernel/other/interrupts.html
// https://docs.zephyrproject.org/latest/kernel/services/index.html#data-passing
//
// I had a hell of a time trying to, and failing at, passing data from ISRs to Evm via
// both message passing and via thread wakeup.
//
// I don't have detailed notes, but I kept seeing asserts issued regarding spinlocks already
// being held as well as complaints about the calls coming from an ISR.
//
// According to the docs, passing data out of an ISR is exactly the thing these are for.
//
// I don't know if this issue below is resolved, but seemed to be indicating that
// when pasing data to these mechanisms, they ultimately trigger scheduling logic
// which seems to be problematic for whatever the current implementation (in 2019 anyway,
// now being late 2022).
// https://github.com/zephyrproject-rtos/zephyr/issues/16273
// 
// For sure, the main thread is (almost always) blocking on waiting on receiving data from
// these message passing mechanisms, and certainly will wakeup when they get it, and I think
// that is the reportedly malfunctioning part of the issue above.
//
// That's confusing to me though, because this is the very purpose of these mechanisms, right?
//
// Before message passing I had tried k_wakeup on a sleeping Evm, and that would mostly work
// except at various times it also had some issue I forget now.  I'm exhausted after days of
// non-stop banging my head against the wall.
//
// I am now simply not sleeping except to allow other threads to work.  My ISR handling
// code no longer attempts to 'wake' the evm thread.  I'm simply relying on checking my work
// queues frequently.



// https://www.freertos.org/a00018.html (API)
// https://www.freertos.org/Embedded-RTOS-Queues.html
template <typename T, uint16_t SIZE>
class KMessagePipe
{
public:
    KMessagePipe()
    {
        msgPipe_ = xQueueCreate(SIZE, (UBaseType_t)sizeof(T));
    }

    ~KMessagePipe()
    {
        vQueueDelete(msgPipe_);
    }

    int Put(T &&val, TickType_t timeout = portMAX_DELAY)
    {
        return Put(val, timeout);
    }

    int Put(T &val, TickType_t timeout = portMAX_DELAY)
    {
        return xQueueSendToBack(msgPipe_, (void *)&val, timeout);
    }

    int Put(T *valList, uint16_t valListLen, TickType_t timeout = portMAX_DELAY)
    {
        BaseType_t retVal;
        for (uint32_t i = 0; i < valListLen; ++i)
        {
            retVal = Put(valList[i], timeout);
        }

        return retVal;
    }

    int Get(T &val, TickType_t timeout = portMAX_DELAY)
    {
        BaseType_t retVal = xQueueReceive(msgPipe_, (void *)&val, timeout);

        return retVal == pdTRUE ? sizeof(val) : 0;
    }

    void Flush()
    {
        xQueueReset(msgPipe_);
    }


private:
    QueueHandle_t msgPipe_;
};



// https://www.freertos.org/a00113.html
// https://www.freertos.org/Embedded-RTOS-Binary-Semaphores.html
// https://www.freertos.org/Real-time-embedded-RTOS-Counting-Semaphores.html
class KSemaphore
{
public:
    KSemaphore(uint32_t initialCount = 0,
               uint32_t countLimit   = UINT32_MAX)
    {
        sem_ = xSemaphoreCreateCounting(countLimit, initialCount);
    }

    ~KSemaphore()
    {
        vSemaphoreDelete(sem_);
    }

    bool Take(TickType_t timeout = portMAX_DELAY)
    {
        BaseType_t retVal = xSemaphoreTake(sem_, timeout);

        return retVal == pdTRUE;
    }

    void Give()
    {
        xSemaphoreGive(sem_);
    }


private:
    SemaphoreHandle_t  sem_;
};


