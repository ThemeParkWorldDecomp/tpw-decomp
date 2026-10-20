// Unit w138: the six coaster type loaders are real; the allocator, ERRORBox and the four cross-section builders record
// complete arguments. Host record sizes differ from the Mac's, so the module maps sizes (see test_CoasterTypes.py).
#include "CoasterBalance.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "calllog.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
using namespace calllog;

uint32_t gCoasterTextureBase = 3;     // 0x1029ce20 (LevelSystems.cpp is not part of this unit)

namespace {
void* gAllocations[160];
uint32_t gSizes[160];
uint32_t gAllocated;
void* gPoints;

void* Allocate(uint32_t size) {
    void* block = std::malloc(size + 16);
    std::memset(block, 0, size);
    std::memset(static_cast<uint8_t*>(block) + size, 0xa5, 16);
    gAllocations[gAllocated] = block;
    gSizes[gAllocated++] = size;
    newlab(block);
    return block;
}
}

// The requested (host) size is logged; the module replaces it by the Mac's (host records are bigger).
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("ReleaseAlloc", {size, flags});
    return Allocate(size);
}
void ERRORBox(const char* format, ...) { ev("ERRORBox", {}, {format}); }

// The cross-section builders: each record gets scripted words (and the shared point block); the answers are scripted.
int32_t BuildCrossSectionPoints(tag_sCoasterType* type, sCoasterRecord* records, CCoasterBalance* balance, int32_t* out) {
    ev("Points", {lab(type), lab(records), lab(balance), lab(out)});
    for (uint32_t i = 0; i < type->mRecordIndex; ++i) {
        records[i].mF00 = static_cast<uint32_t>(ret("p00"));
        records[i].mPointCount = static_cast<uint32_t>(ret("pcount"));
        records[i].mF08 = static_cast<uint32_t>(ret("p08"));
        records[i].mBlock = ret("pblock") != 0 ? gPoints : nullptr;
        records[i].mF10 = static_cast<uint32_t>(ret("p10"));
        records[i].mF14 = static_cast<uint32_t>(ret("p14"));
    }
    *out = static_cast<int32_t>(ret("out"));
    return static_cast<int32_t>(ret("Points", 1));
}
int32_t BuildCrossSectionEdges(tag_sCoasterType* type, sCoasterRecord* records, CCoasterBalance* balance) {
    ev("Edges", {lab(type), lab(records), lab(balance)});
    return static_cast<int32_t>(ret("Edges", 1));
}
int32_t CreateCrossSectionSelectsFromSamFile(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count,
                                             CCoasterBalance* balance, int32_t out) {
    const sCoasterRecord& copy = records[count];
    ev("Selects", {lab(type), lab(records), count, lab(balance), out, copy.mF00, copy.mPointCount, copy.mF08,
                   lab(copy.mBlock), copy.mF10, copy.mF14});
    return static_cast<int32_t>(ret("Selects", 1));
}
void OptimiseCrossSections(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count) {
    ev("Optimise", {lab(type), lab(records), count});
}

