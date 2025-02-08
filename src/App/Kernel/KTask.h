#pragma once

#include "FreeRTOS.h"
#include "task.h"

#include <cstdint>
#include <string>
#include <functional>


template <uint16_t STACK_SIZE>
class KTask
{
public:
    KTask(std::string           name,
          std::function<void()> fn,
          uint32_t              priority)
    : name_(name)
    , fn_(fn)
    {
        h_ = xTaskCreateStatic(TaskRunner,
                               name_.c_str(),
                               STACK_SIZE,
                               this,
                               priority,
                               puxStackBuffer_,
                               &pxTaskBuffer_);
    }

private:
    static void TaskRunner(void* pKtask)
    {
        KTask *p = (KTask *)pKtask;

        p->fn_();

        vTaskDelete(nullptr);
    }

private:
    std::string name_;
    std::function<void()> fn_;

    StackType_t puxStackBuffer_[STACK_SIZE];
    StaticTask_t pxTaskBuffer_;
    TaskHandle_t h_;
};
