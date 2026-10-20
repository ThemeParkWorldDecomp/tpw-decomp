// Unit w119c: the seven functions are real; six external callees record complete arguments.
#include "RideDB.h"
#include "TPCoaster.h"
#include "CoasterBalance.h"
#include "LbFile.h"
#include "calllog.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
using namespace calllog;

uint32_t gCoasterTypeCount;
tag_sCoasterType* gCoasterTypes[kMaxCoasterTypes];
namespace {
void* gAllocations[32];
uint32_t gSizes[32];
uint32_t gAllocated;
tag_sCoasterType* gLatest;
sDTextureSet* gTextures[2];
void* gMarker;
CBalanceTable* gLocal;

// Snapshots expose publication, count and field stores at each external call boundary.
void Snapshot(const char* name, tag_sCoasterType* type) {
    ev(name, {lab(type), gCoasterTypeCount, type->mRideTypeNumber,
        lab(gCoasterTypes[gCoasterTypeCount - 1]), lab(type->mTextures),
        static_cast<uint32_t>(type->mF3C), static_cast<uint32_t>(type->mF2E8),
        static_cast<uint32_t>(type->mF2F8), static_cast<uint32_t>(type->mF318)});
}
void* Allocate(uint32_t size, uint8_t fill) {
    void* block = std::malloc(size + 16);
    std::memset(block, fill, size);
    std::memset(static_cast<uint8_t*>(block) + size, 0xa5, 16);
    gAllocations[gAllocated] = block;
    gSizes[gAllocated++] = size;
    newlab(block);
    return block;
}
}

// This fake preserves the exact requested count and an unwritten guard after the allocation.
void* LbMem_Alloc(uint32_t size) {
    ev("Alloc", {size});
    return Allocate(size, 0xa5);
}
// The host's coaster type is bigger than the Mac's 0x35c bytes (native pointers): its size is logged as the Mac's.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("ReleaseAlloc", {size == sizeof(tag_sCoasterType) ? 0x35cu : size, flags});
    return Allocate(size, 0);
}
// The unused local balance is identified independently from the supplied balance.
void CBalanceTable::Init() {
    gLocal = this;
    tpw_cl_label(50, this);
    ev("Init", {lab(this), GetBalanceDescription(0) == gCoasterBalanceDescriptions});
    mRows = gMarker;
    GetBalanceValuesStart()[0].mInteger = 0x12345678;
}
void CBalanceTable::UnInit() {
    ev("UnInit", {lab(this), this == gLocal, lab(mRows),
        static_cast<uint32_t>(GetBalanceValuesStart()[0].mInteger)}, {gLatest->mTextures->mSharedFolder});
    Snapshot("AfterFolder", gLatest);
}
// Defaults mutate fields and supply the first texture pointer; creation may replace it.
void SetCoasterTypeDefaults(tag_sCoasterType* type) {
    gLatest = type;
    Snapshot("Defaults", type);
    type->mF3C = 150;
    type->mF2E8 = static_cast<int32_t>(ret("factor"));
    type->mTextures = gTextures[0];
    type->mF14 = gMarker;
    type->mPylonsA.mUnknown[0] = 0x67;
}
int32_t CreateCoasterFromSamFile(tag_sCoasterType* type, char* path, CCoasterBalance* balance) {
    ev("Create", {lab(type), lab(balance)}, {path});
    Snapshot("BeforeCreate", type);
    type->mTextures = gTextures[ret("replace") != 0];
    type->mF2F8 = static_cast<int32_t>(ret("second"));
    type->mF318 = static_cast<int32_t>(ret("product"));
    type->mRecords = reinterpret_cast<sCoasterRecord*>(gMarker);
    type->mPylonsB.mUnknown[0x11f] = 0x89;
    balance->mValues[5].mInteger = static_cast<int32_t>(ret("balance"));
    return static_cast<int32_t>(ret("Create"));
}

