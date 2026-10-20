// IntensifyPix<CTraits<N>> from 3DFont.cpp (anonymous namespace on the Mac), four instances of one template.
#include <cmath>
#include <cstdint>

#include "Light.h"
#include "TbGraphicFile.h"

namespace tpw_intensify {

template <int N>
void IntensifyPix(const CTraits<N>&, uint8_t* pix, double alpha, uint32_t colour, uint32_t mask1, uint32_t mask2,
                  uint32_t mask3, uint32_t keepMask, int32_t x, int32_t y, const TbRect& rect) {
    bool inX = false, inXY = false, inside = false;
    alpha = std::fabs(alpha);
    if (x >= rect.left && x < rect.right) inX = true;
    if (inX && y >= rect.top) inXY = true;
    if (inXY && y < rect.bottom) inside = true;
    if (!inside || !(alpha <= 1.0)) return;
    uint32_t src = *reinterpret_cast<uint32_t*>(pix);
    uint32_t w = static_cast<uint32_t>(static_cast<int32_t>(255.0 * (1.0 - alpha))) & 0xff;
    uint32_t inv = 255u - w;
    uint32_t v3 = inv * (src & mask3) + w * (colour & mask3);
    uint32_t v2 = inv * (src & mask2) + w * (colour & mask2);
    uint32_t v1 = inv * (src & mask1) + w * (colour & mask1);
    uint32_t out = (src & keepMask) | (mask1 & ((v1 + (v1 >> 8) + 1) >> 8)) | (mask2 & ((v2 + (v2 >> 8) + 1) >> 8)) |
                   (mask3 & ((v3 + (v3 >> 8) + 1) >> 8));
    for (int i = 0; i < N; i++) {
        *pix++ = static_cast<uint8_t>(out >> ((i & 7) << 3));
    }
}

template void IntensifyPix<1>(const CTraits<1>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);
template void IntensifyPix<2>(const CTraits<2>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);
template void IntensifyPix<3>(const CTraits<3>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);
template void IntensifyPix<4>(const CTraits<4>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);

}  // namespace tpw_intensify
