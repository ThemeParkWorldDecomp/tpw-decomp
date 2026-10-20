// PARTIAL bullfrog palette API. Palette storage is opaque to this unit.
#pragma once
#include <cstdint>
struct TbPalette;
extern TbPalette _lbGlobalPalette;
uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green,
                            uint8_t blue); // bullfrog 0x1001a5c8

// Unit v5650
#include "TbError.h"
// Returns the error through a hidden pointer on the Mac. 0x100474cc, 556 B
TbError LbPalette_GenerateGhostTable(uint8_t** table, const TbPalette* palette, uint32_t percent,
                                     const char* name);
