#include <cstdlib>
#include <cstring>
#include <new>
#include "Colours.h"
#include "LbPalette.h"
#include "batch3_test_support.h"

namespace {
CColourRamp* ramp(CColours* c, int i) {
    CColourRamp* ramps[] = {&c->mRed, &c->mGreen, &c->mBlue, &c->mYellow, &c->mWhite};
    return ramps[i];
}
TbColour** selected(CColours* c, int i) { return i == 5 ? &c->mBlack : &ramp(c, i)->mSelected; }
}
extern "C" {
void* tpw_Colours_global() { return &gColours; }
void* tpw_Colours_palette() { return &_lbGlobalPalette; }
void* tpw_Colours_create() { return std::calloc(1, sizeof(CColours)); }
void tpw_Colours_release(void* c) { std::free(c); }
void* tpw_Colours_colour(CColours* c, int i) { return &ramp(c, i / 10)->mColours[i % 10]; }
void tpw_Colours_seed(CColours* c, const uint8_t* data) {
    for (int i = 0; i < 5; ++i) {
        std::memcpy(ramp(c, i)->mColours, data + i * 52, 50);
        std::memcpy(ramp(c, i)->mPadding, data + i * 52 + 50, 2);
    }
}
void tpw_Colours_read(CColours* c, uint8_t* data) {
    for (int i = 0; i < 5; ++i) {
        std::memcpy(data + i * 52, ramp(c, i)->mColours, 50);
        std::memcpy(data + i * 52 + 50, ramp(c, i)->mPadding, 2);
    }
}
void tpw_Colours_set_pointer(CColours* c, int i, TbColour* p) { *selected(c, i) = p; }
void* tpw_Colours_pointer(CColours* c, int i) { return *selected(c, i); }
void tpw_Colours_Calibrate(CColours* c) { c->Calibrate(); }
void tpw_sinit_Colours() { new (&gColours) CColours; }
}
