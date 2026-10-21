// Test library for HoardingCentreAngle (unit v5646; libtpw_unit_HoardingCentreAngle.dylib), main app:
//   src/game/park/TPHoardings.cpp
// atan2 and fmod are the host's C library functions; no other callees and no globals.
#include <cstdint>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"

extern "C" {

float tpw_hca_call(int32_t x, int32_t y, int32_t side, float w, float h) { return HoardingCentreAngle(x, y, side, w, h); }

}  // extern "C"
