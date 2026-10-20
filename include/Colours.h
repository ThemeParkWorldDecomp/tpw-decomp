// Colours.cpp, main app. Mac: five 0x38-byte ramps, followed by mBlack at +0x118.
#pragma once
#include "TbColour.h"

struct CColourRamp {
    explicit CColourRamp(int selected) : mSelected(&mColours[selected]) {}
    TbColour mColours[10]; // five bytes per colour
    uint8_t mPadding[2];   // original two untouched bytes before the pointer
    TbColour* mSelected;
};

class CColours {
public:
    CColours();           // inline construction in __sinit_Colours_cpp, 0x1007d8ac
    void Calibrate();     // 0x1007d66c
    CColourRamp mRed, mGreen, mBlue, mYellow, mWhite;
    TbColour* mBlack;     // 0x118
};
extern CColours gColours; // 0x102dc08c
