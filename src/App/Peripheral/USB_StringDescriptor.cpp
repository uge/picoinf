#include "USB.h"

#include "tusb.h"

using namespace std;

#include "StrictMode.h"


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

    // Log("String Descriptor ", index);

    uint16_t const *retVal = nullptr;
    
    byteList.clear();

    if (index == 0)
    {
        // 0 is supported language is English (0x0409)
        // put it in little-endian

        byteList.resize(2 + 2);
        byteList[2] = 0x09;
        byteList[3] = 0x04;

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

            // for (int i = 0; auto c : str)
            for (size_t i = 0; auto c : str)
            {
                byteList[2 + (i * 2)] = c;

                ++i;
            }

            retVal = (uint16_t const *)byteList.data();
        }
        else
        {
            // https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
            if (index == 0xEE)
            {
                // do nothing, let null be returned
            }
        }
    }

    if (retVal)
    {
        byteList[0] = (uint8_t)byteList.size();
        byteList[1] = TUSB_DESC_STRING;
    }

    return retVal;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
extern "C"
{
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    return USB::tud_descriptor_string_cb(index, langid);
}
}

