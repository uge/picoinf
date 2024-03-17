#include "USB.h"
#include "Log.h"
#include "KTask.h"
#include "Timeline.h"

#include "tusb.h"


/////////////////////////////////////////////////////////////////////
// Task running TinyUSB code
/////////////////////////////////////////////////////////////////////

void USB::Init()
{
    Timeline::Global().Event("USB::Init");

    if (serial_ == "")
    {
        // have to wait until now to set this, address not available
        // at the time when static init happens
        serial_ = PAL.GetAddress();
    }

    pVbus_.SetInterruptCallback([]{
        if (pVbus_.DigitalRead())
        {
            fnCbVbusConnected_();
        }
        else
        {
            fnCbVbusDisconnected_();
        }
    }, Pin::TriggerType::BOTH);
    pVbus_.EnableInterrupt();

    Log("USB Init");
    Log("VID: ", ToHex(vid_), ", PID: ", ToHex(pid_), ", Device: ", ToHex(device_));
    Log("Manufacturer    : ", manufacturer_);
    Log("Product         : ", product_);
    Log("Serial          : ", serial_);
    Log("CDC Interface   : ", cdcInterface_);
    Log("Vendor Interface: ", vendorInterface_);
    
    static KTask<256> t("TinyUSB", []{
        tusb_init();

        // has to run after scheduler started for some reason
        tud_init(0);

        while (true)
        {
            // blocks via FreeRTOS task sync mechanisms
            tud_task();
        }
    }, 5);
}


/////////////////////////////////////////////////////////////////////
// USB State Callbacks
/////////////////////////////////////////////////////////////////////

extern "C"
{

// Invoked when device is mounted
void tud_mount_cb(void)
{
    // Log("USB Mounted");

    USB::fnCbConnected_();
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    // Log("USB Unmounted");
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en)
{
    // Log("USB Suspended");

    USB::fnCbDisconnected_();
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    // Log("USB Resumed");
}

}   // extern "C"

