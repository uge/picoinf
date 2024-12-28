#include "USB.h"

#include "tusb.h"


/////////////////////////////////////////////////////////////////////
// Configuration Descriptor
/////////////////////////////////////////////////////////////////////


#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

#define EPNUM_VENDOR_OUT  0x03
#define EPNUM_VENDOR_IN   0x83

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_VENDOR_DESC_LEN)


static uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

  // Interface number, string index, EP Out & IN address, EP size
  TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 5, EPNUM_VENDOR_OUT, EPNUM_VENDOR_IN, 64)
};

uint8_t const *USB::tud_descriptor_configuration_cb(uint8_t index)
{
    // Log("Config Descriptor");

    return desc_fs_configuration;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
extern "C"
{
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  return USB::tud_descriptor_configuration_cb(index);
}
}