#pragma once

#include "Clock.h"
#include "Evm.h"
#include "KTask.h"
#include "Log.h"
#include "Timeline.h"
#include "USB_CDC.h"

#include <functional>
#include <vector>


/////////////////////////////////////////////////////////////////////
// USB
/////////////////////////////////////////////////////////////////////

class USB
{
    /////////////////////////////////////////////
    // Startup
    /////////////////////////////////////////////

public:

    static void Init();
    static void SetupShell();


    /////////////////////////////////////////////
    // Device Descriptor functionality
    /////////////////////////////////////////////

public:

    static void SetVid(uint16_t vid);
    static void SetPid(uint16_t pid);
    static void SetDevice(uint16_t device);
    static void SetCallbackVbusConnected(std::function<void()> cbFn);
    static void SetCallbackVbusDisconnected(std::function<void()> cbFn);
    static void SetCallbackConnected(std::function<void()> cbFn);
    static void SetCallbackDisconnected(std::function<void()> cbFn);
    static void EnablePowerSaveMode();
    static void DisablePowerSaveMode();


private:

    static void OnPinVbusInterrupt();


public:

    static uint8_t const *tud_descriptor_device_cb();

    inline static Pin                   pVbus_;
    inline static bool                  powerSaveMode_ = false;
    inline static std::function<void()> fnCbVbusConnected_    = []{};
    inline static std::function<void()> fnCbVbusDisconnected_ = []{};

    inline static std::function<void()> fnCbConnected_    = []{};
    inline static std::function<void()> fnCbDisconnected_ = []{};


private:

    inline static bool initHasRun_ = false;

    inline static uint16_t vid_    = 0x0000;
    inline static uint16_t pid_    = 0x0000;
    inline static uint16_t device_ = 0x0000;


    /////////////////////////////////////////////
    // Configuration Descriptor functionality
    /////////////////////////////////////////////

public:

    static uint8_t const *tud_descriptor_configuration_cb(uint8_t index);


    /////////////////////////////////////////////
    // BOS Descriptor functionality
    /////////////////////////////////////////////

public:



public:

    static uint8_t const *tud_descriptor_bos_cb();
    static bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, void *request);



    /////////////////////////////////////////////
    // String Descriptor functionality
    /////////////////////////////////////////////

public:

    static void SetStringManufacturer(std::string str);
    static void SetStringProduct(std::string str);
    static void SetStringSerial(std::string str);
    static void SetStringCdcInterface(std::string str);
    static void SetStringVendorInterface(std::string str);


public:

    static uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid);


private:

    inline static vector<std::string> strList_ = {
        "Manufacturer",
        "Product",
        "",
        "CDC Interface",
        "Vendor Interface",
    };

    inline static std::string &manufacturer_    = strList_[0];
    inline static std::string &product_         = strList_[1];
    inline static std::string &serial_          = strList_[2];;
    inline static std::string &cdcInterface_    = strList_[3];
    inline static std::string &vendorInterface_ = strList_[4];


    /////////////////////////////////////////////
    // CDC functionality
    /////////////////////////////////////////////

public:

    static USB_CDC *GetCdcInstance(uint8_t instance);


public:

    static void tud_cdc_rx_cb(uint8_t itf);
    static void tud_cdc_tx_complete_cb(uint8_t itf);
    static void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts);


private:

    inline static Timeline t_;
    inline static vector<USB_CDC> cdcList_ = { { 0, t_ } };
};


enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_VENDOR,
    ITF_NUM_TOTAL
};
