// CInGame::Start (main app, SimTheme Park 2.04b3, InGame.cpp; unit w75).
#include "Colours.h"
#include "InGame.h"
#include "LbPalette.h"

namespace {

// One colour of a ramp from palette entry `index`: the Mac builds the four bytes on the stack (0xff, then the entry's first three
// bytes, which are red, green and blue) and stores them with one stw at the colour's address (unaligned), then stores the index
// byte after them.
void SetFromPalette(TbColour& colour, uint8_t index) {
    const uint8_t* entry = reinterpret_cast<const uint8_t*>(&_lbGlobalPalette) + index * 4;
    colour.mAlpha = 0xff;
    colour.mRed = entry[0];
    colour.mGreen = entry[1];
    colour.mBlue = entry[2];
    colour.mIndex = index;
}

}  // namespace

// 0x1014b304 (676 bytes). Eight colours of gColours are set from _lbGlobalPalette: the yellow ramp's colours 8, 6, 4 and 2 (Mac
// offsets 0xd0, 0xc6, 0xbc, 0xb2) from the entries 0x85, 0x87, 0x89 and 0x8b, and the green ramp's colours 8, 6, 4 and 2 (Mac offsets
// 0x60, 0x56, 0x4c, 0x42) from the entries 0xa7, 0xa9, 0xab and 0xad. Nothing else is touched (the ramps' selected colours are left).
void CInGame::Start() {
    SetFromPalette(gColours.mYellow.mColours[8], 0x85);
    SetFromPalette(gColours.mYellow.mColours[6], 0x87);
    SetFromPalette(gColours.mYellow.mColours[4], 0x89);
    SetFromPalette(gColours.mYellow.mColours[2], 0x8b);
    SetFromPalette(gColours.mGreen.mColours[8], 0xa7);
    SetFromPalette(gColours.mGreen.mColours[6], 0xa9);
    SetFromPalette(gColours.mGreen.mColours[4], 0xab);
    SetFromPalette(gColours.mGreen.mColours[2], 0xad);
}
