// Test library for the coaster type's remaining loaders (unit w148; libtpw_unit_CoasterLoaders.dylib, tests/test_CoasterLoaders.py), main app:
//   src/TPCoaster_loaders.cpp   CreateTrackSectionMesh, SetupTrainType, SetupCarType, LoadCoasterPylons, SetupPylonData,
//                               CreateCrossSectionSelectsFromSamFile, SetCrossSectionSelectDefaults, RepackCrossSection,
//                               OptimiseCrossSections, GetTempFixedRideParams
//
// The callees outside the unit are recording fakes (the same event names and arguments as the PowerPC stubs of the module, pointers as
// labels, floats as bits) that answer from the test's queues: MReleaseAllocF, LoadRideType, CTRACE, CountDataNodesByType,
// FindNthDataNodeIndex, IsValidAnimation, SetNewAnim, InstanceRide, AnimateRideSet, DestroyRideInstance, DMCreateMesh, DMFreeTextures
// and SetMatrixIdentity. The unit's own functions call each other for real. The globals of other files the unit reads (gRideTypes,
// gRideAnimSets) are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "CoasterBalance.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here (src/RideTypes.cpp is not part of this unit)
tag_sRideAnimSet* gRideTypes[16];
tag_sRideAnimSet* gRideAnimSets[16];

namespace {

int64_t I(uint32_t v) { return static_cast<int64_t>(v); }
int64_t I(int32_t v) { return static_cast<int64_t>(v); }
int64_t I(bool v) { return v ? 1 : 0; }
const char* Text(const char* t) { return t != nullptr ? t : "(null)"; }

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
// A zeroed block of the requested size (the host records are bigger than the Mac's: the module maps the sizes it logs).
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {I(size), I(flags)});
    void* block = tpw::ArenaAllocZeroed(size != 0 ? size : 1);
    newlab(block);
    return block;
}

uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t c, const char16_t* sign1,
                      const char16_t* sign2, int32_t d, int32_t e) {
    const int32_t* pair = static_cast<const int32_t*>(a);
    ev("LoadRideType", {I(a != nullptr), a != nullptr ? I(pair[0]) : 0, a != nullptr ? I(pair[1]) : 0, I(lab(b)), I(flags), I(id), I(c),
                        I(sign1 != nullptr), I(sign2 != nullptr), I(d), I(e)},
       {Text(folder), Text(name)});
    return static_cast<uint32_t>(ret("LoadRideType"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* name = va_arg(args, const char*);
    int count = va_arg(args, int);
    va_end(args);
    ev("CTRACE", {I(level), I(static_cast<int32_t>(count))}, {Text(format), Text(name)});
}

uint32_t CountDataNodesByType(tag_sRideAnimSet* set, uint32_t kind) {
    ev("CountDataNodesByType", {I(lab(set)), I(kind)});
    return static_cast<uint32_t>(ret("Count", kind, 0));
}

int32_t FindNthDataNodeIndex(tag_sRideAnimSet* set, uint32_t kind, uint32_t n) {
    ev("FindNthDataNodeIndex", {I(lab(set)), I(kind), I(n)});
    char key[64];
    std::snprintf(key, sizeof(key), "Find:%u:%u", kind, n);
    return static_cast<int32_t>(ret(key, -1));
}

int32_t IsValidAnimation(tag_sRideAnimSet*& set, uint32_t a, uint32_t b) {
    ev("IsValidAnimation", {I(lab(set)), I(a), I(b)});
    char key[64];
    std::snprintf(key, sizeof(key), "IsValid:%u:%u", a, b);
    return static_cast<int32_t>(ret(key, 0));
}

void SetNewAnim(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control, uint32_t flags, float speed) {
    const sAnimTimeControl* first = static_cast<const sAnimTimeControl*>(set->mTimeControls);
    ev("SetNewAnim", {I(lab(set)), I(kind), I(index), static_cast<int64_t>(control - first), I(flags), I(fbits(speed))});
}

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e,
                      uint32_t f, int32_t g) {
    const int32_t* pair = static_cast<const int32_t*>(params->mF0C);
    ev("InstanceRide", {I(type), I(flags), I(params->mX), I(params->mZ), I(params->mRotation), I(pair != nullptr),
                        pair != nullptr ? I(pair[0]) : 0, pair != nullptr ? I(pair[1]) : 0, I(a), I(b), I(c != nullptr), I(d)});
    ev("InstanceRideMore", {I(e), I(f), I(g)});
    return static_cast<uint32_t>(ret("InstanceRide"));
}

// Writes into the +0x34 float of each embedded record's block the next value of the queue "AnimateRideSet:<record>" (so that the
// second call of SetupPylonData reads other numbers than the first).
void AnimateRideSet(tag_sRideAnimSet* set, int32_t a, uint32_t flags) {
    ev("AnimateRideSet", {I(lab(set)), I(a), I(flags)});
    const tag_sM3D2Header* header = set->mHeads->mHeader;
    const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(set->mHeads->mF28);
    for (uint32_t i = 0; i < header->mEmbeddedCount; i++) {
        uint8_t* block = embedded->mRecords[i].mBlock;
        const uint32_t bits = static_cast<uint32_t>(ret("AnimateRideSet", static_cast<int64_t>(i), 0));
        if (block != nullptr) std::memcpy(block + 0x34, &bits, 4);
    }
}

void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {I(instance)}); }

