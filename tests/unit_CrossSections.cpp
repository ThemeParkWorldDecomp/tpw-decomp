// Test library for the coaster cross-section builders (unit w147; libtpw_unit_CrossSections.dylib, tests/test_CrossSections.py), main app:
//   src/game/rides/CoasterType.cpp   BuildCrossSectionPoints and BuildCrossSectionEdges
//
// The allocator and the free are recording fakes (the same event names and arguments as the module's PowerPC stubs). A block carries
// kGuard bytes of 0xa5 after its end, so an overrun shows; the host's pointer table is twice the Mac's size (the module halves the
// second allocation of each call in the host log).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "CoasterBalance.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

namespace {
constexpr uint32_t kGuard = 512;
}

// ---- the fakes
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("ReleaseAlloc", {size, flags});
    uint8_t* block = static_cast<uint8_t*>(std::malloc(size + kGuard));
    std::memset(block, 0, size);
    std::memset(block + size, 0xa5, kGuard);
    newlab(block);
    return block;
}

void* MReleaseFreeF(void* block) {
    ev("ReleaseFree", {lab(block)});
    return nullptr;
}

extern "C" {

// A zeroed block the module fills with Mac-offset words.
uint64_t tpw_xs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_xs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sCoasterRecord));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mF00));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mPointCount));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mPoints));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mPointsB));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mF14));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF40));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF90));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF94));
    out[n++] = static_cast<int64_t>(sizeof(CCoasterBalance));
    out[n++] = static_cast<int64_t>(offsetof(CCoasterBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(sCrossSectionPoint));
    out[n++] = static_cast<int64_t>(sizeof(sCrossSectionEdge));
    return n;
}

int32_t tpw_xs_points(uint64_t type, uint64_t records, uint64_t balance, uint64_t out) {
    return BuildCrossSectionPoints(reinterpret_cast<tag_sCoasterType*>(type), reinterpret_cast<sCoasterRecord*>(records),
                                   reinterpret_cast<CCoasterBalance*>(balance), reinterpret_cast<int32_t*>(out));
}

int32_t tpw_xs_edges(uint64_t type, uint64_t records, uint64_t balance) {
    return BuildCrossSectionEdges(reinterpret_cast<tag_sCoasterType*>(type), reinterpret_cast<sCoasterRecord*>(records),
                                  reinterpret_cast<CCoasterBalance*>(balance));
}

}  // extern "C"
