#pragma once

#include "Adafruit_BME280.h"
#include "Adafruit_BMP280.h"
#include "Adafruit_MMC56x3.h"
#include "Adafruit_Si7021.h"
#include "BH1750FVI.h"

// This is the other side of the gross hack that got put into the
// shim Arduino layer.
#undef byte