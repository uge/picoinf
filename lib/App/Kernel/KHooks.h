#include "FreeRTOS.h"
#include "task.h"

extern "C"
{
extern void vApplicationIdleHook();
extern void vApplicationTickHook();
extern void vApplicationMallocFailedHook();
extern void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
}