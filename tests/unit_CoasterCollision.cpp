// Test library for the coaster collision system (unit v5L20; libtpw_unit_CoasterCollision.dylib, tests/test_CoasterCollision.py),
// main app:
//   src/TPConfig_collision.cpp   GetPylonAtCell, GetTrackBounds, RecalcTrackBounds, GenerateSamplesInBoundArea,
//                                DoProperChunkCollision, DotProduct3D, CheckTrackToTrackCollisions, RemoveAllCollisionsFromBrace,
//                                CheckTrackToAllTracksCollisions, CheckAllTrackToTrackCollisions, DoCollisionChecking,
//                                RemoveBraceFromCollisionSystem, FlagRecalcAllCoasterCollisions
//
// The callees outside the unit: NormaliseVector (not recorded: the original's own body runs on the PowerPC, a copy of the host's
// translation, src/BMP_InsideArea_PointOutAllInverse_BMP_Distance.cpp, runs here), MReleaseAllocF (records the size and the flags,
// labels the zeroed block) and MReleaseFreeF (records the block's label). The globals of other files (gCoasterInstances,
// gHeightfieldHeader, gTPGlobals with its sine table) are defined here.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here (gTPGlobals as raw storage: only its sine table is read, the test writes it)
alignas(16) unsigned char gTPGlobalsRaw[sizeof(TPGlobals)] __asm__("_gTPGlobals");
tag_sCoasterInstance* gCoasterInstances;
tag_sM3D2Header* gHeightfieldHeader;

void NormaliseVector(s3DPoint* v) {
    double dx = static_cast<double>(v->x) * static_cast<double>(v->x);
    float yy = v->y * v->y;
    float zz = v->z * v->z;
    double sum = static_cast<double>(zz) + (dx + static_cast<double>(yy));
    float len = static_cast<float>(std::sqrt(sum));
    v->x = v->x / len;
    v->y = v->y / len;
    v->z = v->z / len;
}

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {static_cast<int64_t>(size), static_cast<int64_t>(flags)});
    void* block = tpw::ArenaAllocZeroed(size == 0 ? 1 : size);
    newlab(block);
    return block;
}

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}

