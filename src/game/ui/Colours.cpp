// Colours.cpp: name proven by __sinit_Colours_cpp in the main application.

#include "Colours.h"
#include "LbPalette.h"

CColours gColours; // __sinit_Colours_cpp at 0x1007d8ac, object at 0x102dc08c

CColours::CColours() : mRed(5), mGreen(5), mBlue(5), mYellow(5), mWhite(9),
                        mBlack(&mWhite.mColours[0]) {
    Calibrate();
}

void CColours::Calibrate() {
    const TbPalette* palette = &_lbGlobalPalette;
    for (int i = 0; i < 10; ++i) {
        uint8_t level = static_cast<uint8_t>(i * 255 / 9);
        auto set = [palette](TbColour& colour, uint8_t red, uint8_t green, uint8_t blue) {
            uint8_t index = LbPalette_FindColour(palette, red, green, blue);
            colour.mAlpha = 255;
            colour.mRed = red;
            colour.mGreen = green;
            colour.mBlue = blue;
            colour.mIndex = index;
        };
        set(mRed.mColours[i], level, 0, 0);
        set(mGreen.mColours[i], 0, level, 0);
        set(mBlue.mColours[i], 0, 0, level);
        set(mYellow.mColours[i], level, level, 0);
        set(mWhite.mColours[i], level, level, level);
    }
    *mRed.mSelected = mRed.mColours[5];
    *mGreen.mSelected = mGreen.mColours[5];
    *mBlue.mSelected = mBlue.mColours[5];
    *mYellow.mSelected = mYellow.mColours[5];
    *mWhite.mSelected = mWhite.mColours[9];
    *mBlack = mWhite.mColours[0];
}