int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags) {
    ev("DMCreateMesh", {I(lab(set)), I(lab(textures)), I(flags)}, {Text(name)});
    return static_cast<int32_t>(ret("DMCreateMesh"));
}

void DMFreeTextures(sDTextureSet* set) { ev("DMFreeTextures", {I(lab(set))}); }

void SetMatrixIdentity(sMatrix* matrix) { ev("SetMatrixIdentity", {I(lab(matrix))}); }

extern "C" {

// The host address of the globals: 0 the table of ride types, 1 the table of ride instances.
uint64_t tpw_cld_addr(int32_t which) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(which == 0 ? static_cast<void*>(gRideTypes) : static_cast<void*>(gRideAnimSets)));
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cld_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_cld_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The offsets and sizes the module relies on, in the order of FIELDS in the module (one list generates both).
int32_t tpw_cld_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sCarType));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mValue));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mName));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mRideType));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF10));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF14));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF18));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mNodeIndex));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF24));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF28));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mHeadCount));
    out[n++] = static_cast<int64_t>(sizeof(sPylonControl));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mName));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mRideType));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF18));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF20));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF24));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF28));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF2C));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF30));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF34));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF38));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF3C));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF40));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF44));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF48));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF4C));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF50));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF54));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mF68));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFB4));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFB8));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFBC));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFC0));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFC4));
    out[n++] = static_cast<int64_t>(offsetof(sPylonControl, mFD0));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF14));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mMeshSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mTextures));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF30));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF34));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF38));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mSelectFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mSelectCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mSelectA));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mSelectB));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF98));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF2F4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF2F8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mCarTypeA));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mCarTypeB));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mCarTypeC));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF30C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF310));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mF314));
    out[n++] = static_cast<int64_t>(sizeof(sDMeshEntry));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mFaceCount));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mVertexCount));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mUVCount));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mFaceSetCount));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mPointCount));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mF0E));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mBox));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mMatrix));
    out[n++] = static_cast<int64_t>(sizeof(sDTextureSet));
    out[n++] = static_cast<int64_t>(offsetof(sDTextureSet, mCount));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeControls));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mGroups));
    out[n++] = static_cast<int64_t>(sizeof(sRideAnimGroup));
    out[n++] = static_cast<int64_t>(offsetof(sRideAnimGroup, mFlags));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mEmbeddedCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mEmbedded));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mBox));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertexCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mBox));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedData));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedData, mF04));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedSet));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedSet, mRecords));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedRecord, mBlock));
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControl));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF20));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCrossSectionSelect));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCrossSectionSelect, mF00));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCrossSectionSelect, mF20));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCrossSectionSelect, mF24));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCrossSectionSelect, mF28));
    out[n++] = static_cast<int64_t>(sizeof(sCoasterRecord));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mF00));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mPointCount));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mPoints));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterRecord, mPointsB));
    out[n++] = static_cast<int64_t>(sizeof(sFixedRideParams));
    out[n++] = static_cast<int64_t>(offsetof(sFixedRideParams, mX));
    out[n++] = static_cast<int64_t>(offsetof(sFixedRideParams, mZ));
    out[n++] = static_cast<int64_t>(offsetof(sFixedRideParams, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(sFixedRideParams, mF0C));
    out[n++] = static_cast<int64_t>(sizeof(sCrossSectionPoint));
    out[n++] = static_cast<int64_t>(sizeof(sCrossSectionEdge));
    out[n++] = static_cast<int64_t>(offsetof(sCrossSectionEdge, mF00));
    out[n++] = static_cast<int64_t>(offsetof(sCrossSectionEdge, mF04));
    out[n++] = static_cast<int64_t>(offsetof(sCrossSectionEdge, mF06));
    out[n++] = static_cast<int64_t>(offsetof(sCrossSectionEdge, mF08));
    out[n++] = static_cast<int64_t>(sizeof(CCoasterBalance));
    out[n++] = static_cast<int64_t>(offsetof(CCoasterBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    return n;
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t)
void tpw_cld_mesh(uint64_t type) { CreateTrackSectionMesh(reinterpret_cast<tag_sCoasterType*>(type)); }
void tpw_cld_train(uint64_t type, uint64_t folder) {
    SetupTrainType(reinterpret_cast<tag_sCoasterType*>(type), reinterpret_cast<char*>(folder));
}
void tpw_cld_car(uint64_t car, uint64_t folder) { SetupCarType(reinterpret_cast<sCarType*>(car), reinterpret_cast<char*>(folder)); }
void tpw_cld_pylons(uint64_t type, uint64_t first, uint64_t second, uint64_t folder) {
    LoadCoasterPylons(reinterpret_cast<tag_sCoasterType*>(type), reinterpret_cast<sPylonControl*>(first),
                      reinterpret_cast<sPylonControl*>(second), reinterpret_cast<char*>(folder));
}
void tpw_cld_pylon(uint64_t type, uint64_t pylon, uint32_t id) {
    SetupPylonData(reinterpret_cast<tag_sCoasterType*>(type), reinterpret_cast<sPylonControl*>(pylon), id);
}
int32_t tpw_cld_selects(uint64_t type, uint64_t records, uint32_t count, uint64_t balance, int32_t flag) {
    return CreateCrossSectionSelectsFromSamFile(reinterpret_cast<tag_sCoasterType*>(type), reinterpret_cast<sCoasterRecord*>(records),
                                                count, reinterpret_cast<CCoasterBalance*>(balance), flag);
}
void tpw_cld_defaults(uint64_t type, uint32_t count, uint64_t out, uint64_t a, uint64_t b, int32_t flag) {
    SetCrossSectionSelectDefaults(reinterpret_cast<tag_sCoasterType*>(type), count, reinterpret_cast<tag_sCrossSectionSelect**>(out),
                                  reinterpret_cast<sCoasterRecord*>(a), reinterpret_cast<sCoasterRecord*>(b), flag);
}
void tpw_cld_repack(uint64_t used, uint64_t record, uint64_t dst) {
    RepackCrossSection(reinterpret_cast<uint32_t*>(used), reinterpret_cast<sCoasterRecord*>(record),
                       reinterpret_cast<sCrossSectionPoint*>(dst));
}
void tpw_cld_optimise(uint64_t type, uint64_t records, uint32_t count) {
    OptimiseCrossSections(reinterpret_cast<tag_sCoasterType*>(type), reinterpret_cast<sCoasterRecord*>(records), count);
}
uint64_t tpw_cld_params(int32_t a, int32_t b, int32_t x, int32_t z) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GetTempFixedRideParams(a, b, x, z)));
}

}  // extern "C"
