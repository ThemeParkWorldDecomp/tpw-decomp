// Test library for SetupBraceBezierCtrlParams and GenerateTrack (unit v5992; libtpw_unit_SetupBraceBezierCtrlParams.dylib,
// tests/test_SetupBraceBezierCtrlParams.py), main app:
//   src/game/rides/CoasterTrack.cpp   both functions and the globals gBraceBezierMin/Max, gTrackTextureCacheType,
//   gTrackSubdivTimes
//
// The callees outside the unit are recording fakes; the globals of other files are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

// ---- globals of other files, defined here
void* gCoasterCurrentData;
tag_sCoasterInstance* gCoasterCurrent;
void* gExtrudeFaceStoreB;
sExtrusionParams* gExtrudeParamsTable;
sExtrusionParams gExtrudeParamsBuf[2560];
int32_t gExtrudeKindFlag;
int32_t gExtrudeWinding;
float gExtrudeTOffset;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b) {
    ev("Dist", {CanonBits(a->x), CanonBits(a->y), CanonBits(a->z), CanonBits(b->x), CanonBits(b->y), CanonBits(b->z)});
    uint32_t bits = static_cast<uint32_t>(ret("Dist"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
void RecalcBraceTangents() { ev("RecalcBraceTangents", {}); }
void GetActualBraceParams() { ev("GetActualBraceParams", {}); }
void LinearSampleTrack(tag_sCoasterInstance* instance) { ev("LinearSampleTrack", {lab(instance)}); }
void CalculateTrackGForcesSpeedAndExcitement(tag_sCoasterInstance* instance) { ev("CalcG", {lab(instance)}); }
void RecalcTrackBounds(tag_sCoasterInstance* instance) { ev("RecalcTrackBounds", {lab(instance)}); }
void DoCollisionChecking(tag_sCoasterInstance* instance) { ev("DoCollisionChecking", {lab(instance)}); }
void DestroyDynamicMeshSetInstance(tag_sDMeshSet* set, sHeapInfo* heap, uint32_t flags) {
    ev("Destroy", {lab(set), lab(heap), static_cast<int64_t>(flags)});
}
void GenerateExtrusions() { ev("GenerateExtrusions", {}); }
int32_t GenerateTrackSubdivisions(tag_sTrackBrace* a, tag_sTrackBrace* b, sExtrusionParams* params, float* out, float span) {
    ev("Subdiv", {lab(a), lab(b), lab(params), lab(out), CanonBits(span)});
    return static_cast<int32_t>(ret("Subdiv"));
}
void ExtrudeTrackSection(tag_sTrackBrace* a, tag_sTrackBrace* b, int32_t count, float* ts, int32_t force) {
    ev("Extrude", {lab(a), lab(b), static_cast<int64_t>(count), lab(ts), static_cast<int64_t>(force)});
}
void GetNewTrackMeshSet(int32_t flag) { ev("GetNewTrackMeshSet", {static_cast<int64_t>(flag)}); }

extern "C" {

uint64_t tpw_sbbc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCoasterCurrent", &gCoasterCurrent},
        {"gCoasterCurrentData", &gCoasterCurrentData},
        {"gStoreB", &gExtrudeFaceStoreB},
        {"gCache", &gTrackTextureCacheType},
        {"gTimes", gTrackSubdivTimes},
        {"gParamsTable", &gExtrudeParamsTable},
        {"gParamsBuf", gExtrudeParamsBuf},
        {"gKind", &gExtrudeKindFlag},
        {"gWinding", &gExtrudeWinding},
        {"gTOff", &gExtrudeTOffset},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sbbc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets and sizes, in the order of the module's NAMES.
int32_t tpw_sbbc_layout(int64_t* out) {
    int32_t n = 0;
#define O(T, m) out[n++] = static_cast<int64_t>(offsetof(T, m))
#define SZ(T) out[n++] = static_cast<int64_t>(sizeof(T))
    O(tag_sCoasterInstance, mFlags);
    O(tag_sCoasterInstance, mType);
    O(tag_sCoasterInstance, mF38);
    O(tag_sCoasterInstance, mBlock3C);
    O(tag_sCoasterInstance, mLastBrace);
    O(tag_sCoasterInstance, mF70);
    O(tag_sCoasterInstance, mF74);
    O(tag_sCoasterInstance, mF78);
    O(tag_sCoasterInstance, mF7C);
    O(tag_sCoasterInstance, mF80);
    O(tag_sCoasterInstance, mF84);
    O(tag_sCoasterInstance, mF88);
    O(tag_sCoasterInstance, mF8C);
    O(tag_sCoasterInstance, mF90);
    O(tag_sCoasterInstance, mF94);
    SZ(tag_sCoasterInstance);
    O(tag_sTrackBrace, mFlags);
    O(tag_sTrackBrace, mX4C);
    O(tag_sTrackBrace, mPrev);
    O(tag_sTrackBrace, mNext);
    O(tag_sTrackBrace, mF74);
    O(tag_sTrackBrace, mF90);
    O(tag_sTrackBrace, mData);
    SZ(tag_sTrackBrace);
    O(sTrackBraceData, mFlags);
    SZ(sTrackBraceData);
    O(sBraceSpline, mCount);
    O(sBraceSpline, mControlsA);
    O(sBraceSpline, mControlsB);
    SZ(sBraceSpline);
    O(tag_sBezierControl, mF00);
    O(tag_sBezierControl, mF08);
    O(tag_sBezierControl, mF0C);
    O(tag_sBezierControl, mBrace);
    O(tag_sBezierControl, mNext);
    O(tag_sBezierControl, mF40);
    SZ(tag_sBezierControl);
    O(tag_sCoasterType, mTextures);
    SZ(tag_sCoasterType);
    O(sDTextureSet, mCount);
    O(sDTextureSet, mEntries);
    SZ(sDTextureSet);
    O(sDTextureEntry, mFlags);
    SZ(sDTextureEntry);
    O(tag_sDMeshSet, mF38);
    SZ(tag_sDMeshSet);
    return n;
}

// 1 when the unit is built with the port fixes on (unit coaster64): SetupBraceBezierCtrlParams then leaves each control's mBrace (the
// Mac's previous control word) holding its brace.
int32_t tpw_sbbc_portfix() {
    return 0;
}

void tpw_sbbc_setup(uint64_t instance) { SetupBraceBezierCtrlParams(reinterpret_cast<tag_sCoasterInstance*>(instance)); }
void tpw_sbbc_gen(uint64_t instance) { GenerateTrack(reinterpret_cast<tag_sCoasterInstance*>(instance)); }

}  // extern "C"
