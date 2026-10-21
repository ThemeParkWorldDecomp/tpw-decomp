// Test library for LbDraw.cpp's static initialiser (unit v5717; libtpw_unit_sinit_LbDraw_cpp.dylib, tests/test_sinit_LbDraw_cpp.py), main app:
//   src/bullfrog/LbDraw.cpp   __sinit_LbDraw_cpp
//
// No callees. The globals of LbDraw.cpp it writes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbDraw.h"

using namespace calllog;

TbRect _lbCurrentViewPort;
TbRect _lbCurrentClipRect;
TbPixelCaps _lbCurrentPixelCaps;
RenderContext _lbRenderContext8;
RenderContext _lbRenderContext16;
RenderContext _lbRenderContext24;
RenderContext _lbRenderContext32;

extern "C" {

uint64_t tpw_sl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"view", &_lbCurrentViewPort},
        {"clip", &_lbCurrentClipRect},
        {"caps", &_lbCurrentPixelCaps},
        {"ctx8", &_lbRenderContext8},
        {"ctx16", &_lbRenderContext16},
        {"ctx24", &_lbRenderContext24},
        {"ctx32", &_lbRenderContext32},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbRect));
    out[n++] = static_cast<int64_t>(sizeof(TbPixelCaps));
    out[n++] = static_cast<int64_t>(sizeof(RenderContext));
    return n;
}

void tpw_sl_run() { __sinit_LbDraw_cpp(); }

}  // extern "C"
