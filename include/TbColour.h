// PARTIAL bullfrog colour value. The constructor (main 0x1007d9d4, clears the five bytes) is in src/TbColour.cpp (unit w44); the
// unit tests that only need the type carry a weak placeholder (tests/shim_placeholders.cpp). Each channel is byte-addressed.
#pragma once
#include <cstdint>
struct TbColour {
    TbColour();
    uint8_t mAlpha, mRed, mGreen, mBlue, mIndex;
};
static_assert(sizeof(TbColour) == 5, "TbColour stride");
