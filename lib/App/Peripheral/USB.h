#pragma once

#include "Evm.h"
#include "KTask.h"
#include "Log.h"

#include "tusb.h"

#include <functional>
using namespace std;




/////////////////////////////////////////////////////////////////////
// USB_CDC
/////////////////////////////////////////////////////////////////////

class USB;

class USB_CDC
{
    friend class USB;

public:
    USB_CDC(uint8_t instance)
    : instance_(instance)
    {
        // nothing to do
    }

    void SetCallbackOnRx(function<void(vector<uint8_t> &byteList)> fn)
    {
        cbFnRx_ = fn;
    }

    bool GetDtr()
    {
        return dtr_;
    }

    void Send(const uint8_t *buf, uint16_t bufLen);
    void Clear();


private:

    void tud_cdc_rx_cb();
    void tud_cdc_tx_complete_cb();
    void tud_cdc_line_state_cb(bool dtr, bool rts);


private:

    function<void(vector<uint8_t> &byteList)> cbFnRx_ = [](vector<uint8_t> &){};

    bool dtr_ = false;

    uint8_t instance_;
};


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


    /////////////////////////////////////////////
    // Device Descriptor functionality
    /////////////////////////////////////////////

public:

    static void SetVid(uint16_t vid)
    {
        vid_ = vid;
    }

    static void SetPid(uint16_t pid)
    {
        pid_ = pid;
    }

    static void SetDevice(uint16_t device)
    {
        device_ = device;
    }


public:

    static uint8_t const *tud_descriptor_device_cb();

private:

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
    static bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);



    /////////////////////////////////////////////
    // String Descriptor functionality
    /////////////////////////////////////////////

public:

    static void SetStringManufacturer(string str)
    {
        manufacturer_ = str;
    }

    static void SetStringProduct(string str)
    {
        product_ = str;
    }

    static void SetStringSerial(string str)
    {
        serial_ = str;
    }

    static void SetStringCdcInterface(string str)
    {
        cdcInterface_ = str;
    }

    static void SetStringVendorInterface(string str)
    {
        vendorInterface_ = str;
    }


public:

    static uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid);


private:

    inline static vector<string> strList_ = {
        "Manufacturer",
        "Product",
        "",
        "CDC Interface",
        "Vendor Interface",
    };

    inline static string &manufacturer_    = strList_[0];
    inline static string &product_         = strList_[1];
    inline static string &serial_          = strList_[2];;
    inline static string &cdcInterface_    = strList_[3];
    inline static string &vendorInterface_ = strList_[4];


    /////////////////////////////////////////////
    // CDC functionality
    /////////////////////////////////////////////

public:

    static USB_CDC *GetCdcInstance(uint8_t instance)
    {
        USB_CDC *retVal = nullptr;

        if (instance < cdcList_.size())
        {
            retVal = &cdcList_[instance];
        }

        return retVal;
    }


public:

    static void tud_cdc_rx_cb(uint8_t itf);
    static void tud_cdc_tx_complete_cb(uint8_t itf);
    static void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts);


private:

    inline static vector<USB_CDC> cdcList_ = { 0 };
};


enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_VENDOR,
    ITF_NUM_TOTAL
};
