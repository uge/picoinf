#include "USB.h"
#include "Log.h"
#include "KTask.h"
#include "Shell.h"
#include "Timeline.h"
#include "Utl.h"

#include "tusb.h"

using namespace std;

#include "StrictMode.h"



/////////////////////////////////////////////
// Device Descriptor functionality
/////////////////////////////////////////////

void USB::SetVid(uint16_t vid)
{
    vid_ = vid;
}

void USB::SetPid(uint16_t pid)
{
    pid_ = pid;
}

void USB::SetDevice(uint16_t device)
{
    device_ = device;
}

void USB::SetCallbackVbusConnected(std::function<void()> cbFn)
{
    fnCbVbusConnected_ = cbFn;
}

void USB::SetCallbackVbusDisconnected(std::function<void()> cbFn)
{
    fnCbVbusDisconnected_ = cbFn;
}

void USB::SetCallbackConnected(std::function<void()> cbFn)
{
    fnCbConnected_ = cbFn;
}

void USB::SetCallbackDisconnected(std::function<void()> cbFn)
{
    fnCbDisconnected_ = cbFn;
}

void USB::EnablePowerSaveMode()
{
    if (PAL.GetPicoBoard() == "pico_w")
    {
        Log("NOT entering USB power save mode -- PicoW");
    }
    else
    {
        Log("Enabling USB power save mode");

        powerSaveMode_ = true;

        // simulate the level state change if Init hasn't yet been called
        if (initHasRun_ == false)
        {
            pVbus_ = Pin(24, Pin::Type::INPUT);
            OnPinVbusInterrupt();
        }
        else
        {
            if (pVbus_.DigitalRead())
            {
                Clock::EnableUSB();
            }
            else
            {
                Clock::DisableUSB();
            }
        }
    }
}

void USB::DisablePowerSaveMode()
{
    if (PAL.GetPicoBoard() == "pico_w")
    {
        Log("NOT disabling USB power save mode -- PicoW");
    }
    else
    {
        Log("Disabling USB power save mode");

        powerSaveMode_ = false;

        Clock::EnableUSB();
    }
}

void USB::OnPinVbusInterrupt()
{
    if (pVbus_.DigitalRead())
    {
        Log("VBUS Detected HIGH");

        fnCbVbusConnected_();

        if (powerSaveMode_)
        {
            Clock::EnableUSB();
        }
    }
    else
    {
        Log("VBUS Detected LOW");

        fnCbVbusDisconnected_();

        if (powerSaveMode_)
        {
            Clock::DisableUSB();
        }
    }
}



/////////////////////////////////////////////
// String Descriptor functionality
/////////////////////////////////////////////

void USB::SetStringManufacturer(std::string str)
{
    manufacturer_ = str;
}

void USB::SetStringProduct(std::string str)
{
    product_ = str;
}

void USB::SetStringSerial(std::string str)
{
    serial_ = str;
}

void USB::SetStringCdcInterface(std::string str)
{
    cdcInterface_ = str;
}

void USB::SetStringVendorInterface(std::string str)
{
    vendorInterface_ = str;
}



/////////////////////////////////////////////
// CDC functionality
/////////////////////////////////////////////

USB_CDC *USB::GetCdcInstance(uint8_t instance)
{
    USB_CDC *retVal = nullptr;

    if (instance < cdcList_.size())
    {
        retVal = &cdcList_[instance];
    }

    return retVal;
}


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

    if (PAL.GetPicoBoard() == "pico_w")
    {
        pVbus_ = Pin(24, Pin::Type::INPUT);
        pVbus_.SetInterruptCallback([]{ OnPinVbusInterrupt(); }, Pin::TriggerType::BOTH);
        pVbus_.EnableInterrupt();
    }

    Log("USB Init");
    Log("VID: ", ToHex(vid_), ", PID: ", ToHex(pid_), ", Device: ", ToHex(device_));
    Log("Manufacturer    : ", manufacturer_);
    Log("Product         : ", product_);
    Log("Serial          : ", serial_);
    Log("CDC Interface   : ", cdcInterface_);
    Log("Vendor Interface: ", vendorInterface_);
    
    // Start the thread after the event manager has begun, this allows
    // main code to register for callbacks for events that this
    // thread would otherwise process before main code had that chance
    static Timer ted;
    ted.SetCallback([]{
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
    });
    ted.TimeoutInMs(0);

    initHasRun_ = true;
}

void USB::SetupShell()
{
    Shell::AddCommand("usb.powersave", [](vector<string> argList){
        if (atoi(argList[0].c_str()))
        {
            EnablePowerSaveMode();
        }
        else
        {
            DisablePowerSaveMode();
        }
    }, { .argCount = 1, .help = "power save mode on(1) or off(0)"});

    Shell::AddCommand("usb.t.max", [](vector<string> argList){
        t_.SetMaxEvents((uint32_t)atoi(argList[0].c_str()));
    }, { .argCount = 1, .help = "timeline set max events"});

    Shell::AddCommand("usb.t.report", [](vector<string> argList){
        t_.Report();
    }, { .argCount = 0, .help = "timeline report"});

    Shell::AddCommand("usb.cdc0.stats", [](vector<string> argList){
        USB_CDC *cdc0 = USB::GetCdcInstance(0);

        cdc0->ReportStats();
    }, { .argCount = 0, .help = "cdc0 stats"});
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

