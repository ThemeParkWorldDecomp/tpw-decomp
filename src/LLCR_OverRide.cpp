// LLCR_OverRide 0x10089210 (140 bytes): answers the halfword of the world at +0x1da732 when (x, y) lies in the entrance A column range
// of the balance (x from EntranceAPosX - 1 to the slot after it, 0x584, plus 1) and y equals EntranceAPosY (+0x580); otherwise it asks the
// general routine at 0x1008905c with the addresses of x and y.
#include <cstdint>

#include "LLCellSystem.h"
#include "MainBalance.h"
#include "World.h"

void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y);   // 0x1008905c (untranslated stand-in)

void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    int32_t lo = gMainBalance.mValues[(0x57c - 8) / 4].mInteger - 1;
    int32_t hi = gMainBalance.mValues[(0x584 - 8) / 4].mInteger + 1;
    int32_t row = gMainBalance.mValues[(0x580 - 8) / 4].mInteger;
    if (x < lo || x > hi || y != row) {
        LLCR_OverRide(out, &x, &y);
    } else {
        *out = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(gGameWorld) + 0x1da732);
    }
}
