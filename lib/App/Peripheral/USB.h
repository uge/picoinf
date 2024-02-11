#pragma once

#include "Log.h"

#include "tusb.h"

class USB
{
public:

    static void Test()
    {
        LogModeSync();

        tusb_init();

        while (true)
        {
            tud_task();
        }
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