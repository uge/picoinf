#include "USB.h"
#include "Log.h"
#include "KTask.h"
#include "KMessagePassing.h"
#include "Timeline.h"

#include "tusb.h"

#include <algorithm>
using namespace std;


/////////////////////////////////////////////////////////////////////
// CDC Callback Handlers
/////////////////////////////////////////////////////////////////////

void USB_CDC::tud_cdc_rx_cb()
{
    uint32_t bytesAvailable = tud_cdc_n_available(instance_);

    if (bytesAvailable)
    {
        vector<uint8_t> byteList(bytesAvailable);

        tud_cdc_n_read(instance_, byteList.data(), bytesAvailable);

        cbFnRx_(byteList);
    }
}

void USB::tud_cdc_rx_cb(uint8_t itf)
{
    if (itf < cdcList_.size())
    {
        cdcList_[itf].tud_cdc_rx_cb();
    }
}

// Invoked when received new data
void tud_cdc_rx_cb(uint8_t itf)
{
    USB::tud_cdc_rx_cb(itf);
}

void USB_CDC::tud_cdc_tx_complete_cb()
{
    // nothing to do
}

void USB::tud_cdc_tx_complete_cb(uint8_t itf)
{
    if (itf < cdcList_.size())
    {
        cdcList_[itf].tud_cdc_tx_complete_cb();
    }
}

// Invoked when a TX is complete and therefore space becomes available in TX buffer
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    USB::tud_cdc_tx_complete_cb(itf);
}

void USB_CDC::tud_cdc_line_state_cb(bool dtr, bool rts)
{
    dtr_ = dtr;
}

void USB::tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    if (itf < cdcList_.size())
    {
        cdcList_[itf].tud_cdc_line_state_cb(dtr, rts);
    }
}

// Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    USB::tud_cdc_line_state_cb(itf, dtr, rts);
}


/////////////////////////////////////////////////////////////////////
// CDC Interface
/////////////////////////////////////////////////////////////////////

void USB_CDC::Send(const uint8_t *buf, uint16_t bufLen)
{
    uint16_t bufMin = min((uint32_t)bufLen, tud_cdc_n_write_available(instance_));
    tud_cdc_n_write(instance_, buf, bufMin);
    tud_cdc_n_write_flush(instance_);
}

void USB_CDC::Clear()
{
    tud_cdc_n_write_clear(instance_);
}


/////////////////////////////////////////////////////////////////////
// Task running TinyUSB code
/////////////////////////////////////////////////////////////////////

static KTask<1000> t("TinyUSB", []{
    tusb_init();

    // has to run after scheduler started for some reason
    tud_init(0);

    while (true)
    {
        // blocks via FreeRTOS task sync mechanisms
        tud_task();
    }
}, 10);


/////////////////////////////////////////////////////////////////////
// TinyUSB Interfacing
/////////////////////////////////////////////////////////////////////

extern "C"
{

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

#define USB_VID   0xCAFE
#define USB_BCD   0x0200

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}


//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_TOTAL
};


#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

#define EPNUM_MSC_OUT     0x03
#define EPNUM_MSC_IN      0x83


// #define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN)
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

  // Interface number, string index, EP Out & EP In address, EP size
//   TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "TinyUSB",                     // 1: Manufacturer
  "TinyUSB Device",              // 2: Product
  "123456789012",                // 3: Serials, should use chip ID
  "TinyUSB CDC",                 // 4: CDC Interface
  "TinyUSB MSC",                 // 5: MSC Interface
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }
  else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = (uint8_t) strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2*chr_count + 2));

  return _desc_str;
}






/////////////////////////////////////////////////////////////////////
// USB State Callbacks
/////////////////////////////////////////////////////////////////////

// Invoked when device is mounted
void tud_mount_cb(void)
{
    // Log("Mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    // Log("Unmounted");
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en)
{
    // Log("Suspended");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    // Log("Resumed");
}
}


/*



//------------- WebUSB BOS Platform -------------//

// Descriptor Length
#define TUD_BOS_WEBUSB_DESC_LEN         24

// Vendor Code, iLandingPage
#define TUD_BOS_WEBUSB_DESCRIPTOR(_vendor_code, _ipage) \
  TUD_BOS_PLATFORM_DESCRIPTOR(TUD_BOS_WEBUSB_UUID, U16_TO_U8S_LE(0x0100), _vendor_code, _ipage)

#define TUD_BOS_WEBUSB_UUID   \
  0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, \
  0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65

//------------- Microsoft OS 2.0 Platform -------------//
#define TUD_BOS_MICROSOFT_OS_DESC_LEN   28

// Total Length of descriptor set, vendor code
#define TUD_BOS_MS_OS_20_DESCRIPTOR(_desc_set_len, _vendor_code) \
  TUD_BOS_PLATFORM_DESCRIPTOR(TUD_BOS_MS_OS_20_UUID, U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(_desc_set_len), _vendor_code, 0)

#define TUD_BOS_MS_OS_20_UUID \
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, \
  0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F



*/

























