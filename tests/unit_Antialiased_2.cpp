// Test library for Antialiased<CTraits<__int128>> (unit v5559; libtpw_unit_Antialiased_2.dylib, tests/test_Antialiased_2.py), main app:
//   src/game/ui/3DFont.cpp   tpw_antialias::Antialiased<1>
//
// The callees outside the unit are recording fakes: CColour::GetFormattedValue (answers from the queue) and IntensifyPix<1>.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "DrawRenderClasses.h"
#include "Light.h"
#include "TbGraphicFile.h"
#include "ui_main.h"

using namespace calllog;

namespace {
uint8_t* gData = nullptr;   // the area's pixel base, set by the wrapper: pixel pointers are logged as offsets from it
}

namespace bfl {
namespace draw {
uint32_t CColour::GetFormattedValue(const TbPixFormat& format) const {
    const uint32_t* w = reinterpret_cast<const uint32_t*>(&format);
    const uint8_t* c = reinterpret_cast<const uint8_t*>(this);
    ev("GetFormattedValue", {w[0], w[1], w[2], w[3], w[4], w[5], c[0], c[1], c[2], c[3]});
    return static_cast<uint32_t>(ret("GetFormattedValue"));
}
}  // namespace draw
}  // namespace bfl

namespace tpw_intensify {
template <int N>
void IntensifyPix(const CTraits<N>&, uint8_t* pix, double alpha, uint32_t colour, uint32_t mask1, uint32_t mask2, uint32_t mask3,
                  uint32_t keepMask, int32_t x, int32_t y, const TbRect& rect) {
    uint64_t bits;
    std::memcpy(&bits, &alpha, 8);
    ev("IntensifyPix", {static_cast<int32_t>(pix - gData), static_cast<int32_t>(bits >> 32), static_cast<int32_t>(bits & 0xffffffffu),
                        static_cast<int32_t>(colour), static_cast<int32_t>(mask1), static_cast<int32_t>(mask2), static_cast<int32_t>(mask3),
                        static_cast<int32_t>(keepMask), x, y});
    ev("IntensifyRect", {rect.left, rect.top, rect.right, rect.bottom});
}
template void IntensifyPix<1>(const CTraits<1>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t,
                              const TbRect&);
}  // namespace tpw_intensify

extern "C" {

uint64_t tpw_aa_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_aa_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mData));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mStride));
    out[n++] = static_cast<int64_t>(sizeof(bfl::draw::CColour));
    return n;
}

void tpw_aa_run(uint64_t area, uint64_t clip, uint64_t colour, uint64_t from, uint64_t to) {
    const TbRenderArea& a = *reinterpret_cast<const TbRenderArea*>(static_cast<uintptr_t>(area));
    gData = a.mData;
    tpw_intensify::CTraits<1> traits;
    tpw_antialias::Antialiased<1>(traits, a, *reinterpret_cast<const TbRect*>(static_cast<uintptr_t>(clip)),
                                  *reinterpret_cast<const bfl::draw::CColour*>(static_cast<uintptr_t>(colour)),
                                  *reinterpret_cast<const TbPoint*>(static_cast<uintptr_t>(from)),
                                  *reinterpret_cast<const TbPoint*>(static_cast<uintptr_t>(to)));
}

}  // extern "C"
