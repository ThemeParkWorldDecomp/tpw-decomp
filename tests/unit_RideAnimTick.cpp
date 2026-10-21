// Test library for src/game/TPEmbed.cpp, src/game/TPMesh.cpp, src/game/rides/RideAnim.cpp (libtpw_unit_RideAnimTick.dylib; see
// tests/test_RideAnimTick.py): TickRideAnimSet, DoAnimateRideSet, RAAnimateTextureFrames,
// AnimateNode, GetAnimFrameData, IsMeshDrawEnabled, TPProcessEmbeddedData and
// DoWorldTransformsForEmbeddedData.
//
// The unit also links the real DisableMeshDraw (src/game/TPMesh.cpp), SetMatrixIdentity,
// CombineMatricesNoWDest2nd (0x100c28fc) and VectorThroughLMatrixNoW (src/game/LMatrix.cpp).
// Everything else the eight call is a fake that records its name, its arguments and the words of
// the records it is given (tests/calllog.h); some write what the real function would write, from
// the case's queues. The mesh file's records (headers, nodes, animations, keys, textures) are
// built by the test in the arena, since their Ptr32 fields need it, and handed in as addresses;
// the ride set, the mesh and the embedded records, which use native pointers, are built here from
// the case's numbers.
#include <cstdint>
#include <cstring>
#include <vector>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "calllog.h"
#include "fake_strings.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_mesh.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;

// The engine's map and mesh, as MeshDraw.cpp reaches them (as in tests/unit_MeshDraw.cpp).
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }
CMapWho::CMapWho() {}
CMapWho::~CMapWho() {}

