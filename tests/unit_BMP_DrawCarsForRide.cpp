// Test library for BMP_DrawCarsForRide (unit v5672; libtpw_unit_BMP_DrawCarsForRide.dylib, tests/test_BMP_DrawCarsForRide.py), main app:
//   src/BMP_DrawCarsForRide.cpp   BMP_DrawCarsForRide and its five data words (gDrawCars*)
//
// The callees outside the unit are recording fakes (GetMeshPtrAtHFieldCell, SetRideSpritePosnFocus, GetRideHeightAtPoint,
// SetLMatrixIdentityNoTrans, RotateLMatrixXYZNoTrans, PAR_InitEmitter); the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "LevelSystems.h"
#include "TPAdvisor.h"
#include "TPEngine.h"
#include "effect_manager.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tabCOLLIST gColList;
int32_t gHostSineTable[512];
tag_sRideAnimSet* gRideAnimSets[16];

namespace {

constexpr int kSets = 8;
tag_sRideAnimSet* gSet[kSets];   // registered by the test: the anim sets the fakes know by index

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t MatIdx(const void* m) {
    for (int i = 1; i < kSets; i++)
        if (gSet[i] != nullptr && &gSet[i]->mHeads->mHeader->mF78.get()->mMatrix == m) return i;
    return -1;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
tag_sRideAnimSet* GetMeshPtrAtHFieldCell(uint32_t x, uint32_t y) {
    ev("Mesh", {static_cast<int64_t>(x), static_cast<int64_t>(y)});
    int64_t idx = ret("mesh");
    return idx > 0 && idx < kSets ? gSet[idx] : nullptr;
}

void SetRideSpritePosnFocus(tag_sMesh* mesh) {
    int64_t found = -1;
    for (int i = 1; i < kSets; i++)
        if (gSet[i] != nullptr && gSet[i]->mHeads == mesh) found = i;
    ev("Focus", {found});
}

void GetRideHeightAtPoint(float x, float z, float& y) {
    ev("Height", {CanonBits(x), CanonBits(z)});
    uint32_t v = static_cast<uint32_t>(ret("height"));
    if (v != 1) std::memcpy(&y, &v, 4);
}

void SetLMatrixIdentityNoTrans(sMatrix* matrix) { ev("Identity", {MatIdx(matrix)}); }

void RotateLMatrixXYZNoTrans(sMatrix* matrix, eRotateParams axis, float* angle) {
    ev("Rotate", {MatIdx(matrix), static_cast<int64_t>(axis), CanonBits(angle[0]), CanonBits(angle[1]), CanonBits(angle[2])});
}

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("Emitter", {static_cast<int64_t>(type), static_cast<int64_t>(a), static_cast<int64_t>(b), static_cast<int64_t>(c)});
    return static_cast<uint32_t>(ret("emitter"));
}

extern "C" {

uint64_t tpw_dcr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gColList},
        {"sets", gRideAnimSets},
        {"sine", gHostSineTable},
        {"angles", gDrawCarsAngles},
        {"lo", &gDrawCarsClampLo},
        {"hi", &gDrawCarsClampHi},
        {"init", &gDrawCarsInit},
        {"scale", &gDrawCarsScale},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dcr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 form (offset from the arena base) of a host address.
uint32_t tpw_dcr_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

void tpw_dcr_register(uint32_t index, uint64_t address) {
    if (index < kSets) gSet[index] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(address));
}

int32_t tpw_dcr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSLOT));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    return n;
}

void tpw_dcr_run(int32_t ride, float step) { BMP_DrawCarsForRide(ride, step); }

}  // extern "C"
