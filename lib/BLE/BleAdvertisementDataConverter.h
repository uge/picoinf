#pragma once

#include <stdint.h>

#include "BleAdvertisement.h"

#include <string>
#include <vector>
using namespace std;


// maybe look at this at some point
// https://github.com/google/eddystone


class BleAdvertisement;

class BleAdvertisementDataConverter
{
public:
    static vector<uint8_t> Convert(BleAdvertisement &bleAdvert, string type);
};
