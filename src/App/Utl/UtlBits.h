#pragma once

#include <cstdint>


// meant to extract a range of bits from a bitfield and shift them to
// all the way to the right.
// high and low bit are inclusive
extern uint16_t BitsGet(uint16_t val, uint8_t highBit, uint8_t lowBit);
extern uint16_t BitsPut(uint16_t val, uint8_t highBit, uint8_t lowBit);
