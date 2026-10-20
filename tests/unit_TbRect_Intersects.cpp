// Test library for TbRect::Intersects (unit v5750) and TbRect::Normalise (unit v5L16).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbGraphicFile.h"

extern "C" {

uint64_t tpw_rect_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rect_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbRect, top));
    out[n++] = static_cast<int64_t>(offsetof(TbRect, right));
    out[n++] = static_cast<int64_t>(offsetof(TbRect, bottom));
    out[n++] = static_cast<int64_t>(sizeof(TbRect));
    return n;
}

uint32_t tpw_rect_intersects(uint64_t a, uint64_t b) {
    return reinterpret_cast<const TbRect*>(a)->Intersects(*reinterpret_cast<const TbRect*>(b)) ? 1u : 0u;
}

void tpw_rect_normalise(uint64_t a) { reinterpret_cast<TbRect*>(a)->Normalise(); }

}  // extern "C"
