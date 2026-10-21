// Test library for PlaceEndPylons and CoasterUVAnimTick (unit v5641; libtpw_unit_PlaceEndPylons.dylib, tests/test_PlaceEndPylons.py), main app:
//   src/game/rides/TPCoaster.cpp   PlaceEndPylons, CoasterUVAnimTick
//
// The callees outside the unit (FindDataNodeIndexByID, NewPylon, NewTrackBrace, GetRideAnimTime, FloatToCell) are fakes that record the
// arguments the real ones read and answer from the test's queues. gRideAnimSets is defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "tb_matrix.h"

using namespace calllog;

tag_sRideAnimSet* gRideAnimSets[16];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float bitsf(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

sPylonRecord gFakePylons[8];
int gFakePylonNext = 0;

}  // namespace

// ---- the fakes
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    (void)set;
    ev("Find", {static_cast<int64_t>(mask), static_cast<int64_t>(id)});
    return static_cast<uint32_t>(ret("Find"));
}

sPylonRecord* NewPylon(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, sPylonControl* control, s3DPoint* point) {
    ev("NewPylon", {static_cast<int64_t>(x), static_cast<int64_t>(z), static_cast<int64_t>(flags), static_cast<int64_t>(static_cast<uint32_t>(unused)),
                    lab(control), CanonBits(point->x), CanonBits(point->y), CanonBits(point->z)});
    sPylonRecord* pylon = &gFakePylons[gFakePylonNext++ & 7];
    pylon->mX = bitsf(static_cast<uint32_t>(ret("py.x")));
    pylon->mY = bitsf(static_cast<uint32_t>(ret("py.y")));
    pylon->mZ = bitsf(static_cast<uint32_t>(ret("py.z")));
    return pylon;
}

tag_sTrackBrace* NewTrackBrace(sPylonRecord* pylon, float x, float z, float loft, uint32_t isEnd, sPylonControl* control, uint32_t loops,
                               uint32_t level, uint32_t extra) {
    (void)pylon;
    ev("NewTrackBrace", {CanonBits(x), CanonBits(z), CanonBits(loft), static_cast<int64_t>(isEnd), lab(control), static_cast<int64_t>(loops),
                         static_cast<int64_t>(level), static_cast<int64_t>(extra)});
    return nullptr;
}

uint32_t GetRideAnimTime(int32_t which) {
    ev("GetRideAnimTime", {static_cast<int64_t>(static_cast<uint32_t>(which))});
    return static_cast<uint32_t>(ret("GetRideAnimTime"));
}

int32_t FloatToCell(double value) {
    ev("FloatToCell", {CanonBits(static_cast<float>(value))});
    return 0;
}

extern "C" {

uint64_t tpw_pep_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gRideAnimSets", gRideAnimSets},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pep_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_pep_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The host offsets, in the order of the module's NAMES.
int32_t tpw_pep_layout(int64_t* out) {
    int32_t n = 0;
#define OFF(T, m) out[n++] = static_cast<int64_t>(offsetof(T, m))
#define SZ(T) out[n++] = static_cast<int64_t>(sizeof(T))
    OFF(tag_sCoasterInstance, mType);
    OFF(tag_sCoasterInstance, mEntranceID);
    OFF(tag_sCoasterInstance, mPylons);
    OFF(tag_sCoasterInstance, mLastBrace);
    OFF(tag_sCoasterInstance, mF2C);
    OFF(tag_sCoasterInstance, mF138);
    OFF(tag_sCoasterInstance, mUVOffsets);
    OFF(tag_sCoasterInstance, mF38);
    SZ(tag_sCoasterInstance);
    OFF(tag_sTrackBrace, mData);
    OFF(tag_sTrackBrace, mWord70);
    SZ(tag_sTrackBrace);
    OFF(sTrackBraceData, mF18Table);
    SZ(sTrackBraceData);
    OFF(sPylonRecord, mX);
    OFF(sPylonRecord, mY);
    OFF(sPylonRecord, mZ);
    SZ(sPylonRecord);
    OFF(tag_sRideAnimSet, mHeads);
    SZ(tag_sRideAnimSet);
    OFF(tag_sMesh, mF28);
    OFF(tag_sMesh, mHeader);
    SZ(tag_sMesh);
    OFF(sEmbeddedSet, mRecords);
    OFF(sEmbeddedRecord, mBlock);
    SZ(sEmbeddedRecord);
    OFF(tag_sCoasterType, mTextures);
    OFF(tag_sCoasterType, mTextureCount);
    SZ(tag_sCoasterType);
    OFF(sDTextureSet, mEntries);
    OFF(sDTextureSet, mTextures);
    SZ(sDTextureSet);
    OFF(sDTextureEntry, mFlags);
    SZ(sDTextureEntry);
    OFF(tag_sDMeshSet, mMesh);
    OFF(tag_sDMeshSet, mF38);
    SZ(tag_sDMeshSet);
#undef OFF
#undef SZ
    return n;
}

void tpw_pep_place(uint64_t instance, float height) {
    PlaceEndPylons(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)), height);
}

void tpw_pep_uv(uint64_t instance) { CoasterUVAnimTick(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance))); }

}  // extern "C"
