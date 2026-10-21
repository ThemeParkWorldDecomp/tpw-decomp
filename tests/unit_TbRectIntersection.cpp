// Test entry point for TbRect::Intersection (src/bullfrog/LbRect.cpp, libtpw_unit_TbRectIntersection.dylib; see
// tests/test_TbRectIntersection.py). The function calls nothing.
#include <cstdint>

#include "TbGraphicFile.h"

extern "C" {

// out = a.Intersection(b), each a rectangle of four words.
void tpw_tr_intersection(const int32_t* a, const int32_t* b, int32_t* out) {
    const TbRect left = {a[0], a[1], a[2], a[3]};
    const TbRect right = {b[0], b[1], b[2], b[3]};
    const TbRect result = left.Intersection(right);
    out[0] = result.left;
    out[1] = result.top;
    out[2] = result.right;
    out[3] = result.bottom;
}

}  // extern "C"
