// Test library for src/RideAnimChange.cpp (libtpw_unit_RideAnimChange.dylib; see
// tests/test_RideAnimChange.py): IsValidAnimation, SetupAnimationFlags,
// ResetRideAnimationSingleChannel, SetNewAnim, SetCurRideAnim, InterpolateCMorphData,
// InterpolateCMorphBoundData and RADecompressMorph.
//
// The unit also links the real ResetAnimTimeStats (src/RideAnimInit.cpp), which SetNewAnim calls
// for the clock words. The one other callee, EnableMeshDraw (the matrix form), is a fake that
// records its arguments and the matrix it is given (tests/calllog.h) and sets the mesh flags
// the real one sets. The mesh file's records (headers, nodes, animations, morphs) are built by
// the test in the arena, since their Ptr32 fields need it, and handed in as addresses; the ride
// set and the mesh, which use native pointers, are built here from the case's numbers (the type
// set, which the set's mSelf names, too).
#include <cstdint>
#include <cstring>
#include <vector>

#include "TPEngine.h"
#include "TPGlobals.h"
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

// A zeroed arena block that lives until the next tpw_rac_reset.
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

extern "C" {

// ---- the arena and the globals
uint64_t tpw_rac_base() { return reinterpret_cast<uint64_t>(tpw::ArenaBase()); }

// Frees the blocks of the previous case.
void tpw_rac_reset() {
    for (void* p : gBlocks) tpw::ArenaFree(p);
    gBlocks.clear();
}

// A zeroed arena block (the test fills it).
uint64_t tpw_rac_alloc(uint32_t bytes) { return reinterpret_cast<uint64_t>(Block(bytes)); }

// The globals the functions read: TPGlobals' words at 0x400c, 0x4010 and 0x4018.
void tpw_rac_set_globals(uint32_t engineFlags, uint32_t f4010, uint32_t f4018) {
    gTPGlobals.mEngineFlags = engineFlags;
    gTPGlobals.mF4010 = f4010;
    gTPGlobals.mF4014 = 0.0f;
    gTPGlobals.mF4018 = f4018;
}

// ---- the set functions. p: the world (BuildWorld), then the function's own numbers. out: Report's
// words, the first being the answer (-1 when the case calls the void form).

// IsValidAnimation: kind, index.
int32_t tpw_rac_is_valid(const int64_t* p, int64_t* out) {
    Reader r{p};
    BuildWorld(r);
    const uint32_t kind = r.word();
    const uint32_t index = r.word();
    tag_sRideAnimSet* set = &gSet;
    return Report(IsValidAnimation(set, kind, index), out);
}

// SetupAnimationFlags: the animation's address (the mesh is the set's).
int32_t tpw_rac_setup_flags(const int64_t* p, int64_t* out) {
    Reader r{p};
    BuildWorld(r);
    auto* animation = r.address<tag_sAnimHeader>();
    SetupAnimationFlags(animation, &gMesh);
    return Report(0, out);
}

// ResetRideAnimationSingleChannel: the old animation's address, the new one's (or 0).
int32_t tpw_rac_reset_channel(const int64_t* p, int64_t* out) {
    Reader r{p};
    BuildWorld(r);
    auto* animation = r.address<tag_sAnimHeader>();
    auto* other = r.address<tag_sAnimHeader>();
    ResetRideAnimationSingleChannel(&gSet, animation, other);
    return Report(0, out);
}

// SetNewAnim: kind, index, the control's address, flags, the speed's bits, whether to call the void form.
int32_t tpw_rac_set_new_anim(const int64_t* p, int64_t* out) {
    Reader r{p};
    BuildWorld(r);
    const uint32_t kind = r.word();
    const uint32_t index = r.word();
    auto* control = r.address<sAnimTimeControl>();
    const uint32_t flags = r.word();
    const float speed = FromBits(r.word());
    const bool viaVoid = r.next() != 0;
    if (viaVoid) {
        SetNewAnim(&gSet, kind, index, control, flags, speed);
        return Report(-1, out);
    }
    return Report(SetNewAnimDuration(&gSet, kind, index, control, flags, speed), out);
}

// SetCurRideAnim: kind, animation, loop word, the speed's bits, the control's number, whether to call the
// void form.
int32_t tpw_rac_set_cur_anim(const int64_t* p, int64_t* out) {
    Reader r{p};
    BuildWorld(r);
    const uint32_t kind = r.word();
    const uint32_t anim = r.word();
    const uint32_t loop = r.word();
    const float speed = FromBits(r.word());
    const uint32_t control = r.word();
    const bool viaVoid = r.next() != 0;
    if (viaVoid) {
        SetCurRideAnim(&gSet, kind, anim, loop, speed, control);
        return Report(-1, out);
    }
    return Report(SetCurRideAnimDuration(&gSet, kind, anim, loop, speed, control), out);
}

// ---- the morph functions (arena records, handed in as addresses)
void tpw_rac_interpolate_data(sMorphSet* morph, sVertexPosnSet* positions, sCMKeyData* key, uint32_t tBits,
                              int32_t add) {
    InterpolateCMorphData(morph, positions, key, FromBits(tBits), add);
}

void tpw_rac_interpolate_bound(sMorphSet* morph, sAlignedBox* box, sCMKeyData* key, uint32_t tBits, int32_t add) {
    InterpolateCMorphBoundData(morph, box, key, FromBits(tBits), add);
}

void tpw_rac_decompress(tag_sNode* node, sMorphSet* morph, uint32_t frameBits, uint32_t a, int32_t add) {
    RADecompressMorph(node, morph, FromBits(frameBits), a, add);
}

}  // extern "C"
