#pragma once

#include <functional>

#include <zephyr/usb/usb_device.h>

#include "Evm.h"


// https://docs.zephyrproject.org/apidoc/latest/group____usb__device__core__api.html
// https://docs.zephyrproject.org/apidoc/latest/group____usb__device__controller__api.html
// https://devzone.nordicsemi.com/f/nordic-q-a/79205/usb-connect-event-and-disconnect-event-subscription
// 
// the sample cdc_acm looks to have some simple and high value functionality on display
// especially in the new api.
//

class USB
{
public:
    static void Enable()
    {
        enableTime_ = PAL.Micros();

        usb_enable(Handler);
    }

    static void SetCallbackConnected(function<void()> cb)
    {
        cbOnConnected_ = cb;
    }

    static void SetCallbackDisconnected(function<void()> cb)
    {
        cbOnDisconnected_ = cb;
    }

    static string StatusToStr(usb_dc_status_code cb_status)
    {
        string retVal = "USB_DC_UNKNOWN";

        if (cb_status == USB_DC_ERROR)
        {
            retVal = "USB_DC_ERROR";
        }
        else if (cb_status == USB_DC_RESET)
        {
            retVal = "USB_DC_RESET";
        }
        else if (cb_status == USB_DC_CONNECTED)
        {
            retVal = "USB_DC_CONNECTED";
        }
        else if (cb_status == USB_DC_CONFIGURED)
        {
            retVal = "USB_DC_CONFIGURED";
        }
        else if (cb_status == USB_DC_DISCONNECTED)
        {
            retVal = "USB_DC_DISCONNECTED";
        }
        else if (cb_status == USB_DC_SUSPEND)
        {
            retVal = "USB_DC_SUSPEND";
        }
        else if (cb_status == USB_DC_RESUME)
        {
            retVal = "USB_DC_RESUME";
        }
        else if (cb_status == USB_DC_INTERFACE)
        {
            retVal = "USB_DC_INTERFACE";
        }
        else if (cb_status == USB_DC_SET_HALT)
        {
            retVal = "USB_DC_SET_HALT";
        }
        else if (cb_status == USB_DC_CLEAR_HALT)
        {
            retVal = "USB_DC_CLEAR_HALT";
        }
        else if (cb_status == USB_DC_SOF)
        {
            retVal = "USB_DC_SOF";
        }

        return retVal;
    }

private:

    inline static uint64_t enableTime_ = 0;

    static void Handler(usb_dc_status_code cb_status, const uint8_t *param)
    {
        // values chosen by eye, looking at debug statements from zephyr.
        // basically these correspond to when I can/can't use the device
        // for serial.
        // seemingly physical connect/disconnect is well outside those.
        //
        // hurried through this, I'm sure there's a state transition
        // diagram somewhere I should consult
        //
        // seems to be able to flap between states without going all the
        // way to the ones I want fully, so put in some protection bool
        // to ensure I'm handling correctly.

        uint64_t timeNow = PAL.Micros();
        Log("[", Commas(timeNow - enableTime_) , "] ***** USB Status Changed to ", cb_status, " - ", StatusToStr(cb_status), " *****");

        enableTime_ = timeNow;

        if (cb_status == USB_DC_CONFIGURED)
        {
            if (!wasConnected_)
            {
                Evm::QueueWork("USB_CONNECTED", []{
                    cbOnConnected_();
                });

                wasConnected_ = true;
            }
        }
        else if (cb_status == USB_DC_SUSPEND)
        {
            if (wasConnected_)
            {
                Evm::QueueWork("USB_DISCONNECTED", []{
                    cbOnDisconnected_();
                });

                wasConnected_ = false;
            }
        }
    }

    inline static function<void()> cbOnConnected_ = []{};
    inline static function<void()> cbOnDisconnected_ = []{};
    inline static bool wasConnected_ = false;
};

/*
enum usb_dc_status_code {
	// USB error reported by the controller
	USB_DC_ERROR,
	// USB reset
	USB_DC_RESET,
	// USB connection established, hardware enumeration is completed
	USB_DC_CONNECTED,
	// USB configuration done
	USB_DC_CONFIGURED,
	// USB connection lost
	USB_DC_DISCONNECTED,
	// USB connection suspended by the HOST
	USB_DC_SUSPEND,
	// USB connection resumed by the HOST
	USB_DC_RESUME,
	// USB interface selected
	USB_DC_INTERFACE,
	// Set Feature ENDPOINT_HALT received
	USB_DC_SET_HALT,
	// Clear Feature ENDPOINT_HALT received
	USB_DC_CLEAR_HALT,
	// Start of Frame received
	USB_DC_SOF,
	// Initial USB connection status
	USB_DC_UNKNOWN
};


typedef void (*usb_dc_status_callback)(enum usb_dc_status_code cb_status,
				                       const uint8_t *param);



*/
