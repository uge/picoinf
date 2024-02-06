#pragma once

#include "FreeRTOS.h"
#include "queue.h"

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




// // https://docs.zephyrproject.org/latest/kernel/services/data_passing/pipes.html
// template <typename T, uint16_t SIZE>
// class KMessagePipe
// {
// public:
//     KMessagePipe()
//     {
//         k_pipe_init(&msgPipe_, buf_, sizeof(T) * SIZE);
//     }

//     ~KMessagePipe()
//     {
//         k_pipe_cleanup(&msgPipe_);
//     }

//     int Put(T &&val, TickType_t timeout = portMAX_DELAY)
//     {
//         size_t bytesWritten;
//         return k_pipe_put(&msgPipe_, &val, sizeof(T), &bytesWritten, sizeof(T), KTime{timeout});
//     }

//     int Put(T *valList, uint16_t valListLen, TickType_t timeout = portMAX_DELAY)
//     {
//         size_t bytesWritten;
//         return k_pipe_put(&msgPipe_, valList, sizeof(T) * valListLen, &bytesWritten, sizeof(T) * valListLen, KTime{timeout});
//     }

//     int Get(T &val, TickType_t timeout = portMAX_DELAY)
//     {
//         size_t bytesRead;
//         k_pipe_get(&msgPipe_, &val, sizeof(T), &bytesRead, sizeof(T), KTime{timeout});
//         return bytesRead;
//     }

//     void Flush()
//     {
//         k_pipe_flush(&msgPipe_);
//     }


// private:
//     k_pipe msgPipe_;
//     uint8_t buf_[sizeof(T) * SIZE];
// };


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
// class KSemaphore
// {
// public:
//     KSemaphore(uint32_t initialCount = 0,
//                uint32_t countLimit   = K_SEM_MAX_LIMIT)
//     {
//         k_sem_init(&sem_, initialCount, countLimit);
//     }

//     bool Take(k_timeout_t timeout)
//     {
//         int retVal = k_sem_take(&sem_, KTime{timeout});

//         return retVal == 0;
//     }

//     void Give()
//     {
//         k_sem_give(&sem_);
//     }

//     void Reset()
//     {
//         k_sem_reset(&sem_);
//     }

//     uint32_t Count()
//     {
//         return k_sem_count_get(&sem_);
//     }

// private:
//     k_sem sem_;
// };


