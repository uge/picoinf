#include "USB.h"

#include "tusb.h"


/////////////////////////////////////////////////////////////////////
// Device Descriptors
/////////////////////////////////////////////////////////////////////




static tusb_desc_device_t desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be
    // common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0,
    .idProduct          = 0,
    .bcdDevice          = 0,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
// Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
uint8_t const *USB::tud_descriptor_device_cb()
{
    // Log("Device Descriptor");

    desc_device.idVendor = vid_;
    desc_device.idProduct = pid_;
    desc_device.bcdDevice = device_;

    return (uint8_t const *)&desc_device;
}

extern "C"
{
uint8_t const *tud_descriptor_device_cb(void)
{
    return USB::tud_descriptor_device_cb();
}
}