// Test library for LbMaths_ArcTan (unit v5657; libtpw_unit_LbMaths_ArcTan.dylib, tests/test_LbMaths_ArcTan.py).
// atan is a pure import; the host uses std::atan, the PowerPC side the harness MATH1 atan.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AudioPlaceHolder.h"

extern "C" {

double gLbMathsAtanScale = 0.0;  // 0x10074810, set at run time by the original; the test writes it on both sides

uint32_t tpw_lbarctan_a(int32_t x, int32_t y) { return static_cast<uint32_t>(bfl::maths::LbMaths_ArcTan(x, y)); }

}  // extern "C"
