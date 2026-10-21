// Test library for StaticifyCreateOnlyAnimNodes (unit v5288; libtpw_unit_StaticifyCreateOnlyAnimNodes.dylib,
// tests/test_StaticifyCreateOnlyAnimNodes.py), main app:
//   src/game/rides/RideAnim.cpp   StaticifyCreateOnlyAnimNodes
//
// No callees. gei_System is defined here as raw storage under its symbol name (the real CSystem constructor is not linked);
// only its first word (mFlags) is used.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "m3d2.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
alignas(16) unsigned char gSystemStorage[0x400] __asm__("_gei_System");

extern "C" {

uint64_t tpw_sacn_addr(const char* name) {
    if (std::strcmp(name, "sys") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gSystemStorage));
    return 0;
}

uint64_t tpw_sacn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset a Ptr32 holds for a block address.
uint32_t tpw_sacn_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_sacn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mAllNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mMiniNodes));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMiniNode));
    return n;
}

void tpw_sacn_run(uint64_t set) { StaticifyCreateOnlyAnimNodes(reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set))); }

}  // extern "C"
