#pragma once

#include "FreeRTOS.h"
#include "task.h"

extern "C"
{
extern void vApplicationIdleHook();
extern void vApplicationTickHook();
extern void vApplicationMallocFailedHook();
extern void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
extern void vApplicationGetIdleTaskMemory(StaticTask_t ** ppxIdleTaskTCBBuffer,
                                          StackType_t  ** ppxIdleTaskStackBuffer,
                                          uint32_t      * pulIdleTaskStackSize);
extern void vApplicationGetTimerTaskMemory(StaticTask_t ** ppxTimerTaskTCBBuffer,
                                           StackType_t  ** ppxTimerTaskStackBuffer,
                                           uint32_t      * pulTimerTaskStackSize);

}