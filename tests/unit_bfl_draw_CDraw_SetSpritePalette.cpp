// Test library for bfl::draw::CDraw::SetSpritePalette (unit v5336; libtpw_unit_bfl_draw_CDraw_SetSpritePalette.dylib), main app:
//   src/bullfrog/Draw.cpp
//
// TbMaskRotates::Init is a fake: it records both formats and writes the rotates from the case's queue "rot".
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "DrawState.h"
#include "DrawRenderClasses.h"
#include "TbPixFmt.h"

using namespace calllog;

void bfl::draw::TbMaskRotates::Init(const TbPixFormat& source, const TbPixFormat& destination) {
    ev("TbMaskRotates::Init", {source.mRedMask, source.mGreenMask, source.mBlueMask, source.mAlphaMask, source.mBitsPerPixel,
                               source.mPalettised, destination.mRedMask, destination.mGreenMask, destination.mBlueMask,
                               destination.mAlphaMask, destination.mBitsPerPixel, destination.mPalettised});
    for (int i = 0; i < 4; ++i) mRotate[i] = static_cast<uint32_t>(ret("rot"));
}

extern "C" {

uint64_t tpw_sprpal_addr(const char*) { return 0; }

uint64_t tpw_sprpal_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sprpal_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(bfl::draw::CDraw, mCurrent));
    out[n++] = static_cast<int64_t>(sizeof(bfl::draw::CDraw));
    out[n++] = static_cast<int64_t>(offsetof(bfl::draw::CDraw, mF68));
    return n;
}

void tpw_sprpal_set(uint64_t self, uint64_t palette, uint64_t format) {
    reinterpret_cast<bfl::draw::CDraw*>(self)->SetSpritePalette(*reinterpret_cast<const TbPalette*>(palette),
                                                                reinterpret_cast<const TbPixFormat*>(format));
}

}  // extern "C"
