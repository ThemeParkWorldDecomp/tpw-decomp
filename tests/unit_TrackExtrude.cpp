// Test library for the coaster track's mesh (unit w213; libtpw_unit_TrackExtrude.dylib, tests/test_TrackExtrude.py), main app:
//   src/game/rides/CoasterExtrude.cpp, src/game/rides/CoasterSpline.cpp   ExtrudeTrackSection, ExtrudeCrossSectionVertices, GetNewTrackMeshSet, SkinCrossSectionFaces,
//                              LinkFEStoreToFETextureCont, CopyExtrudedFaceData and InterpolateCoasterSpline (the sTrackSample overload)
//
// The untranslated callees are recording fakes (the same event names and arguments as the module's PowerPC stubs): the four spline
// functions BezierPosition, BezierTangent, GetAlignmentParamsQuick and GetBankParams (each writes words popped from the case's queues),
// SetMeshPosition, DMEnableMeshRender (answers 0) and InstanceDynamicMeshSet (hands out the prepared sets in turn, the last one again
// when they run out; answers 1). The globals of other files (the current coaster, its data block, the face stores, the sine table) are
// defined here; the module reaches them and the unit's own globals through tpw_tex_addr. Every block the module builds comes from the
// arena (tpw_tex_alloc), because the face lists and the mesh header keep arena offsets.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read: Format (the only member src/bullfrog/FileSystem.cpp leaves to the test) does nothing
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- the globals of other files
tag_sCoasterInstance* gCoasterCurrent;
void* gCoasterCurrentData;
void* gExtrudeFaceStoreB;
void* gExtrudeFaceStoreA;
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
static_assert(sizeof(TPGlobals::mSinTable) == 0x4000, "the table's size");
static_assert(offsetof(sDMeshEntry, mFaceCount) == 4 && offsetof(sDMeshEntry, mVertexCount) == 6 && offsetof(sDMeshEntry, mUVCount) == 8 &&
                  offsetof(sDMeshEntry, mFaceSetCount) == 0xa && offsetof(sDMeshEntry, mPointCount) == 0xc,
              "the mesh entry's counts are the Mac's");
static_assert(offsetof(sTrackBraceData, mFlags) == 4, "the brace data's flags");

namespace {

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// The next word of the case's queue, as a float.
float PopFloat(const char* key) { return FromBits(static_cast<uint32_t>(ret(key))); }

tag_sDMeshSet* gPrepared[3];
int32_t gPreparedNext;

}  // namespace

// ---- the fakes
void BezierPosition(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* point) {
    ev("BezierPosition", {lab(a), lab(b), fbits(t), lab(point)});
    point->x = PopFloat("bpos");
    point->y = PopFloat("bpos");
    point->z = PopFloat("bpos");
}

void BezierTangent(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* tangent) {
    ev("BezierTangent", {lab(a), lab(b), fbits(t), lab(tangent)});
    tangent->x = PopFloat("btan");
    tangent->y = PopFloat("btan");
    tangent->z = PopFloat("btan");
}

void GetAlignmentParamsQuick(s3DPoint tangent, float twist, sCoasterOrientation* orient, float* out) {
    ev("GetAlignmentParamsQuick", {fbits(tangent.x), fbits(tangent.y), fbits(tangent.z), fbits(twist), lab(orient), lab(out)});
    orient->mF00 = PopFloat("align");
    orient->mF04 = PopFloat("align");
    orient->mF08 = PopFloat("align");
    orient->mF0C = PopFloat("align");
    orient->mF10 = PopFloat("align");
    orient->mF14 = PopFloat("align");
    orient->mF18 = PopFloat("align");
}

void GetBankParams(s3DPoint* point, tag_sBezierControl* c, tag_sBezierControl* d, float t, sCoasterOrientation* orient) {
    ev("GetBankParams", {lab(point), lab(c), lab(d), fbits(t), lab(orient)});
    orient->mF0C = PopFloat("bank");
}

void SetMeshPosition(tag_sMesh* mesh, float x, float y, float z) {
    ev("SetMeshPosition", {lab(mesh), fbits(x), fbits(y), fbits(z)});
}

