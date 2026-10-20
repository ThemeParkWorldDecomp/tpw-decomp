// Test library for LbPalette_GenerateGhostTable (unit v5650), fakes for its five callees.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbPalette.h"
#include "LbFile.h"

using namespace calllog;

namespace {
uint64_t gAbuf, gPre, gPal;
int64_t gCount, gAcc;

int64_t Lab(const void* p) {
    uint64_t v = reinterpret_cast<uintptr_t>(p);
    if (v == 0) return 0;
    if (v == gAbuf) return 1;
    if (v == gPre) return 2;
    if (v == gPal) return 3;
    return 4;
}
int64_t NameHash(const char* s) {
    uint32_t h = 7;
    for (; *s; s++) h = h * 31 + static_cast<uint8_t>(*s);
    return h;
}
}  // namespace

void* LbMem_Alloc(uint32_t size) {
    ev("Alloc", {size});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(gAbuf));
}
TbError LbFile_Load(const char* name, void* buffer, uint32_t capacity, int32_t* length) {
    ev("Load", {NameHash(name), Lab(buffer), capacity, length != nullptr});
    return TbError(static_cast<int32_t>(ret("load")));
}
TbError LbFile_Save(const char* name, void* data, uint32_t size) {
    ev("Save", {NameHash(name), Lab(data), size});
    return TbError(static_cast<int32_t>(ret("save")));
}
void LbMem_Free(void* block) { ev("Free", {Lab(block)}); }
uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t r, uint8_t g, uint8_t b) {
    gCount++;
    gAcc = static_cast<uint32_t>(gAcc * 31 + ((uint32_t(r) << 16) | (uint32_t(g) << 8) | b) + 1 + Lab(palette));
    return static_cast<uint8_t>(r * 131 + g * 17 + b * 53 + (r ^ g));
}

extern "C" {
uint64_t tpw_gg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
int32_t tpw_gg_layout(int64_t* out) { return 0; }
void tpw_gg_set(uint64_t abuf, uint64_t pre, uint64_t pal) {
    gAbuf = abuf;
    gPre = pre;
    gPal = pal;
    gCount = 0;
    gAcc = 0;
}
void tpw_gg_state(int64_t* out) {
    out[0] = gCount;
    out[1] = static_cast<uint32_t>(gAcc);
}
uint32_t tpw_gg_run(uint64_t slot, uint64_t pal, uint32_t percent, uint64_t name) {
    TbError e = LbPalette_GenerateGhostTable(reinterpret_cast<uint8_t**>(static_cast<uintptr_t>(slot)),
                                             reinterpret_cast<const TbPalette*>(static_cast<uintptr_t>(pal)), percent,
                                             reinterpret_cast<const char*>(static_cast<uintptr_t>(name)));
    return static_cast<uint32_t>(e.mValue);
}
}  // extern "C"
