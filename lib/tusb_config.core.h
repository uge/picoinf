#pragma once

// Intended to be the constant configuration for base features
// for the core library.  Per-app configuration should include
// this and override and supplement if needed.







/////////////////////////////////////////////////////////////////////
// Enable Device and/or Host stacks
// TUD = TinyUSB Device
// TUH = TinyUSB Host
/////////////////////////////////////////////////////////////////////

#define CFG_TUD_ENABLED       1
#define CFG_TUH_ENABLED       0


/////////////////////////////////////////////////////////////////////
// Device Class CDC Configuration
/////////////////////////////////////////////////////////////////////

#define CFG_TUD_CDC  1

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE   1024
#define CFG_TUD_CDC_TX_BUFSIZE   1024

// CDC Endpoint transfer buffer size
#define CFG_TUD_CDC_EP_BUFSIZE   64


/////////////////////////////////////////////////////////////////////
// Vendor Class CDC Configuration
/////////////////////////////////////////////////////////////////////

#define CFG_TUD_VENDOR  1

#define CFG_TUD_VENDOR_RX_BUFSIZE  256
#define CFG_TUD_VENDOR_TX_BUFSIZE  256


/////////////////////////////////////////////////////////////////////
// Other Device Class Configuration
/////////////////////////////////////////////////////////////////////

#define CFG_TUD_MSC     0
#define CFG_TUD_HID     0
#define CFG_TUD_MIDI    0

// MSC Buffer size of Device Mass storage
#define CFG_TUD_MSC_EP_BUFSIZE   512


/////////////////////////////////////////////////////////////////////


// Configure Debug
#define CFG_TUSB_DEBUG  0


// Define platform

// Issue described here:
// https://github.com/hathach/tinyusb/pull/1993
// but basically in v1.5.0 of sdk, the tinyusb
// libs default to assuming OPT_OS_PICO, and no good way to change it
// outside of that to their liking, may be fixed in v1.6.0.
//
// The hack I've put in place is changing
// pico-sdk\lib\tinyusb\hw\bsp\rp2040\family.cmake to change that
// default value.
//
// The below definition is redundant, but was useful in causing
// warnings to get thrown as a result of a re-definition.
// I will leave that in place now as a visual indicator if
// anything changes it again.

#ifdef CFG_TUSB_OS
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)
//   #pragma message(VAR_NAME_VALUE(CFG_TUSB_OS))
#undef CFG_TUSB_OS
#endif
#define CFG_TUSB_OS   OPT_OS_FREERTOS


// nevermind I'm just overriding it like this here.
// I don't want to forever see a modified file in the
// repo when I do a git status.
// hopefully they fix this one day
#ifdef CFG_TUSB_OS
  #undef CFG_TUSB_OS
#endif
#define CFG_TUSB_OS OPT_OS_FREERTOS
