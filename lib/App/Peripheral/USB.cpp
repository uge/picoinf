#include "USB.h"

#include "Log.h"


extern "C"
{
uint8_t const * tud_descriptor_device_cb(void)
{
    Log("Get Device Descriptor");
    return nullptr;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    Log("Get Configuration Descriptor");
    return nullptr;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    Log("Get String Descriptor");
    return nullptr;
}

// Invoked when device is mounted
void tud_mount_cb(void)
{
    Log("Mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    Log("Unmounted");
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en)
{
    Log("Suspended");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    Log("Resumed");
}
}