namespace {

CMesh gEngine;
CMapWho gMap;
int32_t IsEngine(const CMapLinkBase* p) { return p == static_cast<const CMapLinkBase*>(&gEngine); }

std::vector<void*> gBlocks;

// A zeroed arena block that lives until the next tpw_rat_reset.
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

// Label of the node a matrix is the member of (the fakes name matrices by their node).
int64_t NodeOf(const sMatrix* matrix) {
    return lab(reinterpret_cast<const char*>(matrix) - offsetof(tag_sMiniNode, mMatrix));
}

// A matrix's sixteen words in two events.
void EvMatrix(const char* name, const sMatrix* matrix) {
    uint32_t w[16];
    std::memcpy(w, matrix, sizeof w);
    ev(name, {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev(name, {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}

// A control's fourteen words in two events (the animation it applied last as a label).
void EvControl(const sAnimTimeControl* control) {
    uint32_t w[14];
    std::memcpy(w, control, sizeof w);
    w[13] = static_cast<uint32_t>(lab(control->mF34.get()));
    ev("Control", {w[0], w[1], w[2], w[3], w[4], w[5], w[6]});
    ev("Control", {w[7], w[8], w[9], w[10], w[11], w[12], w[13]});
}

uint32_t Bits(float value) { return fbits(value); }

}  // namespace

// ---- fakes: the engine's map (MeshDraw.cpp)
CMapWho* gei_MapWho = &gMap;
void CMesh::SetLocalMatrix(int32_t flags, sMatrix& matrix) {
    ev("SetLocalMatrix", {this == &gEngine, flags});
    EvMatrix("Matrix", &matrix);
}
int32_t CMapWho::Link(CMapLinkBase* p, int32_t list) {
    ev("Link", {IsEngine(p), list});
    return 0;
}
int32_t CMapWho::Unlink(CMapLinkBase* p, int32_t list) {
    ev("Unlink", {IsEngine(p), list});
    return 0;
}
int32_t CMapWho::Link(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* p, int32_t list) {
    ev("LinkRange", {x0, z0, x1, z1, IsEngine(p), list});
    return 0;
}
int32_t CMapWho::Unlink(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* p) {
    ev("UnlinkRange", {x0, z0, x1, z1, IsEngine(p)});
    return 0;
}

// ---- fakes: starting an animation, and the node animation's helpers
// SetNewAnim records its arguments and the control it is given, then rewrites the control from
// the case's queues (the real one sets the kind and index, except for the kinds 0xd and 0xe, and
// works out the new animation's length): the kind and index (by default the arguments, or the
// control's own for those two kinds), the flags (and-ed with one queue's
// value, then or-ed with another's), the length (mF1C, as float bits) and the start word (mF10).
void SetNewAnim(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control, uint32_t flags,
                float speed) {
    ev("SetNewAnim", {lab(set), kind, index, lab(control), flags, Bits(speed)});
    EvControl(control);
    const bool special = kind == 0xd || kind == 0xe;   // the real one leaves the kind alone for these
    control->mF04 = static_cast<uint32_t>(ret("NewKind", special ? control->mF04 : kind));
    control->mF08 = static_cast<uint32_t>(ret("NewIndex", special ? control->mF08 : index));
    const uint32_t keep = static_cast<uint32_t>(ret("FlagsAnd", 0xffffffffll));
    control->mFlags = (control->mFlags & keep) | static_cast<uint32_t>(ret("FlagsOr", 0));
    control->mF1C = FromBits(static_cast<uint32_t>(ret("NewLength", Bits(control->mF1C))));
    control->mF10 = static_cast<int32_t>(ret("NewStart", control->mF10));
}
void RADecompressMorph(tag_sNode* node, sMorphSet* morph, float frame, uint32_t a, int32_t b) {
    ev("RADecompressMorph", {lab(node), lab(morph), Bits(frame), a, b});
}
// Writes the nine words of the case's "Axis" queue.
void RACalcOrientationBetweenControlPoints(sQuatKey* keys, uint32_t index, float fraction, s3DAxis* out) {
    ev("RACalcOrientation", {lab(keys), index, Bits(fraction)});
    float* words = &out->mX.x;
    for (int i = 0; i < 9; ++i) words[i] = FromBits(static_cast<uint32_t>(ret("Axis", 0)));
}
// The version with an axis as its source (0x100c37a4; the other, 0x100c28fc, is the real one).
void CombineMatricesNoWDest2nd(s3DAxis* source, sMatrix* destination) {
    uint32_t w[9];
    std::memcpy(w, source, sizeof w);
    ev("CombineAxis", {NodeOf(destination), w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8]});
    EvMatrix("CombineDestination", destination);
}
void SetLMatrixOrientation(sMatrix* matrix, s3DPoint* x, s3DPoint* y, s3DPoint* z) {
    uint32_t w[9];
    std::memcpy(w, x, sizeof(s3DPoint));
    std::memcpy(w + 3, y, sizeof(s3DPoint));
    std::memcpy(w + 6, z, sizeof(s3DPoint));
    ev("SetLMatrixOrientation", {NodeOf(matrix), w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], y == x + 1,
                                 z == y + 1});
    EvMatrix("SetLMatrixOrientationMatrix", matrix);
}
void SetLMatrixScaleNoTrans(sMatrix* matrix, float& x, float& y, float& z) {
    ev("SetLMatrixScale", {NodeOf(matrix), Bits(x), Bits(y), Bits(z)});
    EvMatrix("SetLMatrixScaleMatrix", matrix);
}
void SetLMatrixXZOrientation(sMatrix* matrix, s3DPoint* direction) {
    ev("SetLMatrixXZ", {NodeOf(matrix), Bits(direction->x), Bits(direction->y), Bits(direction->z)});
    EvMatrix("SetLMatrixXZMatrix", matrix);
}
// Writes one float from the "Place" queue.
void RAInterpPosnAtFrame(float* values, uint32_t count, float frame, float* out, int32_t wrap) {
    ev("RAInterpPosnAtFrame", {lab(values), count, Bits(frame), wrap});
    *out = FromBits(static_cast<uint32_t>(ret("Place", 0)));
}
namespace {
// The five shape interpolators: each writes three floats from the "Point" queue.
void ShapeFake(const char* name, tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    ev(name, {lab(shape), lab(a), index, Bits(fraction)});
    out->x = FromBits(static_cast<uint32_t>(ret("Point", 0)));
    out->y = FromBits(static_cast<uint32_t>(ret("Point", 0)));
    out->z = FromBits(static_cast<uint32_t>(ret("Point", 0)));
}
}  // namespace
void RABezierInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    ShapeFake("RABezierInter", shape, a, index, fraction, out);
}
void RABezierTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    ShapeFake("RABezierTangent", shape, a, index, fraction, out);
}
void RALinearInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    ShapeFake("RALinearInter", shape, a, index, fraction, out);
}
void RALinearTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    ShapeFake("RALinearTangent", shape, a, index, fraction, out);
}
void RACatmulInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    ShapeFake("RACatmulInter", shape, a, index, fraction, out);
}
void RAAnimateTextureCoords(sAnimTimeControl* control, tag_sNodeAnim* nodeAnim, int32_t a) {
    ev("RAAnimateTextureCoords", {lab(control), lab(nodeAnim), a});
}

