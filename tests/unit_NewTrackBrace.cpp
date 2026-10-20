// Test library for the track editor's new brace and the preview brace's undoing (unit v5157; libtpw_unit_NewTrackBrace.dylib,
// tests/test_NewTrackBrace.py), main app:
//   src/NewTrackBrace.cpp   NewTrackBrace, UnLinkPreviewBrace
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The globals of other
// files the unit reads (gCoasterEditInstance, gLinkFoundPylon, gPylonStackSkip, gRideAnimSets) and the two statics of
// GetTempFixedRideParams (which the original inlines; the real function is copied here, its statics are the Mac's 0x102b5bf8 and 0x102b5c08)
// are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
tag_sCoasterInstance* gCoasterEditInstance;
sPylonRecord* gLinkFoundPylon;
uint32_t gPylonStackSkip;
tag_sRideAnimSet* gRideAnimSets[16];

namespace {

int32_t sPair[2];             // 0x102b5c08
sFixedRideParams sParams;     // 0x102b5bf8
void* gAllocs[2];
int gAllocCount;

int64_t I(int64_t v) { return v; }

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

void RecordParams(const sFixedRideParams* p) {
    if (p == nullptr) {
        ev("RideParams", {0, 0, 0, 0, 0, 0});
        return;
    }
    int64_t pair = p->mF0C == nullptr ? 0 : (p->mF0C == static_cast<void*>(sPair) ? 1 : 2);
    ev("RideParams", {I(p->mX), I(p->mZ), I(p->mRotation), pair, I(sPair[0]), I(sPair[1])});
}

}  // namespace

// 0x1003d5b0: the real function (src/TPCoaster_loaders.cpp is not compiled here)
sFixedRideParams* GetTempFixedRideParams(int32_t a, int32_t b, int32_t x, int32_t z) {
    sPair[0] = a;
    sPair[1] = b;
    sParams.mX = x;
    sParams.mZ = z;
    sParams.mRotation = 0;
    sParams.mF0C = sPair;
    return &sParams;
}

// ---- the fakes
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    void* p = gAllocCount < 2 ? gAllocs[gAllocCount] : nullptr;
    gAllocCount++;
    ev("MReleaseAllocF", {I(static_cast<int32_t>(size)), I(static_cast<int32_t>(flags)), lab(p)});
    return p;
}

void CorrectBraceNumbering(tag_sTrackBrace* brace) { ev("CorrectBraceNumbering", {lab(brace)}); }

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e, uint32_t f,
                      int32_t g) {
    ev("InstanceRide", {I(static_cast<int32_t>(type)), I(static_cast<int32_t>(flags)), I(static_cast<int32_t>(a)), I(b), lab(c), I(d), I(e),
                        I(static_cast<int32_t>(f)), I(g)});
    RecordParams(params);
    return static_cast<uint32_t>(ret("InstanceRide"));
}

uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d, int32_t e,
                             int32_t f) {
    ev("RestoreRideInstance", {I(static_cast<int32_t>(instance)), I(static_cast<int32_t>(type)), I(static_cast<int32_t>(flags)),
                               I(static_cast<int32_t>(a)), lab(c), I(d), I(e), I(f)});
    RecordParams(params);
    return static_cast<uint32_t>(ret("RestoreRideInstance"));
}

void SetNewAnim(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control, uint32_t flags, float f) {
    int64_t at = (reinterpret_cast<char*>(control) - static_cast<char*>(set->mTimeControls)) / static_cast<int64_t>(sizeof(sAnimTimeControl));
    ev("SetNewAnim", {lab(set), I(static_cast<int32_t>(kind)), I(static_cast<int32_t>(index)), at, I(static_cast<int32_t>(flags)), CanonBits(f)});
}

void SetupBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info, sBraceControl* control, sPylonControl* pylon) {
    char* b = reinterpret_cast<char*>(brace);
    char* i = reinterpret_cast<char*>(info);
    int64_t rel = i == reinterpret_cast<char*>(&brace->mCount78) ? 0x78 : (i == reinterpret_cast<char*>(&brace->mCount84) ? 0x84 : i - b);
    ev("SetupBraceInfo", {lab(brace), rel, static_cast<int64_t>(reinterpret_cast<char*>(control) - reinterpret_cast<char*>(pylon)), lab(pylon)});
}

void Assign3DObjectToAIWorld(void* instance, uint32_t a, uint32_t b, uint32_t c, int32_t d) {
    ev("Assign3DObjectToAIWorld", {lab(instance), I(static_cast<int32_t>(a)), I(static_cast<int32_t>(b)), I(static_cast<int32_t>(c)), I(d)});
}

void InsertBraceIntoPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sLinkPylon* pylon, uint32_t level, float height, float* out,
                          int32_t a, int32_t b, int32_t c, int32_t d) {
    ev("InsertBraceIntoPylon", {lab(instance), lab(brace), lab(pylon), I(static_cast<int32_t>(level)), CanonBits(height), lab(out), I(a), I(b), I(c), I(d)});
}

void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace) { ev("FlagCollisionUpdateTrackAroundBrace", {lab(brace)}); }

void RemoveBraceFromCollisionSystem(tag_sTrackBrace* brace, tag_sCoasterInstance* instance) {
    ev("RemoveBraceFromCollisionSystem", {lab(brace), lab(instance)});
}

void UnLinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params) {
    ev("UnLinkBrace", {lab(brace), lab(reinterpret_cast<char*>(params) - offsetof(tag_sCoasterInstance, mBlock3C))});
}

void RestorePylonDetails(tag_sCoasterInstance* instance, tag_sPylon* pylon) { ev("RestorePylonDetails", {lab(instance), lab(pylon)}); }

void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b) {
    ev("UpdatePylonStackMeshes", {lab(control), lab(pylon), I(a), I(b)});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ntb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"pair", sPair},
        {"params", &sParams},
        {"edit", &gCoasterEditInstance},
        {"found", &gLinkFoundPylon},
        {"skip", &gPylonStackSkip},
        {"rides", gRideAnimSets},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_ntb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The two blocks MReleaseAllocF hands out, in call order.
void tpw_ntb_set_alloc(uint64_t first, uint64_t second) {
    gAllocs[0] = reinterpret_cast<void*>(static_cast<uintptr_t>(first));
    gAllocs[1] = reinterpret_cast<void*>(static_cast<uintptr_t>(second));
    gAllocCount = 0;
}

// The host offsets and sizes of every field the module reads or writes by name, in the order of the module's ALL table.
int32_t tpw_ntb_layout(int64_t* out) {
    int32_t n = 0;
#define OFF(type, member) out[n++] = static_cast<int64_t>(offsetof(type, member))
    OFF(tag_sTrackBrace, mNumber);
    OFF(tag_sTrackBrace, mFlags);
    OFF(tag_sTrackBrace, mX4C);
    OFF(tag_sTrackBrace, mL50);
    OFF(tag_sTrackBrace, mZ54);
    OFF(tag_sTrackBrace, mPrev);
    OFF(tag_sTrackBrace, mNext);
    OFF(tag_sTrackBrace, mF6C);
    OFF(tag_sTrackBrace, mWord70);
    OFF(tag_sTrackBrace, mF74);
    OFF(tag_sTrackBrace, mCount78);
    OFF(tag_sTrackBrace, mBlk7C);
    OFF(tag_sTrackBrace, mBlk80);
    OFF(tag_sTrackBrace, mCount84);
    OFF(tag_sTrackBrace, mBlk88);
    OFF(tag_sTrackBrace, mBlk8C);
    OFF(tag_sTrackBrace, mF90);
    OFF(tag_sTrackBrace, mData);
    OFF(tag_sTrackBrace, mFF8);
    OFF(tag_sTrackBrace, mFE8w);
    OFF(tag_sCoasterInstance, mFlags);
    OFF(tag_sCoasterInstance, mBlock3C);
    OFF(tag_sCoasterInstance, mF40);
    OFF(tag_sCoasterInstance, mBraceCount);
    OFF(tag_sCoasterInstance, mF48);
    OFF(tag_sCoasterInstance, mPylonTail);
    OFF(tag_sCoasterInstance, mLastBrace);
    OFF(tag_sCoasterInstance, mBraces);
    OFF(tag_sCoasterInstance, mBraceB5C);
    OFF(tag_sCoasterInstance, mPreviewBrace);
    OFF(tag_sCoasterInstance, mBrace64);
    OFF(tag_sCoasterInstance, mPreviewPylon);
    OFF(tag_sCoasterInstance, mBrace6C);
    OFF(tag_sCoasterInstance, mF118);
    OFF(tag_sCoasterInstance, mCObject);
    OFF(sPylonRecord, mNumber);
    OFF(sPylonRecord, mFlags);
    OFF(sPylonRecord, mCellX);
    OFF(sPylonRecord, mCellZ);
    OFF(sPylonRecord, mControl);
    OFF(sPylonRecord, mNext);
    OFF(sPylonRecord, mPrev);
    OFF(sPylonRecord, mCount);
    OFF(sPylonRecord, mBraces);
    OFF(sPylonControl, mFlags);
    OFF(sPylonControl, mF30);
    OFF(sPylonControl, mF44);
    OFF(sPylonControl, mFB4);
    OFF(sPylonControl, mFB8);
    OFF(sPylonControl, mFD0);
    OFF(sPylonControl, mRideType);
    OFF(tag_sRideAnimSet, mFlags);
    OFF(tag_sRideAnimSet, mTimeControls);
    OFF(tag_sRideAnimSet, mGroups);
    out[n++] = static_cast<int64_t>(sizeof(sRideAnimGroup));
    OFF(tag_sRideAnimSet, mBraceBlock);
    OFF(sAnimTimeControl, mF1C);
    OFF(sAnimTimeControl, mF20);
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControl));
    OFF(sFixedRideParams, mX);
    OFF(sFixedRideParams, mZ);
    OFF(sFixedRideParams, mRotation);
    OFF(sFixedRideParams, mF0C);
#undef OFF
    return n;
}

// ---- the functions
uint64_t tpw_ntb_new(uint64_t pylon, float x, float z, float loft, uint32_t flags, uint64_t control, uint32_t loops, uint32_t level, uint32_t extra) {
    tag_sTrackBrace* brace = NewTrackBrace(reinterpret_cast<sPylonRecord*>(static_cast<uintptr_t>(pylon)), x, z, loft, flags,
                                           reinterpret_cast<sPylonControl*>(static_cast<uintptr_t>(control)), loops, level, extra);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(brace));
}

void tpw_ntb_unlink(uint64_t instance) { UnLinkPreviewBrace(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance))); }

}  // extern "C"