int32_t InstanceDynamicMeshSet(tag_sDMeshSet* set, sHeapInfo* heap, uint32_t flags, tag_sDMeshSet** output) {
    ev("InstanceDynamicMeshSet", {lab(set), lab(heap), static_cast<int64_t>(flags), lab(output)});
    int32_t k = gPreparedNext < 2 ? gPreparedNext : 2;
    ++gPreparedNext;
    *output = gPrepared[k];
    return 1;
}

int32_t DMEnableMeshRender(tag_sDMeshSet* set) {
    ev("DMEnableMeshRender", {lab(set)});
    return 0;
}

extern "C" {

// A zeroed arena block; the module fills it with the pattern and the guard.
uint64_t tpw_tex_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Gives a block back (the module frees what it allocated at the end of each case).
void tpw_tex_free(uint64_t address) { tpw::ArenaFree(reinterpret_cast<void*>(address)); }

// The arena offset (the Ptr32 word) of a block's address.
uint32_t tpw_tex_off(uint64_t address) { return address != 0 ? tpw::ArenaOffset(reinterpret_cast<const void*>(address)) : 0; }

// The three sets InstanceDynamicMeshSet hands out in turn.
void tpw_tex_prepare(uint64_t first, uint64_t second, uint64_t third) {
    gPrepared[0] = reinterpret_cast<tag_sDMeshSet*>(first);
    gPrepared[1] = reinterpret_cast<tag_sDMeshSet*>(second);
    gPrepared[2] = reinterpret_cast<tag_sDMeshSet*>(third);
    gPreparedNext = 0;
}

// The address of a global, in the order of the module's GL table.
uint64_t tpw_tex_addr(int32_t id) {
    void* const globals[] = {&gTPGlobals,           &gCoasterCurrent,    &gCoasterCurrentData,  &gExtrudeFaceStoreA,    &gExtrudeFaceStoreB,
                             gExtrudeParamsStatic,  &gExtrudeParamsTable, &gExtrudeTextureKind,  &gExtrudeFaceNext,      &gExtrudeKindFlag,
                             &gExtrudeWinding,      &gExtrudeTOffset,     &gExtrudeMax,          &gExtrudeMin,           &gExtrudeFaceSetCount,
                             &gExtrudeFacesUsed,    &gExtrudePointsUsed,  &gExtrudeUVsUsed,      &gExtrudeVertsUsed,     &gExtrudeFacesLeft,
                             &gExtrudePointsLeft,   &gExtrudeUVsLeft,     &gExtrudeVertsLeft};
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(globals[id]));
}