extern "C" {
void tpw_ct_defaults(tag_sCoasterType* t) { SetCoasterTypeDefaults(t); }
int32_t tpw_ct_mesh(tag_sCoasterType* t, char* path, CCoasterBalance* b) { return CreateCoasterTrackMasterDMeshSet(t, path, b); }
int32_t tpw_ct_cross(tag_sCoasterType* t, CCoasterBalance* b) { return CreateCrossSectionsFromSamFile(t, b); }
int32_t tpw_ct_pylon(tag_sCoasterType* t, CCoasterBalance* b) { return SetupPylonControlsFromSamFile(t, b); }
int32_t tpw_ct_car(tag_sCoasterType* t, CCoasterBalance* b) { return SetupCarAndTrainTypesFromSamFile(t, b); }
int32_t tpw_ct_create(tag_sCoasterType* t, char* path, CCoasterBalance* b) { return CreateCoasterFromSamFile(t, path, b); }
void tpw_ct_reset(void* points, uint32_t base) {
    for (uint32_t i = 0; i < gAllocated; ++i) std::free(gAllocations[i]);
    gAllocated = 0;
    gPoints = points;
    gCoasterTextureBase = base;
}
void* tpw_ct_alloc(uint32_t index) { return gAllocations[index]; }
uint32_t tpw_ct_size(uint32_t index) { return gSizes[index]; }
uint32_t tpw_ct_nalloc() { return gAllocated; }
void* tpw_ct_statics() { return gCoasterStaticTextureEntries; }
void* tpw_ct_pairs() { return gCoasterDefaultPairs; }
// kind 0: the type (size, then the named words in the module's order); 1: the pylon control; 2: car; 3: record; 4: texture
// entry; 5: texture set; 6: mesh set; 7: mesh entry; 8: balance.
uint32_t tpw_ct_layout(uint32_t kind, uint32_t field) {
#define T(m) offsetof(tag_sCoasterType, m)
    static const uint32_t type[] = {sizeof(tag_sCoasterType),
        T(mFlags), T(mRideTypeNumber), T(mF08), T(mF0C), T(mF10), T(mF14), T(mMeshSet), T(mTextures), T(mTextureCount),
        T(mF24), T(mF28), T(mF2C), T(mF30), T(mF34), T(mF38), T(mF3C),
        T(mF40), T(mF44), T(mF48), T(mF4C), T(mF50), T(mF54), T(mF58), T(mF5C), T(mF60), T(mF64), T(mF68), T(mF6C), T(mF70),
        T(mF78), T(mRecords), T(mRecordIndex), T(mF90), T(mF94), T(mF98), T(mPairs), T(mPairCount), T(mFA4),
        T(mF2E8), T(mF2EC), T(mF2F0), T(mF2F4), T(mF2F8), T(mCarTypeA), T(mCarTypeB), T(mCarTypeC), T(mCarTypes),
        T(mF30C), T(mF310), T(mF314), T(mF318), T(mF31C),
        T(mF320), T(mF324), T(mF328), T(mF32C), T(mF330), T(mF334), T(mF338), T(mF33C), T(mF340), T(mF344), T(mF348),
        T(mF34C), T(mF350), T(mF354), T(mF358), T(mPylonsA), T(mPylonsB)};
#undef T
#define P(m) offsetof(sPylonControl, m)
    static const uint32_t pylon[] = {sizeof(sPylonControl),
        P(mFlags), P(mF0C), P(mName), P(mF58), P(mF5C), P(mF60), P(mF64), P(mF68), P(mF6C), P(mF70),
        P(mAngles), P(mAngles) + 4, P(mAngles) + 8, P(mAngles) + 12, P(mAngles) + 16, P(mAngles) + 20, P(mAngles) + 24,
        P(mAngles) + 28, P(mAngles) + 32, P(mAngles) + 36,
        P(mF9C), P(mFA0), P(mFA4), P(mFA8), P(mFAC), P(mFB0), P(mFB4), P(mFB8), P(mFCC), P(mFD0)};
#undef P
    static const uint32_t car[] = {sizeof(sCarType), offsetof(sCarType, mFlags), offsetof(sCarType, mValue),
        offsetof(sCarType, mName), offsetof(sCarType, mF10), offsetof(sCarType, mF14), offsetof(sCarType, mF18),
        offsetof(sCarType, mF1C), offsetof(sCarType, mBlock)};
    static const uint32_t record[] = {sizeof(sCoasterRecord), offsetof(sCoasterRecord, mF00),
        offsetof(sCoasterRecord, mPointCount), offsetof(sCoasterRecord, mF08), offsetof(sCoasterRecord, mBlock),
        offsetof(sCoasterRecord, mF10), offsetof(sCoasterRecord, mF14)};
    static const uint32_t entry[] = {sizeof(sDTextureEntry), offsetof(sDTextureEntry, mFlags),
        offsetof(sDTextureEntry, mName), offsetof(sDTextureEntry, mF08), offsetof(sDTextureEntry, mF0C)};
    static const uint32_t textures[] = {sizeof(sDTextureSet), offsetof(sDTextureSet, mFlags), offsetof(sDTextureSet, mCount),
        offsetof(sDTextureSet, mFolder), offsetof(sDTextureSet, mEntries)};
    static const uint32_t meshset[] = {sizeof(tag_sDMeshSet), offsetof(tag_sDMeshSet, mF00),
        offsetof(tag_sDMeshSet, mCount), offsetof(tag_sDMeshSet, mEntries)};
    static const uint32_t meshentry[] = {sizeof(sDMeshEntry), offsetof(sDMeshEntry, mFlags)};
    static const uint32_t balance[] = {sizeof(CCoasterBalance), offsetof(CCoasterBalance, mValues)};
    switch (kind) {
    case 0: return type[field];
    case 1: return pylon[field];
    case 2: return car[field];
    case 3: return record[field];
    case 4: return entry[field];
    case 5: return textures[field];
    case 6: return meshset[field];
    case 7: return meshentry[field];
    default: return balance[field];
    }
}
}
