// Test library for IntensifyPix<CTraits<N>> (unit v5003; libtpw_unit_unnamed_IntensifyPix.dylib), main app:
//   src/Light_unnamed_IntensifyPix.cpp   four template instances. No callees, no globals.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Light.h"
#include "TbGraphicFile.h"

using namespace tpw_intensify;

extern "C" {

uint64_t tpw_ip_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ip_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbRect, top));
    out[n++] = static_cast<int64_t>(offsetof(TbRect, right));
    out[n++] = static_cast<int64_t>(offsetof(TbRect, bottom));
    out[n++] = static_cast<int64_t>(sizeof(TbRect));
    return n;
}

#define WRAP(NAME, N)                                                                                                         \
    void NAME(uint64_t pix, double a, uint32_t c, uint32_t m1, uint32_t m2, uint32_t m3, uint32_t keep, int32_t x, int32_t y, \
              uint64_t rect) {                                                                                                \
        CTraits<N> t;                                                                                                         \
        IntensifyPix<N>(t, reinterpret_cast<uint8_t*>(pix), a, c, m1, m2, m3, keep, x, y, *reinterpret_cast<TbRect*>(rect)); \
    }
WRAP(tpw_ip_p4, 4)
WRAP(tpw_ip_p3, 3)
WRAP(tpw_ip_p2, 2)
WRAP(tpw_ip_p1, 1)

}  // extern "C"
