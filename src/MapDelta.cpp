// MapDelta::Rotate (main app, SimTheme Park 2.04b3, MapDelta.cpp; unit w139), 0x10103d88, 272 bytes, and the three compass offsets.
//
// The offset turned for a ride rotated by `rotation` degrees: r = rotation % 360 (C remainder: the sign of the dividend), then 0 gives
// (x, y); 180 or -180 gives (-x, -y); 90 or -270 gives (y, -x); 270 or -90 gives (-y, x); any other value asserts
// "MapDelta::Rotate - illegal angle" (the release assert does nothing) and answers (x, y). The Mac never writes the flag byte of the result
// (the result's third word is whatever was there); this version writes 0.
#include "ObjectParts.h"

#include <cstdint>

#include "Debug.h"

// __sinit_MapDelta_cpp 0x10103ec0 (436 bytes; the REPORT anchor of unit w146): the static construction of the eight compass offsets, in this order.
// The Mac writes the two words of each (x then y; the flag byte stays 0 in the BSS) and registers a destructor (0x102663d0) for it with
// __register_global_object, which links an 8-byte node after each object (so the objects lie 0x14 bytes apart). The host's constant aggregates
// need no construction and no destructor. Units w139 and w145 held these definitions before; they were moved here to stand in the __sinit order.
const MapDelta gMapDeltaMinusY = {0, -1, 0};        // Mac 0x1034c2ac
const MapDelta gMapDeltaPlusXMinusY = {1, -1, 0};   // Mac 0x1034c298
const MapDelta gMapDeltaPlusX = {1, 0, 0};          // Mac 0x1034c284
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};     // Mac 0x1034c270
const MapDelta gMapDeltaPlusY = {0, 1, 0};          // Mac 0x1034c25c
const MapDelta gMapDeltaMinusXPlusY = {-1, 1, 0};   // Mac 0x1034c248
const MapDelta gMapDeltaMinusX = {-1, 0, 0};        // Mac 0x1034c234
const MapDelta gMapDeltaMinusXMinusY = {-1, -1, 0}; // Mac 0x1034c220

namespace {
// neg: wraps for INT_MIN
int32_t Negate(int32_t v) { return static_cast<int32_t>(0u - static_cast<uint32_t>(v)); }
}  // namespace

MapDelta MapDelta::Rotate(int32_t rotation) const {
    const int32_t r = rotation % 360;
    MapDelta result = {0, 0, 0};
    if (r == 0) {
        result.mX = mX;
        result.mY = mY;
    } else if (r == 180 || r == -180) {
        result.mX = Negate(mX);
        result.mY = Negate(mY);
    } else if (r == 90 || r == -270) {
        result.mX = mY;
        result.mY = Negate(mX);
    } else if (r == 270 || r == -90) {
        result.mX = Negate(mY);
        result.mY = mX;
    } else {
        GAME_ASSERTMSG(0, "MapDelta::Rotate - illegal angle");
        result.mX = mX;
        result.mY = mY;
    }
    return result;
}
