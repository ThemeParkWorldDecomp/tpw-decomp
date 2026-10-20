// Test library for DestroyTrackBrace and PlaceEntrancePylons (unit v5365; libtpw_unit_DestroyTrackBrace.dylib, tests/test_DestroyTrackBrace.py), main app:
//   src/TPCoaster_instances_DestroyTrackBrace.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of other
// files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"

using namespace calllog;

// ---- globals of other files
tag_sCoasterInstance* gCoasterEditInstance;
tag_sRideAnimSet* gRideAnimSets[16];

namespace {

tag_sTrackBrace* gSelf;
sPylonRecord* gPylon;
tag_sTrackBrace* gBraceRet;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {static_cast<int64_t>(instance)}); }

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}

tag_sTrackBrace* GetCurSelectedTrackBrace() {
    ev("GetCur", {});
    return ret("Cur") != 0 ? gSelf : reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(0x5a5a));
}

void SetCurSelectedTrackBrace(tag_sTrackBrace* brace) { ev("SetCur", {lab(brace)}); }

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("Find", {lab(set), static_cast<int64_t>(mask), static_cast<int64_t>(id)});
    return static_cast<uint32_t>(ret("Find"));
}

int32_t FloatToCell(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, 8);
    ev("Cell", {bits});
    return static_cast<int32_t>(ret("Cell"));
}

sPylonRecord* NewPylon(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, sPylonControl* control, s3DPoint* point) {
    int64_t a = 0, b = 0, c = 0;
    if (point != nullptr) {
        a = CanonBits(point->x);
        b = CanonBits(point->y);
        c = CanonBits(point->z);
    }
    ev("NewPylon", {static_cast<int64_t>(x), static_cast<int64_t>(z), static_cast<int64_t>(flags), static_cast<int64_t>(unused), lab(control), lab(point), a, b, c});
    return gPylon;
}

tag_sTrackBrace* NewTrackBrace(sPylonRecord* pylon, float x, float z, float loft, uint32_t isEnd, sPylonControl* control, uint32_t loops,
                               uint32_t level, uint32_t extra) {
    ev("NewTrackBrace", {lab(pylon), CanonBits(x), CanonBits(z), CanonBits(loft), static_cast<int64_t>(isEnd), lab(control),
                         static_cast<int64_t>(loops), static_cast<int64_t>(level), static_cast<int64_t>(extra)});
    return gBraceRet;
}

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) { ev("Setup", {lab(type), lab(brace)}); }
void SetPotentialPylonArea() { ev("SetPotential", {}); }
void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode) { ev("Flash", {lab(instance), static_cast<int64_t>(mode)}); }
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGen", {lab(instance), static_cast<int64_t>(flag)}); }

extern "C" {

uint64_t tpw_dt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},
        {"sets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of a block (what a Ptr32 field holds).
uint32_t tpw_dt_off(uint64_t address) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)))); }

// The objects the fakes answer with.
void tpw_dt_arm(uint64_t pylon, uint64_t braceRet, uint64_t self) {
    gPylon = reinterpret_cast<sPylonRecord*>(static_cast<uintptr_t>(pylon));
    gBraceRet = reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(braceRet));
    gSelf = reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(self));
}

// The host offsets, in the order of the module's NAMES.
int32_t tpw_dt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mBlk7C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mBlk80));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mBlk88));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mBlk8C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mEntranceID));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF40));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mPylonsA));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedSet, mRecords));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedRecord, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mX));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mY));
    out[n++] = static_cast<int64_t>(offsetof(sPylonRecord, mZ));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    out[n++] = static_cast<int64_t>(sizeof(sPylonRecord));
    return n;
}

void tpw_dt_destroy(uint64_t brace, uint64_t pylon) {
    DestroyTrackBrace(reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(brace)),
                      reinterpret_cast<sPylonControl*>(static_cast<uintptr_t>(pylon)));
}

void tpw_dt_place(uint64_t instance, uint32_t x, uint32_t z, float height, int32_t flag) {
    PlaceEntrancePylons(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)), x, z, height, flag);
}

}  // extern "C"
