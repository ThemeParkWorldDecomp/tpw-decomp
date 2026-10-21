// Test library for the ride hoardings (unit v5987; libtpw_unit_SetRideHoardingLevel.dylib, tests/test_SetRideHoardingLevel.py), main app:
//   src/game/park/TPHoardings.cpp   SetRideHoardingLevel, EnableRideHoardings, ActivateHoardings, ReactivateHoardings
//
// The callees outside the unit are recording fakes; gTPGlobals (another file's) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "rsse.h"
#include "SaveLoad.h"

using namespace calllog;

// gTPGlobals belongs to another file and its constructors are not linked here: raw zeroed storage under the same symbol.
alignas(16) char gTPGlobalsStorage[sizeof(TPGlobals)] asm("_gTPGlobals");

namespace {
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
int64_t Pack(HFieldXZ h) { return (static_cast<int64_t>(h.mF00) << 16) | h.mF02; }
}  // namespace

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {lab(matrix)});
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) matrix->m[i][j] = i == j ? 1.0f : 0.0f;
}

void YRotateLMatrixNoTrans(sMatrix* matrix, float c, float s) { ev("YRotate", {lab(matrix), CanonBits(c), CanonBits(s)}); }

int32_t DMEnableMeshRender(tag_sDMeshSet* set, HFieldXZ from, HFieldXZ to) {
    ev("DMEnable", {lab(set), Pack(from), Pack(to)});
    return 1;
}

int32_t DMDisableMeshRender(tag_sDMeshSet* set, HFieldXZ from, HFieldXZ to) {
    ev("DMDisable", {lab(set), Pack(from), Pack(to)});
    return 1;
}

extern "C" {

uint64_t tpw_hoard_addr(const char* name) {
    if (std::strcmp(name, "gTPGlobals") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gTPGlobals));
    return 0;
}

uint64_t tpw_hoard_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_hoard_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_hoard_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(tag_sRideAnimSet, mFlags);
    out[n++] = offsetof(tag_sRideAnimSet, mHeads);
    out[n++] = offsetof(tag_sRideAnimSet, mHoardingMesh);
    out[n++] = offsetof(tag_sRideAnimSet, mDynamicMeshSet);
    out[n++] = offsetof(tag_sRideAnimSet, mHoardingLevel);
    out[n++] = offsetof(tag_sRideAnimSet, mHoardingSpeed);
    out[n++] = offsetof(tag_sRideAnimSet, mFC0);
    out[n++] = offsetof(tag_sRideAnimSet, mFC4);
    out[n++] = sizeof(tag_sRideAnimSet);
    out[n++] = offsetof(tag_sMesh, mHeader);
    out[n++] = offsetof(tag_sMesh, mF10);
    out[n++] = sizeof(tag_sMesh);
    out[n++] = offsetof(tag_sDMeshSet, mF00);
    out[n++] = offsetof(tag_sDMeshSet, mEntries);
    out[n++] = offsetof(tag_sDMeshSet, mMesh);
    out[n++] = offsetof(tag_sDMeshSet, mVertexSets);
    out[n++] = sizeof(tag_sDMeshSet);
    out[n++] = offsetof(sDMeshEntry, mMatrix);
    out[n++] = offsetof(sDMeshEntry, mFaceSets);
    out[n++] = sizeof(sDMeshEntry);
    out[n++] = offsetof(TPGlobals, mSinTableMask);
    out[n++] = offsetof(TPGlobals, mSinTableScale);
    out[n++] = offsetof(TPGlobals, mSinTable);
    return n;
}

void tpw_hoard_call(int32_t which, uint64_t set, uint32_t arg, float level) {
    tag_sRideAnimSet* p = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set));
    switch (which) {
        case 0: SetRideHoardingLevel(p, level); break;
        case 1: EnableRideHoardings(p, static_cast<int32_t>(arg)); break;
        case 2: ActivateHoardings(p, arg); break;
        default: ReactivateHoardings(p); break;
    }
}

}  // extern "C"