namespace {
template <class T>
T* P(uint64_t v) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(v));
}
uint64_t U(const void* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

extern "C" {

uint64_t tpw_ccol_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCollisionChunksA", &gCollisionChunksA}, {"gCollisionChunksB", &gCollisionChunksB},
        {"gCollisionBlockA", &gCollisionBlockA},   {"gCollisionBlockB", &gCollisionBlockB},
        {"gCollisionSampleA", &gCollisionSampleA}, {"gCollisionSampleB", &gCollisionSampleB},
        {"gCollisionTypeA", &gCollisionTypeA},     {"gCollisionTypeB", &gCollisionTypeB},
        {"gCollisionRingB", &gCollisionRings[0]},  {"gCollisionRingA", &gCollisionRings[12]},
        {"gCollisionCorner0", &gCollisionCorner0}, {"gCollisionCorner1", &gCollisionCorner1},
        {"gCollisionCorner2", &gCollisionCorner2}, {"gCollisionCorner3", &gCollisionCorner3},
        {"gCoasterInstances", &gCoasterInstances}, {"gHeightfieldHeader", &gHeightfieldHeader},
        {"gTPGlobals", gTPGlobalsRaw},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return U(entry.address);
    return 0;
}

uint64_t tpw_ccol_alloc(uint32_t size) { return U(tpw::ArenaAllocZeroed(size)); }

// The header's height field (an arena offset on the host).
void tpw_ccol_set_hfield(uint64_t header, uint64_t field) { P<tag_sM3D2Header>(header)->mHField = P<sHField>(field); }

int32_t tpw_ccol_layout(int64_t* out) {
    int32_t n = 0;
#define O(S, M) out[n++] = static_cast<int64_t>(offsetof(S, M))
#define Z(S) out[n++] = static_cast<int64_t>(sizeof(S))
    O(tag_sCoasterInstance, mType);
    O(tag_sCoasterInstance, mNext);
    O(tag_sCoasterInstance, mPylons);
    O(tag_sCoasterInstance, mLastBrace);
    O(tag_sCoasterInstance, mSamples);
    O(tag_sCoasterInstance, mSegmentSpan);
    O(tag_sCoasterInstance, mCollisionRefs);
    Z(tag_sCoasterInstance);
    O(tag_sTrackBrace, mNumber);
    O(tag_sTrackBrace, mFlags);
    O(tag_sTrackBrace, mPrev);
    O(tag_sTrackBrace, mNext);
    O(tag_sTrackBrace, mFB0);
    O(tag_sTrackBrace, mCollisions);
    O(tag_sTrackBrace, mCollisionBounds);
    O(tag_sTrackBrace, mSample);
    O(tag_sTrackBrace, mData);
    Z(tag_sTrackBrace);
    O(sTrackBraceData, mFlags);
    O(sTrackBraceData, mF18Table);
    Z(sTrackBraceData);
    O(sTrackSample, mPos);
    O(sTrackSample, mYaw);
    Z(sTrackSample);
    O(tag_sCoasterType, mPairs);
    O(tag_sCoasterType, mPairCount);
    Z(tag_sCoasterType);
    O(sPylonRecord, mFlags);
    O(sPylonRecord, mIndex);
    O(sPylonRecord, mNext);
    Z(sPylonRecord);
    Z(tag_sM3D2Header);
    O(sHField, mF18);
    Z(sHField);
    Z(sPylonControl);
#undef O
#undef Z
    return n;
}

uint64_t tpw_ccol_pylon_at(uint32_t x, uint32_t z, uint64_t out) { return U(GetPylonAtCell(x, z, P<uint32_t>(out))); }
void tpw_ccol_bounds(uint64_t inst, uint64_t a, uint64_t b, uint64_t box) {
    GetTrackBounds(P<tag_sCoasterInstance>(inst), P<tag_sTrackBrace>(a), P<tag_sTrackBrace>(b), P<sAlignedBox>(box));
}
void tpw_ccol_recalc_bounds(uint64_t inst) { RecalcTrackBounds(P<tag_sCoasterInstance>(inst)); }
void tpw_ccol_generate(uint64_t inst, uint64_t a, uint64_t b, uint64_t box, uint64_t chunks, uint64_t count) {
    GenerateSamplesInBoundArea(P<tag_sCoasterInstance>(inst), P<tag_sTrackBrace>(a), P<tag_sTrackBrace>(b), P<sAlignedBox>(box),
                               P<sTrackChunk>(chunks), P<uint32_t>(count));
}
int32_t tpw_ccol_proper(uint64_t a, uint64_t b) { return DoProperChunkCollision(P<sTrackChunk>(a), P<sTrackChunk>(b)); }
uint32_t tpw_ccol_dot(uint64_t a, uint64_t b) { return fbits(DotProduct3D(P<s3DPoint>(a), P<s3DPoint>(b))); }
void tpw_ccol_track_to_track(uint64_t ia, uint64_t ib, uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1) {
    CheckTrackToTrackCollisions(P<tag_sCoasterInstance>(ia), P<tag_sCoasterInstance>(ib), P<tag_sTrackBrace>(a0),
                                P<tag_sTrackBrace>(a1), P<tag_sTrackBrace>(b0), P<tag_sTrackBrace>(b1));
}
void tpw_ccol_remove_all(uint64_t inst, uint64_t b) { RemoveAllCollisionsFromBrace(P<tag_sCoasterInstance>(inst), P<tag_sTrackBrace>(b)); }
void tpw_ccol_to_all(uint64_t inst, uint64_t b) { CheckTrackToAllTracksCollisions(P<tag_sCoasterInstance>(inst), P<tag_sTrackBrace>(b)); }
void tpw_ccol_check_all(uint64_t inst) { CheckAllTrackToTrackCollisions(P<tag_sCoasterInstance>(inst)); }
void tpw_ccol_do_checking(uint64_t inst) { DoCollisionChecking(P<tag_sCoasterInstance>(inst)); }
void tpw_ccol_remove_brace(uint64_t b, uint64_t inst) { RemoveBraceFromCollisionSystem(P<tag_sTrackBrace>(b), P<tag_sCoasterInstance>(inst)); }
void tpw_ccol_flag_recalc(uint64_t inst) { FlagRecalcAllCoasterCollisions(P<tag_sCoasterInstance>(inst)); }

}  // extern "C"
