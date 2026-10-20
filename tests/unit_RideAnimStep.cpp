// RideAnimStep differential library. File records are built in the arena; native sets and
// meshes are built from numbers, using RideAnimTick and RideAnimChange's harness conventions.
#include <cstdint>
#include <cstring>
#include <vector>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPEmbed.h"
#include "calllog.h"
#include "fake_strings.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
void* gScratchGeomBuffer = nullptr;   // 0x102c7990, the scratch geometry buffer ResetRideAnimationSingleChannel uses

// RAInitNodeAnimNodes (linked with ResetAnimTimeStats, never called here) swaps morph points.
namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) {}
}  // namespace SamsUtilities

namespace {

constexpr int kEntries = 3;   // entries a group of the case's sets has

tag_sRideAnimSet gSet;        // the ride set under test (label 10)
tag_sRideAnimSet gTypeSet;    // the type's set, which gSet.mSelf names (label 11)
tag_sMesh gMesh;              // gSet.mHeads (label 20)
tag_sMesh gTypeMesh;          // gTypeSet.mHeads (label 21)
sRideAnimEntry gEntries[12][kEntries];
sMatrix gMatrix;              // the matrix EnableMeshDraw is given when the case has one (label 30)

std::vector<void*> gBlocks;

// A zeroed arena block that lives until the next tpw_ras_reset.
void* Block(size_t bytes) {
    void* p = tpw::ArenaAllocZeroed(bytes);
    gBlocks.push_back(p);
    return p;
}

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

// The case's numbers, read in the order the test wrote them.
struct Reader {
    const int64_t* p;
    int64_t next() { return *p++; }
    uint32_t word() { return static_cast<uint32_t>(*p++); }
    // An address the test built in the arena (or 0).
    template <class T>
    T* address() {
        return reinterpret_cast<T*>(static_cast<intptr_t>(*p++));
    }
};

// A matrix's sixteen words in two events.
void EvMatrix(const char* name, const sMatrix* matrix) {
    uint32_t w[16];
    std::memcpy(w, matrix, sizeof w);
    ev(name, {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev(name, {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}

// Builds the set and its mesh from the case's numbers: the set's flags, its time control count, the
// word at +0x14, the mesh's and the type mesh's first word, the headers' addresses (the set's and
// the type's), the time controls' address, the scratch buffer's address, whether there is a matrix
// (and its sixteen words), then twelve groups of {flags, count, kEntries animation addresses}.
void BuildWorld(Reader& r) {
    std::memset(&gSet, 0xa5, sizeof gSet);
    std::memset(&gTypeSet, 0xa5, sizeof gTypeSet);
    std::memset(&gMesh, 0xa5, sizeof gMesh);
    std::memset(&gTypeMesh, 0xa5, sizeof gTypeMesh);
    gSet.mFlags = r.word();
    gSet.mTimeCount = static_cast<uint16_t>(r.next());
    gSet.mAnimatedCount = r.word();
    gMesh.mF00 = r.word();
    gTypeMesh.mF00 = r.word();
    gMesh.mHeader = r.address<tag_sM3D2Header>();
    gTypeMesh.mHeader = r.address<tag_sM3D2Header>();
    gSet.mTimeControls = r.address<void>();
    gScratchGeomBuffer = r.address<void>();
    const bool hasMatrix = r.next() != 0;
    uint32_t words[16];
    for (uint32_t& w : words) w = r.word();
    std::memcpy(&gMatrix, words, sizeof gMatrix);
    gSet.mHeads = &gMesh;
    gSet.mSelf = &gTypeSet;
    gTypeSet.mHeads = &gTypeMesh;
    gSet.mMatrix = hasMatrix ? &gMatrix : nullptr;
    for (int k = 0; k < 12; ++k) {
        const uint32_t flags = r.word();
        const uint32_t count = r.word();
        gSet.mGroups[k] = {flags, count, gEntries[k]};
        for (int j = 0; j < kEntries; ++j) gEntries[k][j] = {static_cast<uint32_t>(j), r.address<tag_sAnimHeader>()};
    }
    tpw_cl_label(10, &gSet);
    tpw_cl_label(11, &gTypeSet);
    tpw_cl_label(20, &gMesh);
    tpw_cl_label(21, &gTypeMesh);
    tpw_cl_label(30, &gMatrix);
}

// The words every set function reports: the answer, the set's flags, the word at +0x14, the mesh's
// and the type mesh's first word, and the three globals the functions read (they must not be written).
int32_t Report(int64_t answer, int64_t* out) {
    out[0] = answer;
    out[1] = gSet.mFlags;
    out[2] = static_cast<uint32_t>(gSet.mAnimatedCount);
    out[3] = gMesh.mF00;
    out[4] = gTypeMesh.mF00;
    out[5] = gTPGlobals.mEngineFlags;
    out[6] = gTPGlobals.mF4010;
    out[7] = gTPGlobals.mF4018;
    return 8;
}

}  // namespace

// ---- the fake: the matrix form of EnableMeshDraw (0x10065200)
void EnableMeshDraw(tag_sMesh* mesh, sMatrix* matrix, int32_t flags) {
    ev("EnableMeshDraw", {lab(mesh), lab(matrix), flags, mesh->mF00});
    if (matrix != nullptr) EvMatrix("Matrix", matrix);
    mesh->mF00 |= 0x11;   // what the real one sets for the flags 1 form
}

// AnimateRideSet records its inputs and can change flags, to check the caller's reloads.
void AnimateRideSet(tag_sRideAnimSet* set, int32_t animate, uint32_t flags) {
    ev("AnimateRideSet", {lab(set), animate, flags, set->mFlags});
    set->mFlags = (set->mFlags & static_cast<uint32_t>(ret("AnimateAnd", 0xffffffffu))) |
                  static_cast<uint32_t>(ret("AnimateOr", 0));
}

// TickRideHeads receives the address of the mesh pointer, rather than the mesh itself.
void TickRideHeads(tag_sMesh*& mesh) {
    ev("TickRideHeads", {lab(mesh), lab(&mesh)});
}

// Hoarding helpers record the set, enable word and level bits, including the caller's state.
void EnableRideHoardings(tag_sRideAnimSet* set, int32_t enable) {
    ev("EnableRideHoardings", {lab(set), enable, set->mFlags});
}
void SetRideHoardingLevel(tag_sRideAnimSet* set, float level) {
    ev("SetRideHoardingLevel", {lab(set), fbits(level), set->mFlags});
}

extern "C" {

uint64_t tpw_ras_base() { return reinterpret_cast<uint64_t>(tpw::ArenaBase()); }

// Releases the previous case's file records and callback set.
void tpw_ras_reset() {
    for (void* p : gBlocks) tpw::ArenaFree(p);
    gBlocks.clear();
}

uint64_t tpw_ras_alloc(uint32_t bytes) { return reinterpret_cast<uint64_t>(Block(bytes)); }

// One entry point for the five functions; p holds BuildWorld's numbers, then step-specific fields.
int32_t tpw_ras_run(const int64_t* p, int64_t* out) {
    Reader r{p};
    BuildWorld(r);
    const int32_t operation = static_cast<int32_t>(r.next());
    const int32_t animate = static_cast<int32_t>(r.next());
    const bool present = r.next() != 0;
    gMesh.mF28 = r.next() != 0 ? gMesh.mHeader : nullptr;
    gSet.mHoardingMesh = r.next() != 0 ? gMesh.mHeader : nullptr;
    gSet.mDynamicMeshSet = r.next() != 0 ? reinterpret_cast<tag_sDMeshSet*>(gTypeMesh.mHeader) : nullptr;
    gSet.mHoardingLevel = FromBits(r.word());
    gSet.mHoardingSpeed = FromBits(r.word());
    const float step = FromBits(r.word());
    gTPGlobals.mEngineFlags = r.word();
    gTPGlobals.mF4010 = r.word();
    gTPGlobals.mF4018 = r.word();
    gTPGlobals.mF4014 = step;
    const uint32_t selected = r.word();
    const uint32_t outputs = r.word();
    uint32_t values[3] = {0x12345678, 0x23456789, 0x3456789a};
    uint32_t answer = 0;
    if (operation == 0) {
        // The callback uses a Ptr32 to a native set; the copied set therefore lives in the arena.
        auto* callbackSet = static_cast<tag_sRideAnimSet*>(Block(sizeof(tag_sRideAnimSet)));
        *callbackSet = gSet;
        tpw_cl_label(10, callbackSet);
        gMesh.mHeader->mRideSet = present ? callbackSet : nullptr;
        // The mesh-pointer argument is labelled by its position in the actual callback set.
        tpw_cl_label(40, &callbackSet->mHeads);
        AnimateRideCallback(gMesh.mHeader);
        gSet = *callbackSet;
    } else if (operation == 1) {
        tpw_cl_label(40, &gSet.mHeads);
        ProcessRideAnim(&gSet, animate);
    } else if (operation == 2) {
        ResetRideAnimationAllChannels(&gSet);
    } else if (operation == 3) {
        HandleActiveHoardings(&gSet, step);
    } else {
        answer = GetCurrentAnimStats(&gSet, outputs & 1 ? &values[0] : nullptr,
                                    outputs & 2 ? &values[1] : nullptr,
                                    outputs & 4 ? &values[2] : nullptr, selected);
    }
    Report(answer, out);
    out[8] = fbits(gSet.mHoardingLevel);
    out[9] = fbits(gSet.mHoardingSpeed);
    out[10] = values[0];
    out[11] = values[1];
    out[12] = values[2];
    out[13] = fbits(gTPGlobals.mF4014);
    return 14;
}

}  // extern "C"

