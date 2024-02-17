#include "BleAdvertisement.h"


static vector<uint8_t> byteList;

const vector<uint8_t> &BleAdvertisement::GetRawAdvertisingDataStructure()
{
    byteList = BleAdvertisementDataConverter::Convert(*this);

    return byteList;
}