#include "BleAdvertisement.h"

#include "StrictMode.h"


static vector<uint8_t> byteList;

const vector<uint8_t> &BleAdvertisement::GetRawAdvertisingDataStructure(string type)
{
    byteList = BleAdvertisementDataConverter::Convert(*this, type);

    return byteList;
}