// ---- fakes: the embedded data
// Writes the nine words of the "Orient" queue over the matrix's rotation.
void CalculateOrientFromTrackMorph(s3DPoint* points, s3DPoint* position, s3DPoint* normal,
                                   sTrackMorphingParent* morph, sMatrix* matrix) {
    uint32_t p[9];
    uint32_t q[6];
    uint32_t m[4];
    std::memcpy(p, points, sizeof p);
    std::memcpy(q, position, sizeof(s3DPoint));
    std::memcpy(q + 3, normal, sizeof(s3DPoint));
    m[0] = static_cast<uint32_t>(morph->mF00) | (static_cast<uint32_t>(morph->mBack) << 16);
    std::memcpy(m + 1, morph->mF04, sizeof(morph->mF04));
    ev("CalculateOrient", {lab(morph), p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]});
    ev("CalculateOrientMore", {q[0], q[1], q[2], q[3], q[4], q[5], m[0], m[1], m[2], m[3]});
    EvMatrix("CalculateOrientMatrix", matrix);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            matrix->m[row][column] = FromBits(static_cast<uint32_t>(ret("Orient", 0)));
        }
    }
}
void RecalculateInvalidNodeFaceNormals(tag_sNode* node) { ev("RecalculateInvalidNodeFaceNormals", {lab(node)}); }