extern "C" {
// Generic wrappers use raw native RideInfo storage, without constructing its unrelated balance.
void tpw_rh_copy(const char16_t** out, const char16_t* text) { copy_line(out, text); }
int32_t tpw_rh_comp(const char16_t* a, const char16_t* b) { return comp(a, b); }
int32_t tpw_rh_id(const char16_t* text) { return find_string_id(text); }
const char16_t* tpw_rh_line(const char16_t* text) { return find_line(text); }
uint32_t tpw_rh_particle(const RideInfo* ride, int32_t index) { return ride->GetCustomParticleEffect(index); }
int32_t tpw_rh_sound(const RideInfo* ride) { return ride->GetCustomSoundBank(); }
tag_sCoasterType* tpw_rh_coaster(uint32_t type, char* folder, char* path, CCoasterBalance* balance) {
    return LoadCoasterTypeDataFile(type, folder, path, *balance);
}
void tpw_rh_reset(sDTextureSet* a, sDTextureSet* b, void* marker) {
    for (uint32_t i = 0; i < gAllocated; ++i) std::free(gAllocations[i]);
    gAllocated = 0;
    gLatest = nullptr;
    gLocal = nullptr;
    gTextures[0] = a;
    gTextures[1] = b;
    gMarker = marker;
}
void* tpw_rh_alloc(uint32_t index) { return gAllocations[index]; }
uint32_t tpw_rh_size(uint32_t index) { return gSizes[index]; }
uint32_t tpw_rh_nalloc() { return gAllocated; }
void* tpw_rh_count() { return &gCoasterTypeCount; }
void* tpw_rh_table() { return gCoasterTypes; }
uint32_t tpw_rh_layout(uint32_t kind, uint32_t field) {
    static const uint32_t coaster[] = {
        sizeof(tag_sCoasterType), offsetof(tag_sCoasterType, mRideTypeNumber),
        offsetof(tag_sCoasterType, mF14), offsetof(tag_sCoasterType, mMeshSet),
        offsetof(tag_sCoasterType, mTextures), offsetof(tag_sCoasterType, mF78),
        offsetof(tag_sCoasterType, mRecords), offsetof(tag_sCoasterType, mRecordIndex),
        offsetof(tag_sCoasterType, mF90), offsetof(tag_sCoasterType, mF94), offsetof(tag_sCoasterType, mF98),
        offsetof(tag_sCoasterType, mCarTypeA), offsetof(tag_sCoasterType, mCarTypeB),
        offsetof(tag_sCoasterType, mCarTypeC), offsetof(tag_sCoasterType, mCarTypes),
        offsetof(tag_sCoasterType, mF3C), offsetof(tag_sCoasterType, mPylonsA),
        offsetof(tag_sCoasterType, mPylonsB), offsetof(tag_sCoasterType, mF2E8),
        offsetof(tag_sCoasterType, mF2F8), offsetof(tag_sCoasterType, mF318)};
    static const uint32_t texture[] = {
        sizeof(sDTextureSet), offsetof(sDTextureSet, mFlags), offsetof(sDTextureSet, mCount),
        offsetof(sDTextureSet, mFolder), offsetof(sDTextureSet, mSharedFolder),
        offsetof(sDTextureSet, mEntries), offsetof(sDTextureSet, mInfos),
        offsetof(sDTextureSet, mTextures), offsetof(sDTextureSet, mUse) + offsetof(sDMeshShared, mInstances)};
    static const uint32_t ride[] = {sizeof(RideInfo), offsetof(RideInfo, mEmitterWords)};
    static const uint32_t balance[] = {sizeof(CCoasterBalance), offsetof(CCoasterBalance, mRows),
        offsetof(CCoasterBalance, mValues)};
    return kind == 0 ? coaster[field] : kind == 1 ? texture[field] : kind == 2 ? ride[field] : balance[field];
}
const char16_t* tpw_rh_tag(uint32_t index) { return gRideStringTags[index]; }
}
