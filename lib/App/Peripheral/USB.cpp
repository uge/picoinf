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

void USB::Init()
{
    if (serial_ == "")
    {
        // have to wait until now to set this, address not available
        // at the time when static init happens
        serial_ = PAL.GetAddress();
    }

    Log("USB Init");
    Log("VID: ", ToHex(vid_), ", PID: ", ToHex(pid_));
    Log("Manufacturer : ", manufacturer_);
    Log("Product      : ", product_);
    Log("Serial       : ", serial_);
    Log("CDC Interface: ", cdcInterface_);
    LogNL();

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
}


/////////////////////////////////////////////////////////////////////
// TinyUSB Interfacing
/////////////////////////////////////////////////////////////////////

extern "C"
{


/////////////////////////////////////////////////////////////////////
// Device Descriptors
/////////////////////////////////////////////////////////////////////

static tusb_desc_device_t desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0,
    .idProduct          = 0,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
// Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
uint8_t const *USB::tud_descriptor_device_cb()
{
    desc_device.idVendor = vid_;
    desc_device.idProduct = pid_;

    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_device_cb(void)
{
    return USB::tud_descriptor_device_cb();
}


/////////////////////////////////////////////////////////////////////
// Configuration Descriptor
/////////////////////////////////////////////////////////////////////

enum
{
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
//   ITF_NUM_VENDOR,
  ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

#define EPNUM_VENDOR_OUT  0x03
#define EPNUM_VENDOR_IN   0x83


// #define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN)
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
// #define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_VENDOR_DESC_LEN)

static uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

  // Interface number, string index, EP Out & IN address, EP size
//   TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 5, EPNUM_VENDOR_OUT, 0x80 | EPNUM_VENDOR_IN, 64)
};

uint8_t const *USB::tud_descriptor_configuration_cb(uint8_t index)
{
    return desc_fs_configuration;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  return USB::tud_descriptor_configuration_cb(index);
}


/////////////////////////////////////////////////////////////////////
// String Descriptors
/////////////////////////////////////////////////////////////////////

uint16_t const *USB::tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    // build a structure that has:
    // first byte is byte length (including header)
    // second byte is string type
    // subsequent bytes - 2-byte chars
    // no null terminator
    static vector<uint8_t> byteList;

    uint16_t const *retVal = nullptr;
    
    byteList.clear();

    if (index == 0)
    {
        // 0 is supported language is English (0x0409)
        // put it in little-endian

        byteList.resize(2 + 2);
        byteList[1] = 0x09;
        byteList[2] = 0x04;

        retVal = (uint16_t const *)byteList.data();
    }
    else
    {
        // It's an indexed string.
        // map their index onto mine.
        uint8_t idx = index - 1;

        if (idx < strList_.size())
        {
            string &str = strList_[idx];

            byteList.resize(2 + (2 * str.length()));

            for (int i = 0; auto c : str)
            {
                byteList[2 + (i * 2)] = c;

                ++i;
            }

            retVal = (uint16_t const *)byteList.data();
        }
    }

    if (retVal)
    {
        byteList[0] = byteList.size();
        byteList[1] = TUSB_DESC_STRING;
    }

    return retVal;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    return USB::tud_descriptor_string_cb(index, langid);
}










/////////////////////////////////////////////////////////////////////
// BOS Descriptors
/////////////////////////////////////////////////////////////////////

#if 0




/* Microsoft OS 2.0 registry property descriptor
Per MS requirements https://msdn.microsoft.com/en-us/library/windows/hardware/hh450799(v=vs.85).aspx
device should create DeviceInterfaceGUIDs. It can be done by driver and
in case of real PnP solution device should expose MS "Microsoft OS 2.0
registry property descriptor". Such descriptor can insert any record
into Windows registry per device/configuration/interface. In our case it
will insert "DeviceInterfaceGUIDs" multistring property.

GUID is freshly generated and should be OK to use.

https://developers.google.com/web/fundamentals/native-hardware/build-for-webusb/
(Section Microsoft OS compatibility descriptors)
*/

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

#define MS_OS_20_DESC_LEN  0xB2

enum
{
    VENDOR_REQUEST_WEBUSB = 1,
    VENDOR_REQUEST_MICROSOFT = 2
};

// BOS Descriptor is required for webUSB
static uint8_t const desc_bos[] =
{
  // total length, number of device caps
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),

  // Vendor Code, iLandingPage
  TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),

  // Microsoft OS 2.0 descriptor
  TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)
};


uint8_t const *USB::tud_descriptor_bos_cb()
{
    Log("BOS Requested");
    return desc_bos;
}

uint8_t const * tud_descriptor_bos_cb(void)
{
  return USB::tud_descriptor_bos_cb();
}






#define URL  "example.tinyusb.org/webusb-serial/index.html"

static tusb_desc_webusb_url_t desc_url =
{
  .bLength         = 3 + sizeof(URL) - 1,
  .bDescriptorType = 3, // WEBUSB URL type
  .bScheme         = 1, // 0: http, 1: https
  .url             = URL
};


uint8_t const desc_ms_os_20[] =
{
  // Set header: length, type, windows version, total length
  U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

  // Configuration subset header: length, type, configuration index, reserved, configuration total length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A),

  // Function Subset header: length, type, first interface, reserved, subset length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_VENDOR, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08),

  // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
  U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

  // MS OS 2.0 Registry property descriptor: length, type
  U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08-0x08-0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
  'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
  'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
  U16_TO_U8S_LE(0x0050), // wPropertyDataLength
	//bPropertyData: “{975F44D9-0D08-43FD-8B3E-127CA8AFFF9D}”.
  '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
  '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
  '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
  '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};







// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool USB::tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case VENDOR_REQUEST_WEBUSB:
            Log("VENDOR_REQUEST_WEBUSB");
          // match vendor request in BOS descriptor
          // Get landing page url
          return tud_control_xfer(rhport, request, (void*)(uintptr_t) &desc_url, desc_url.bLength);

        case VENDOR_REQUEST_MICROSOFT:
            Log("VENDOR_REQUEST_MICROSOFT ", request->wIndex);
          if ( request->wIndex == 7 )
          {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20+8, 2);

            return tud_control_xfer(rhport, request, (void*)(uintptr_t) desc_ms_os_20, total_len);
          }else
          {
            return false;
          }

        default: break;
      }
    break;

    case TUSB_REQ_TYPE_CLASS:
    Log("TUSB_REQ_TYPE_CLASS ", request->bRequest);
      if (request->bRequest == 0x22)
      {
        // // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
        // web_serial_connected = (request->wValue != 0);

        // // Always lit LED if connected
        // if ( web_serial_connected )
        // {
        //   board_led_write(true);
        //   blink_interval_ms = BLINK_ALWAYS_ON;

        //   tud_vendor_write_str("\r\nWebUSB interface connected\r\n");
        //   tud_vendor_write_flush();
        // }else
        // {
        //   blink_interval_ms = BLINK_MOUNTED;
        // }

        // response with status OK
        return tud_control_status(rhport, request);
      }
    break;

    default: break;
  }

  // stall unknown request
  return false;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    return USB::tud_vendor_control_xfer_cb(rhport, stage, request);
}

#endif










/////////////////////////////////////////////////////////////////////
// USB State Callbacks
/////////////////////////////////////////////////////////////////////

// Invoked when device is mounted
void tud_mount_cb(void)
{
    Log("USB Mounted");
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



}   // extern "C"


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

