// The offsets and sizes the module relies on (LAYOUT, in its order).
int32_t tpw_tex_layout(int64_t* out) {
    int32_t n = 0;
#define L(x) out[n++] = static_cast<int64_t>(x)
    L(sizeof(tag_sCoasterInstance));
    L(offsetof(tag_sCoasterInstance, mType));
    L(offsetof(tag_sCoasterInstance, mF38));
    L(offsetof(tag_sCoasterInstance, mBlock3C));
    L(offsetof(tag_sCoasterInstance, mF70));
    L(offsetof(tag_sCoasterInstance, mF74));
    L(offsetof(tag_sCoasterInstance, mF78));
    L(offsetof(tag_sCoasterInstance, mSamples));
    L(sizeof(tag_sCoasterType));
    L(offsetof(tag_sCoasterType, mFlags));
    L(offsetof(tag_sCoasterType, mF14));
    L(offsetof(tag_sCoasterType, mMeshSet));
    L(offsetof(tag_sCoasterType, mTextures));
    L(offsetof(tag_sCoasterType, mTextureCount));
    L(offsetof(tag_sCoasterType, mSelectA));
    L(sizeof(tag_sDMeshSet));
    L(offsetof(tag_sDMeshSet, mVertexSets));
    L(offsetof(tag_sDMeshSet, mPoints));
    L(offsetof(tag_sDMeshSet, mUVSets));
    L(offsetof(tag_sDMeshSet, mFaces));
    L(offsetof(tag_sDMeshSet, mMesh));
    L(offsetof(tag_sDMeshSet, mF38));
    L(sizeof(tag_sMesh));
    L(offsetof(tag_sMesh, mHeader));
    L(sizeof(sCoasterRecord));
    L(offsetof(sCoasterRecord, mF00));
    L(offsetof(sCoasterRecord, mPointCount));
    L(offsetof(sCoasterRecord, mPoints));
    L(offsetof(sCoasterRecord, mBlock));
    L(offsetof(sCoasterRecord, mPointsB));
    L(offsetof(sCoasterRecord, mF14));
    L(sizeof(sExtrusionParams));
    L(offsetof(sExtrusionParams, mF00));
    L(offsetof(sExtrusionParams, mF04));
    L(offsetof(sExtrusionParams, mF08));
    L(offsetof(sExtrusionParams, mSection));
    L(sizeof(sBraceSpline));
    L(offsetof(sBraceSpline, mCount));
    L(offsetof(sBraceSpline, mControlsA));
    L(offsetof(sBraceSpline, mControlsB));
    L(sizeof(tag_sBezierControl));
    L(offsetof(tag_sBezierControl, mF08));
    L(offsetof(tag_sBezierControl, mF0C));
    L(offsetof(tag_sBezierControl, mBrace));
    L(offsetof(tag_sBezierControl, mF40));
    L(sizeof(tag_sTrackBrace));
    L(offsetof(tag_sTrackBrace, mNumber));
    L(offsetof(tag_sTrackBrace, mFlags));
    L(offsetof(tag_sTrackBrace, mF6C));
    L(offsetof(tag_sTrackBrace, mF90));
    L(offsetof(tag_sTrackBrace, mFB0));
    L(offsetof(tag_sTrackBrace, mFE4));
    L(offsetof(tag_sTrackBrace, mData));
    L(sizeof(sTrackSample));
    L(offsetof(sTrackSample, mBank));
    L(sizeof(sDTextureSet));
    L(offsetof(sDTextureSet, mCount));
    const char* base = reinterpret_cast<const char*>(&gTPGlobals);
    L(reinterpret_cast<const char*>(&gTPGlobals.mSinTableMask) - base);
    L(reinterpret_cast<const char*>(&gTPGlobals.mSinTableScale) - base);
    L(reinterpret_cast<const char*>(gTPGlobals.mSinTable) - base);
#undef L
    return n;
}

// One wrapper for each function; floats arrive as their bits, pointers as addresses.
void tpw_tex_ics(uint64_t samples, uint64_t params, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint32_t t, uint64_t point,
                 uint64_t orient) {
    InterpolateCoasterSpline(reinterpret_cast<sTrackSample*>(samples), reinterpret_cast<sCoasterTrackParams*>(params),
                             reinterpret_cast<tag_sBezierControl*>(a), reinterpret_cast<tag_sBezierControl*>(b),
                             reinterpret_cast<tag_sBezierControl*>(c), reinterpret_cast<tag_sBezierControl*>(d), FromBits(t),
                             reinterpret_cast<s3DPoint*>(point), reinterpret_cast<sCoasterOrientation*>(orient));
}

void tpw_tex_ecsv(uint32_t t, uint32_t a2, uint32_t a3, uint32_t a4, uint64_t point, uint64_t min, uint64_t max, int32_t first,
                  uint64_t section, uint64_t params) {
    ExtrudeCrossSectionVertices(FromBits(t), FromBits(a2), FromBits(a3), FromBits(a4), *reinterpret_cast<s3DPoint*>(point),
                                *reinterpret_cast<s3DPoint*>(min), *reinterpret_cast<s3DPoint*>(max), first,
                                reinterpret_cast<sCrossSection*>(section), reinterpret_cast<sExtrusionParams*>(params));
}

void tpw_tex_gntms(int32_t flag) { GetNewTrackMeshSet(flag); }

void tpw_tex_skin(uint64_t section, int32_t edges, int32_t pts) {
    SkinCrossSectionFaces(reinterpret_cast<sCrossSection*>(section), edges, pts);
}

uint64_t tpw_tex_link(uint64_t cont) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LinkFEStoreToFETextureCont(reinterpret_cast<sFaceExtrudeTextureCont*>(cont))));
}

void tpw_tex_copy() { CopyExtrudedFaceData(); }

void tpw_tex_ets(uint64_t brace1, uint64_t brace2, int32_t count, uint64_t ts, int32_t force) {
    ExtrudeTrackSection(reinterpret_cast<tag_sTrackBrace*>(brace1), reinterpret_cast<tag_sTrackBrace*>(brace2), count,
                        reinterpret_cast<float*>(ts), force);
}

}  // extern "C"
