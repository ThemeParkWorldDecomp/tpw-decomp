// Test entry point for LbDraw_GetClipRect (src/bfl_lbdraw_leaves.cpp, libtpw_unit_LbDrawGetClipRect.dylib; see
// tests/test_LbDrawGetClipRect.py). The library's other leaves are linked but not run; TbPixelCaps::Set, which
// LbDraw_SetCurrent calls, is a fake that is never reached.
#include <cstdint>

#include "LbDraw.h"

void TbPixelCaps::Set(const TbPixFormat&) {}

extern "C" {

// Sets the clip rectangle's four words to `rect`, calls LbDraw_GetClipRect and answers through out: the four words read through the
// returned pointer, 1 when the pointer is the clip rectangle itself, then writes `poke` through the pointer and answers the clip
// rectangle's first word afterwards (so that the pointer is shown to alias it).
void tpw_gc_get(const int32_t* rect, int32_t poke, int32_t* out) {
    _lbCurrentClipRect = {rect[0], rect[1], rect[2], rect[3]};
    TbRect* const clip = LbDraw_GetClipRect();
    out[0] = clip->left;
    out[1] = clip->top;
    out[2] = clip->right;
    out[3] = clip->bottom;
    out[4] = clip == &_lbCurrentClipRect ? 1 : 0;
    clip->left = poke;
    out[5] = _lbCurrentClipRect.left;
}

}  // extern "C"
