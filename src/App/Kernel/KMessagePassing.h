#pragma once

#include "KTime.h"
#include "PAL.h"

#include "FreeRTOS.Wrapped.h"
#include "queue.h"
#include "semphr.h"

#include <cstdint>


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
        msgPipe_ = xQueueCreateStatic(SIZE,
                                      (UBaseType_t)sizeof(T),
                                      (uint8_t *)&pucQueueStorageBuffer_,
                                      &pxQueueBuffer_);
    }

    ~KMessagePipe()
    {
        vQueueDelete(msgPipe_);
    }

    int Put(T &&val, TickType_t timeoutUs = portMAX_DELAY)
    {
        return Put(val, timeoutUs);
    }

    int Put(T &val, TickType_t timeoutUs = portMAX_DELAY)
    {
        int retVal;

        if (PAL.InIsrReal())
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            retVal = xQueueSendToBackFromISR(msgPipe_, (void *)&val, &xHigherPriorityTaskWoken);

            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        else
        {
            retVal = xQueueSendToBack(msgPipe_, (void *)&val, KTime{timeoutUs});
        }

        return retVal;
    }

    int Put(T *valList, uint16_t valListLen, TickType_t timeoutUs = portMAX_DELAY)
    {
        int retVal = pdPASS;

        for (uint32_t i = 0; i < valListLen; ++i)
        {
            retVal = Put(valList[i], KTime{timeoutUs});
        }

        return retVal;
    }

    int Overwrite(T &val)
    {
        int retVal;

        if (PAL.InIsrReal())
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            retVal = xQueueOverwriteFromISR(msgPipe_, (void *)&val, &xHigherPriorityTaskWoken);

            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        else
        {
            retVal = xQueueOverwrite(msgPipe_, (void *)&val);
        }

        return retVal;
    }

    int Get(T &val, TickType_t timeoutUs = portMAX_DELAY)
    {
        BaseType_t retVal;
        
        if (PAL.InIsrReal())
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            retVal = xQueueReceiveFromISR(msgPipe_, (void *)&val, &xHigherPriorityTaskWoken);

            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        else
        {
            retVal = xQueueReceive(msgPipe_, (void *)&val, KTime{timeoutUs});
        }
        
        return retVal == pdTRUE ? sizeof(val) : 0;
    }

    uint16_t Count() const
    {
        uint16_t retVal;

        if (PAL.InIsrReal())
        {
            retVal = uxQueueMessagesWaiting(msgPipe_);
        }
        else
        {
            retVal = uxQueueMessagesWaitingFromISR(msgPipe_);
        }

        return retVal;
    } 

    bool IsFull()
    {
        return uxQueueSpacesAvailable(msgPipe_) == 0;
    }

    void Flush()
    {
        xQueueReset(msgPipe_);
    }


private:
    uint8_t       pucQueueStorageBuffer_[SIZE * sizeof(T)];
    StaticQueue_t pxQueueBuffer_;
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
        sem_ = xSemaphoreCreateCountingStatic(countLimit,
                                              initialCount,
                                              &pxSemaphoreBuffer_);
    }

    ~KSemaphore()
    {
        vSemaphoreDelete(sem_);
    }

    bool Take(uint32_t timeoutUs = portMAX_DELAY)
    {
        BaseType_t retVal;
        
        if (PAL.InIsrReal())
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            retVal = xSemaphoreTakeFromISR(sem_, &xHigherPriorityTaskWoken);

            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        else
        {
            retVal = xSemaphoreTake(sem_, KTime{timeoutUs});
        }

        return retVal == pdTRUE;
    }

    void Give()
    {
        if (PAL.InIsrReal())
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            xSemaphoreGiveFromISR(sem_, &xHigherPriorityTaskWoken);

            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        else
        {
            xSemaphoreGive(sem_);
        }
    }


private:
    StaticSemaphore_t pxSemaphoreBuffer_;
    SemaphoreHandle_t sem_;
};


// Meant to represent a single object which can be updated safely.
//
// This will not block a reader.
// This will block a writer.
//
// If the reader can't get access to the protected object, the cached
// last value is used for immediate use.
template <typename T>
class KObject
: private KMessagePipe<T, 1>
{
    using Super = KMessagePipe<T, 1>;

public:

    const T &Get()
    {
        Super::Get(valLast_, 0);

        return valLast_;
    }

    void Put(const T &val)
    {
        Super::Overwrite(val);
    }

    void Put(const T &&val)
    {
        Put(val);
    }

private:

    T valLast_;
};


