#pragma once

#include "Evm.h"
#include "Log.h"

#include "tusb.h"

class USB
{
public:

    // does this need to be its own KTask which uses
    // FreeRTOS blocking (how?)
    static void Test()
    {
        tusb_init();

        static TimedEventHandlerDelegate tedRunOnce;
        tedRunOnce.SetCallback([]{
            // has to run after scheduler started for some reason
            tud_init(0);
        });
        tedRunOnce.RegisterForTimedEvent(0);

        static TimedEventHandlerDelegate tedRuntime;
        tedRuntime.SetCallback([]{
            tud_task();
        }, "TUD_TASK");
        tedRuntime.RegisterForTimedEventInterval(5, 0);
    }

private:
};




extern "C"
{
extern uint8_t const * tud_descriptor_device_cb(void);
extern uint8_t const * tud_descriptor_configuration_cb(uint8_t index);
extern uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid);
extern void tud_mount_cb(void);
extern void tud_umount_cb(void);
extern void tud_suspend_cb(bool remote_wakeup_en);
extern void tud_resume_cb(void);
}