extern "C" {

// ---- the arena and the globals
uint64_t tpw_rat_base() { return reinterpret_cast<uint64_t>(tpw::ArenaBase()); }

// Frees the blocks of the previous case.
void tpw_rat_reset() {
    for (void* p : gBlocks) tpw::ArenaFree(p);
    gBlocks.clear();
}

// A zeroed arena block (the test fills it).
uint64_t tpw_rat_alloc(uint32_t bytes) { return reinterpret_cast<uint64_t>(Block(bytes)); }

// The globals the eight read and write: TPGlobals' words at 0x400c, 0x4010, 0x4018, 0x401c, 0x4020.
void tpw_rat_set_globals(uint32_t engineFlags, uint32_t f4010, uint32_t f4018, uint32_t f401c, uint32_t f4020) {
    gTPGlobals.mEngineFlags = engineFlags;
    gTPGlobals.mF4010 = f4010;
    gTPGlobals.mF4014 = 0.0f;
    gTPGlobals.mF4018 = f4018;
    gTPGlobals.mF401C = f401c;
    gTPGlobals.mF4020 = f4020;
}
void tpw_rat_get_globals(uint32_t* out) {
    out[0] = gTPGlobals.mEngineFlags;
    out[1] = gTPGlobals.mF4010;
    out[2] = gTPGlobals.mF4018;
    out[3] = gTPGlobals.mF401C;
    out[4] = gTPGlobals.mF4020;
}

// ---- the plain ones
// IsMeshDrawEnabled on a mesh record whose first word is `*mesh`.
int32_t tpw_rat_is_mesh_draw_enabled(const uint32_t* mesh) {
    tag_sMesh record{};
    record.mF00 = mesh[0];
    return IsMeshDrawEnabled(&record);
}

int32_t tpw_rat_frame_data(sAnimTimeControl* control, void* keys, uint32_t count, uint32_t type, int32_t wrap,
                           uint32_t* first, uint32_t* second, float* fraction) {
    return GetAnimFrameData(control, keys, count, type, wrap, first, second, fraction);
}

void tpw_rat_texture_frames(tag_sM3D2Header* header, tag_sAnimHeader* animation, sAnimTimeControl* control) {
    RAAnimateTextureFrames(header, animation, control);
}

int32_t tpw_rat_animate_node(sAnimTimeControl* control, tag_sM3D2Header* header, tag_sAnimHeader* animation,
                             tag_sNode* node, tag_sNodeAnim* nodeAnim) {
    return AnimateNode(control, header, animation, node, nodeAnim);
}

// ---- a ride set (native pointers) around a set of controls
namespace {
tag_sRideAnimSet gSet;
tag_sMesh gMesh;
}  // namespace

// TickRideAnimSet: p = {whether there is a set, its flags, animation count, time control count,
// then three controls of fourteen words each}. out: the answer, then the fourteen words of the
// three controls.
int32_t tpw_rat_tick_set(const int64_t* p, int64_t* out) {
    Reader r{p};
    const bool present = r.next() != 0;
    std::memset(&gSet, 0xa5, sizeof gSet);
    gSet.mFlags = r.word();
    gSet.mAnimationCount = r.word();
    gSet.mTimeCount = static_cast<uint16_t>(r.next());
    auto* controls = static_cast<sAnimTimeControl*>(Block(3 * sizeof(sAnimTimeControl)));
    gSet.mTimeControls = controls;
    for (int c = 0; c < 3; ++c) {
        uint32_t words[14];
        for (uint32_t& w : words) w = r.word();
        std::memcpy(&controls[c], words, sizeof words);
        std::memset(reinterpret_cast<char*>(&controls[c]) + sizeof words, 0xa5, sizeof(sAnimTimeControl) - sizeof words);
    }
    tpw_cl_label(10, &gSet);
    const int32_t answer = TickRideAnimSet(present ? &gSet : nullptr);
    int64_t* at = out;
    *at++ = answer;
    for (int c = 0; c < 3; ++c) {
        uint32_t words[14];
        std::memcpy(words, &controls[c], sizeof words);
        for (uint32_t w : words) *at++ = w;
    }
    return static_cast<int32_t>(at - out);
}

// DoAnimateRideSet: p = {a, whether there is a set, the set's flags, its words at 0xc0 and 0xc4,
// the mesh's first word, whether it has an engine mesh, the header's address, the control's address,
// then twelve groups of {count, three animation addresses}}. The mesh is label 20, the engine mesh
// 60, the set 10. out: the set's flags and the mesh's first word.
int32_t tpw_rat_do_animate(const int64_t* p, int64_t* out) {
    Reader r{p};
    const int32_t a = static_cast<int32_t>(r.next());
    const bool present = r.next() != 0;
    std::memset(&gSet, 0xa5, sizeof gSet);
    std::memset(&gMesh, 0xa5, sizeof gMesh);
    gSet.mFlags = r.word();
    const uint32_t fc0 = r.word();
    const uint32_t fc4 = r.word();
    gSet.mFC0 = {static_cast<uint16_t>(fc0 >> 16), static_cast<uint16_t>(fc0)};
    gSet.mFC4 = {static_cast<uint16_t>(fc4 >> 16), static_cast<uint16_t>(fc4)};
    gMesh.mF00 = r.word();
    const bool hasEngine = r.next() != 0;
    gMesh.mEngineMesh = hasEngine ? &gEngine : nullptr;
    gMesh.mHeader = r.address<tag_sM3D2Header>();
    gSet.mHeads = &gMesh;
    auto* control = r.address<sAnimTimeControl>();
    static sRideAnimEntry entries[12][3];
    for (int k = 0; k < 12; ++k) {
        const uint32_t count = r.word();
        gSet.mGroups[k] = {0xa5a5a5a5, count, entries[k]};
        for (int j = 0; j < 3; ++j) entries[k][j] = {static_cast<uint32_t>(j), r.address<tag_sAnimHeader>()};
    }
    tpw_cl_label(10, &gSet);
    tpw_cl_label(20, &gMesh);
    tpw_cl_label(60, &gEngine);
    tpw_cl_label(4, control);
    DoAnimateRideSet(present ? &gSet : nullptr, control, a);
    out[0] = gSet.mFlags;
    out[1] = gMesh.mF00;
    return 2;
}

// ---- the embedded data
namespace {
sEmbeddedSet gEmbedded;
sEmbeddedRecord gRecords[4];
uint8_t gBlockStore[4][0x40];

// The records of a case: eight words each (flags, then the rest), all with a block.
void BuildRecords(Reader& r, int count) {
    for (int i = 0; i < count; ++i) {
        std::memset(&gRecords[i], 0xa5, sizeof gRecords[i]);
        gRecords[i].mFlags = r.word();
        gRecords[i].mF08 = r.word();
        gRecords[i].mF0C = r.word();
        gRecords[i].mF10 = r.word();
        std::memset(gBlockStore[i], 0xa5, sizeof gBlockStore[i]);
        gRecords[i].mBlock = gBlockStore[i];
    }
}

void DumpRecords(int count, int64_t*& out) {
    for (int i = 0; i < count; ++i) {
        *out++ = gRecords[i].mFlags;
        *out++ = gRecords[i].mF08;
        *out++ = gRecords[i].mF0C;
        *out++ = gRecords[i].mF10;
        uint32_t words[16];
        std::memcpy(words, gBlockStore[i], sizeof words);
        for (uint32_t w : words) *out++ = w;
    }
}
}  // namespace

// TPProcessEmbeddedData: p = {the mesh's first word, whether it has an embedded set, the set's
// flags, the header's address, the number of records (0 to 4), then four records of four words
// each}. The mesh is label 20, the embedded records have blocks. out: the records' words and
// their blocks (sixteen words each).
int32_t tpw_rat_process_embedded(const int64_t* p, int64_t* out) {
    Reader r{p};
    std::memset(&gMesh, 0xa5, sizeof gMesh);
    gMesh.mF00 = r.word();
    const bool hasSet = r.next() != 0;
    gEmbedded.mFlags = r.word();
    gMesh.mHeader = r.address<tag_sM3D2Header>();
    r.next();   // (the number of records the test says it builds: always 4)
    BuildRecords(r, 4);
    gEmbedded.mRecords = gRecords;
    gMesh.mF28 = hasSet ? &gEmbedded : nullptr;
    tpw_cl_label(20, &gMesh);
    TPProcessEmbeddedData(&gMesh);
    int64_t* at = out;
    DumpRecords(4, at);
    return static_cast<int32_t>(at - out);
}

// DoWorldTransformsForEmbeddedData: p = {the node's address, the header's address, the matrix by
// value (16 words), the world matrix (16 words), then four records of four words each}. out: as
// above.
int32_t tpw_rat_do_world(const int64_t* p, int64_t* out) {
    Reader r{p};
    auto* node = r.address<tag_sNode>();
    auto* header = r.address<tag_sM3D2Header>();
    uint32_t words[16];
    for (uint32_t& w : words) w = r.word();
    sMatrix matrix;
    std::memcpy(&matrix, words, sizeof matrix);
    for (uint32_t& w : words) w = r.word();
    sMatrix world;
    std::memcpy(&world, words, sizeof world);
    BuildRecords(r, 4);
    DoWorldTransformsForEmbeddedData(node, header, matrix, &world, gRecords);
    int64_t* at = out;
    DumpRecords(4, at);
    return static_cast<int32_t>(at - out);
}

}  // extern "C"
