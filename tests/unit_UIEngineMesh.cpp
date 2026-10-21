// Test library for the engine mesh draw objects' instancing and changes (libtpw_unit_UIEngineMesh.dylib; see tests/test_UIEngineMesh.py):
//
//   src/game/TPMesh.cpp, src/game/ui/InterfaceDrawEngine.cpp   InterfaceDrawEngineMesh: InstanceMe, CalculateFrame, Change; the gradients' Change; the horizontal
//                                     gradient's CalcVerts; UpdateMeshLocalMatrix
//   src/game/LMatrix.cpp, src/game/ui/InterfaceDrawEngine.cpp                 GetUIMatrix, CombineMatricesNoWDest1st
//
// Real here: those, SetMatrixIdentity and VectorThroughLMatrixNoW (src/game/LMatrix.cpp), the rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp). Fakes that record the same
// event on both sides: InstanceMesh, the engine mesh's SetLocalMatrix, the engine mesh object's constructor (it writes the same recognisable
// words as the PowerPC side's fake), the vertical gradient's CalcVerts and CalcUVs. The draw object base class and the gradients' constructors,
// destructors and Show are fakes that record nothing (the unit never calls them).
//
// The world: up to 5 meshes (each a tag_sMesh, a header and a chain of up to 5 nodes made in the arena, with the nodes' flags, parents,
// matrices and boxes the test gives), 4 engine mesh draw objects, 3 gradients (horizontal or vertical), the UI's heap (a labelled block or NULL)
// and the objects InstanceMe makes. Everything is described to the library as arrays of 32-bit words (floats as their bits), the same words the
// PowerPC side writes into the emulator, and the state is dumped as arrays of words in an order the Python side repeats. Objects compare by label.
#include <cstdint>
#include <cstring>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "calllog.h"
#include "fake_strings.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"

using namespace calllog;

namespace {

constexpr int kMeshes = 5;
constexpr int kNodes = 5;
constexpr int kObjects = 4;
constexpr int kGradients = 3;
constexpr int kInstances = 8;

// The labels the Python side uses (tpw_uem_label in the test module).
constexpr int kLabMesh = 10;       // + mesh
constexpr int kLabEngine = 20;     // + mesh
constexpr int kLabObject = 30;     // + slot
constexpr int kLabGradient = 40;   // + slot
constexpr int kLabHeap = 50;
constexpr int kLabNode = 100;      // + mesh * 8 + node

struct World {
    tag_sMesh* mesh[kMeshes];
    tag_sM3D2Header* header[kMeshes];
    tag_sNode* node[kMeshes][kNodes];
    int nodeCount[kMeshes];
    InterfaceDrawEngineMesh* object[kObjects];
    InterfaceDrawEngineHGradient* horizontal[kGradients];
    InterfaceDrawEngineVGradient* vertical[kGradients];
    InterfaceDrawObject* instance[kInstances];
    int instanceCount;
    char heap[16];   // what the UI's heap pointer points at (the type is not defined)
};

World gWorld;
CMesh gEngine[kMeshes];
int64_t gLastHeap[4];
bool gBuilding = false;   // the test is making an object itself: the constructor records nothing

uint32_t Bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// A NaN is one value (the fused operations leave different NaN payloads and signs).
uint32_t Canon(float value) { return value != value ? 0x7fc00000u : Bits(value); }

uint32_t Short(int16_t value) { return static_cast<uint32_t>(static_cast<int32_t>(value)); }

int MeshRef(const void* pointer) {
    if (pointer == nullptr) return -1;
    for (int m = 0; m < kMeshes; ++m)
        if (gWorld.mesh[m] == pointer) return m;
    return -2;
}

int NodeRef(const void* pointer) {
    if (pointer == nullptr) return -1;
    for (int m = 0; m < kMeshes; ++m)
        for (int i = 0; i < gWorld.nodeCount[m]; ++i)
            if (gWorld.node[m][i] == pointer) return m * 8 + i;
    return -2;
}

void Label(int label, const void* object) { tpw_cl_label(label, object); }

}  // namespace

// ---- fakes and globals of the unit

TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;

CSystem::CSystem() {}
CSystem gei_System;

sHeapInfo* gUIHeap;

CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }

// The engine mesh's local matrix: who, the flags and the sixteen words (NaNs as one value).
void CMesh::SetLocalMatrix(int32_t flags, sMatrix& matrix) {
    uint32_t w[16];
    for (int i = 0; i < 16; ++i) w[i] = Canon(matrix.m[i / 4][i % 4]);
    ev("SetLocalMatrix", {lab(this), flags});
    ev("Matrix", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev("Matrix", {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}

// InstanceMesh: records who it was asked about and what the output held; answers 1. The queue "InstanceMesh" says which mesh (1 up) to put in
// the output (0: leave the output as it is).
int32_t InstanceMesh(tag_sMesh* mesh, sHeapInfo* heap, uint32_t flags, tag_sMesh** output, uint32_t mask) {
    ev("InstanceMesh", {lab(mesh), lab(heap), flags, mask, output != nullptr, output != nullptr ? lab(*output) : 0});
    const int64_t which = ret("InstanceMesh");
    if (output != nullptr && which > 0 && which <= kMeshes) *output = gWorld.mesh[which - 1];
    return 1;
}

// The draw object base class: the fields get zero and the virtual members do nothing (the unit never calls them).
InterfaceDrawObject::InterfaceDrawObject(void* data) : mData(data) {
    mShown = 0;
    mFrameBase = 0;
    mFrame = 0;
    mFrameCount = 0;
    mInLibrary = 0;
    mResizable = 0;
    mF28 = 0;
    mF2A = 0;
    mF2C = 0;
    mF2E = 0;
    mF30 = 0;
    mF32 = 0;
    mDepth = 0;
}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// The engine mesh object's constructor: records the new object (a new label) and the mesh, then sets every word to something InstanceMe's
// result can be told from (the frame words are overwritten by InstanceMe; nothing else is).
InterfaceDrawEngineMesh::InterfaceDrawEngineMesh(void* mesh) : InterfaceDrawObject(mesh) {
    if (!gBuilding) ev("ctor", {newlab(this), lab(mesh)});
    mShown = 0;
    mFrameBase = 0x1111;
    mFrame = 0x2222;
    mFrameCount = 0x3333;
    mInLibrary = 0;
    mResizable = 0;
    mRect.mLeft = mRect.mTop = mRect.mRight = mRect.mBottom = 0;
    mRect2.mLeft = mRect2.mTop = mRect2.mRight = mRect2.mBottom = 0;
    std::memset(&mMatrix, 0, sizeof mMatrix);
    mNodeFlags = nullptr;
    mLocalised = 0;
}
InterfaceDrawEngineMesh::~InterfaceDrawEngineMesh() {}
void InterfaceDrawEngineMesh::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawEngineMesh::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawEngineMesh::Show(int16_t) {}

// The gradients: constructors, destructors and Show are not the unit's and are never called; the vertical one's CalcVerts and CalcUVs are
// recorded (Change reaches them through the vtable).
InterfaceDrawEngineHGradient::InterfaceDrawEngineHGradient(void* mesh) : InterfaceDrawObject(mesh) {}
InterfaceDrawEngineHGradient::~InterfaceDrawEngineHGradient() {}
void InterfaceDrawEngineHGradient::Show(int16_t) {}
InterfaceDrawEngineVGradient::InterfaceDrawEngineVGradient(void* mesh) : InterfaceDrawObject(mesh) {}
InterfaceDrawEngineVGradient::~InterfaceDrawEngineVGradient() {}
void InterfaceDrawEngineVGradient::Show(int16_t) {}
void InterfaceDrawEngineVGradient::CalcVerts() { ev("VCalcVerts", {lab(this)}); }
void InterfaceDrawEngineVGradient::CalcUVs() { ev("VCalcUVs", {lab(this)}); }

namespace {

void FreeWorld() {
    for (auto& object : gWorld.object) {
        delete object;
        object = nullptr;
    }
    for (auto& object : gWorld.horizontal) {
        delete object;
        object = nullptr;
    }
    for (auto& object : gWorld.vertical) {
        delete object;
        object = nullptr;
    }
    for (int i = 0; i < gWorld.instanceCount; ++i) delete gWorld.instance[i];
    gWorld.instanceCount = 0;
    for (int m = 0; m < kMeshes; ++m) {
        for (int i = 0; i < gWorld.nodeCount[m]; ++i) {
            tpw::ArenaFree(gWorld.node[m][i]);
            gWorld.node[m][i] = nullptr;
        }
        gWorld.nodeCount[m] = 0;
        tpw::ArenaFree(gWorld.header[m]);
        gWorld.header[m] = nullptr;
        delete gWorld.mesh[m];
        gWorld.mesh[m] = nullptr;
    }
}

// The shared words of a draw object and a gradient (the first 22): the built flag or mesh, shown, frame base, frame, frame count, the two
// library words, both rectangles and the seven words 0x28 to 0x34.
void SetBase(InterfaceDrawObject& object, const uint32_t* w) {
    object.mShown = static_cast<int16_t>(w[1]);
    object.mFrameBase = static_cast<int16_t>(w[2]);
    object.mFrame = static_cast<int16_t>(w[3]);
    object.mFrameCount = static_cast<int16_t>(w[4]);
    object.mInLibrary = static_cast<int16_t>(w[5]);
    object.mResizable = static_cast<int16_t>(w[6]);
    object.mRect.mLeft = static_cast<int16_t>(w[7]);
    object.mRect.mTop = static_cast<int16_t>(w[8]);
    object.mRect.mRight = static_cast<int16_t>(w[9]);
    object.mRect.mBottom = static_cast<int16_t>(w[10]);
    object.mRect2.mLeft = static_cast<int16_t>(w[11]);
    object.mRect2.mTop = static_cast<int16_t>(w[12]);
    object.mRect2.mRight = static_cast<int16_t>(w[13]);
    object.mRect2.mBottom = static_cast<int16_t>(w[14]);
    object.mF28 = static_cast<int16_t>(w[15]);
    object.mF2A = static_cast<int16_t>(w[16]);
    object.mF2C = static_cast<int16_t>(w[17]);
    object.mF2E = static_cast<int16_t>(w[18]);
    object.mF30 = static_cast<int16_t>(w[19]);
    object.mF32 = static_cast<int16_t>(w[20]);
    object.mDepth = static_cast<int16_t>(w[21]);
}

void SetMatrix(sMatrix& matrix, const uint32_t* w) {
    for (int i = 0; i < 16; ++i) matrix.m[i / 4][i % 4] = FromBits(w[i]);
}

void DumpBase(const InterfaceDrawObject& object, uint32_t*& out) {
    *out++ = Short(object.mShown);
    *out++ = Short(object.mFrameBase);
    *out++ = Short(object.mFrame);
    *out++ = Short(object.mFrameCount);
    *out++ = Short(object.mInLibrary);
    *out++ = Short(object.mResizable);
    *out++ = Short(object.mRect.mLeft);
    *out++ = Short(object.mRect.mTop);
    *out++ = Short(object.mRect.mRight);
    *out++ = Short(object.mRect.mBottom);
    *out++ = Short(object.mRect2.mLeft);
    *out++ = Short(object.mRect2.mTop);
    *out++ = Short(object.mRect2.mRight);
    *out++ = Short(object.mRect2.mBottom);
    *out++ = Short(object.mF28);
    *out++ = Short(object.mF2A);
    *out++ = Short(object.mF2C);
    *out++ = Short(object.mF2E);
    *out++ = Short(object.mF30);
    *out++ = Short(object.mF32);
    *out++ = Short(object.mDepth);
}

void DumpMatrix(const sMatrix& matrix, uint32_t*& out) {
    for (int i = 0; i < 16; ++i) *out++ = Canon(matrix.m[i / 4][i % 4]);
}

// An engine mesh draw object: the mesh, the base's words, the matrix, the node, the localised word (40 words).
void DumpObject(const InterfaceDrawEngineMesh& object, uint32_t*& out) {
    *out++ = static_cast<uint32_t>(MeshRef(object.mData));
    DumpBase(object, out);
    DumpMatrix(object.mMatrix, out);
    *out++ = static_cast<uint32_t>(NodeRef(object.mNodeFlags));
    *out++ = static_cast<uint32_t>(object.mLocalised);
}

// A gradient: built, the base's words, the matrix, the box, the fraction, the mesh, the vertices and the texture coordinates (86 words).
template <class Gradient>
void DumpGradient(const Gradient& gradient, uint32_t*& out) {
    *out++ = gradient.mData != nullptr;
    DumpBase(gradient, out);
    DumpMatrix(gradient.mMatrix, out);
    for (float value : gradient.mBox) *out++ = Bits(value);
    *out++ = Canon(gradient.mFraction);
    *out++ = static_cast<uint32_t>(MeshRef(gradient.mMeshSet.mMesh));
    const float* vertices = reinterpret_cast<const float*>(gradient.mMeshSet.mVertexSets);
    for (int i = 0; i < 24; ++i) *out++ = Canon(vertices[i]);
    const float* uvs = reinterpret_cast<const float*>(gradient.mMeshSet.mUVSets);
    for (int i = 0; i < 16; ++i) *out++ = Canon(uvs[i]);
}

// The change record: word 0 the first word, then frame, depth, rotation, frame base, the two pivot words, the rectangle (4), whether it has
// one and whether there is a record at all (13 words).
struct ChangeRecord {
    DOChange change{};
    CoolRect rect;
    DOChange* pointer = nullptr;
};

void MakeChange(ChangeRecord& record, const uint32_t* w) {
    if (w[12] == 0) return;
    std::memcpy(record.change.mF00, &w[0], 4);
    record.change.mFrame = static_cast<int16_t>(w[1]);
    record.change.mDepth = static_cast<int16_t>(w[2]);
    record.change.mF08 = static_cast<int16_t>(w[3]);
    record.change.mFrameBase = static_cast<int16_t>(w[4]);
    record.change.mF0C = static_cast<int16_t>(w[5]);
    record.change.mF0E = static_cast<int16_t>(w[6]);
    record.rect.mLeft = static_cast<int16_t>(w[7]);
    record.rect.mTop = static_cast<int16_t>(w[8]);
    record.rect.mRight = static_cast<int16_t>(w[9]);
    record.rect.mBottom = static_cast<int16_t>(w[10]);
    record.change.mRect = w[11] != 0 ? &record.rect : nullptr;
    record.pointer = &record.change;
}

InterfaceDrawEngineMesh* ObjectAt(int index) {
    if (index < kObjects) return gWorld.object[index];
    return index - kObjects < gWorld.instanceCount ? static_cast<InterfaceDrawEngineMesh*>(gWorld.instance[index - kObjects]) : nullptr;
}

}  // namespace

extern "C" {

// Forgets the last case's world. (Call before tpw_cl_reset, with the log off.)
void tpw_uem_reset() {
    FreeWorld();
    std::memset(&gWorld, 0, sizeof gWorld);
}

// The globals a case sets: the system's flags, whether the UI has a heap, and TPGlobals' sine table (size, mask, scale bits, the entries).
void tpw_uem_globals(uint32_t systemFlags, int32_t heapPresent, uint32_t size, uint32_t mask, uint32_t scaleBits, const uint32_t* table,
                     uint32_t count) {
    gei_System.mFlags = systemFlags;
    gUIHeap = heapPresent != 0 ? reinterpret_cast<sHeapInfo*>(gWorld.heap) : nullptr;
    Label(kLabHeap, gWorld.heap);
    gTPGlobals.mSinTableSize = size;
    gTPGlobals.mSinTableMask = mask;
    gTPGlobals.mSinTableScale = FromBits(scaleBits);
    for (float& entry : gTPGlobals.mSinTable) entry = 0.0f;   // (the image's table is zero)
    for (uint32_t i = 0; i < count && i < 0x1000; ++i) gTPGlobals.mSinTable[i] = FromBits(table[i]);
}

// Mesh m with `count` nodes; 24 words a node: flags, the parent (a node of the same mesh before it, or -1), the matrix (16), the box (6).
void tpw_uem_mesh(int32_t m, int32_t count, const uint32_t* words) {
    tag_sMesh* mesh = new tag_sMesh{};
    tag_sM3D2Header* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    gWorld.mesh[m] = mesh;
    gWorld.header[m] = header;
    gWorld.nodeCount[m] = count;
    mesh->mHeader = header;
    mesh->mEngineMesh = &gEngine[m];
    Label(kLabMesh + m, mesh);
    Label(kLabEngine + m, &gEngine[m]);
    for (int i = 0; i < count; ++i) gWorld.node[m][i] = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
    for (int i = 0; i < count; ++i) {
        tag_sNode* node = gWorld.node[m][i];
        const uint32_t* w = words + 24 * i;
        node->mFlags = w[0];
        node->mF04 = static_cast<int32_t>(w[1]) >= 0 ? gWorld.node[m][w[1]] : nullptr;
        node->mF0C = i + 1 < count ? gWorld.node[m][i + 1] : nullptr;
        SetMatrix(node->mMatrix, w + 2);
        for (int k = 0; k < 3; ++k) {
            node->mBox.mMin[k] = FromBits(w[18 + k]);
            node->mBox.mMax[k] = FromBits(w[21 + k]);
        }
        Label(kLabNode + m * 8 + i, node);
    }
    header->mF78 = count > 0 ? gWorld.node[m][0] : nullptr;
}

// Draw object `slot`; 40 words: the mesh (-1: none), the base's 21 words (see SetBase), the matrix, the node (mesh * 8 + node, -1: none), the
// localised word.
void tpw_uem_object(int32_t slot, const uint32_t* w) {
    const int32_t meshIndex = static_cast<int32_t>(w[0]);
    gBuilding = true;
    InterfaceDrawEngineMesh* object = new InterfaceDrawEngineMesh(meshIndex >= 0 ? gWorld.mesh[meshIndex] : nullptr);
    gBuilding = false;
    gWorld.object[slot] = object;
    Label(kLabObject + slot, object);
    SetBase(*object, w);
    SetMatrix(object->mMatrix, w + 22);
    const int32_t nodeRef = static_cast<int32_t>(w[38]);
    object->mNodeFlags = nodeRef >= 0 ? reinterpret_cast<uint32_t*>(gWorld.node[nodeRef / 8][nodeRef % 8]) : nullptr;
    object->mLocalised = static_cast<int32_t>(w[39]);
}

// Gradient `slot`; 86 words: built, the base's 21, the matrix, the box (6), the fraction, the mesh, the vertices (24) and the texture
// coordinates (16). The arrays are made here, and are the gradient's own.
void tpw_uem_gradient(int32_t slot, int32_t horizontal, const uint32_t* w) {
    static float vertexStorage[kGradients][24];
    static float uvStorage[kGradients][16];
    const int32_t meshIndex = static_cast<int32_t>(w[45]);
    tag_sMesh* mesh = meshIndex >= 0 ? gWorld.mesh[meshIndex] : nullptr;
    for (int i = 0; i < 24; ++i) vertexStorage[slot][i] = FromBits(w[46 + i]);
    for (int i = 0; i < 16; ++i) uvStorage[slot][i] = FromBits(w[70 + i]);
    auto fill = [&](auto* gradient) {
        gradient->mData = w[0] != 0 ? reinterpret_cast<void*>(1) : nullptr;
        SetBase(*gradient, w);
        SetMatrix(gradient->mMatrix, w + 22);
        for (int i = 0; i < 6; ++i) gradient->mBox[i] = FromBits(w[38 + i]);
        gradient->mFraction = FromBits(w[44]);
        gradient->mMeshSet.mMesh = mesh;
        gradient->mMeshSet.mVertexSets = reinterpret_cast<sVertexPosnSet*>(vertexStorage[slot]);
        gradient->mMeshSet.mUVSets = reinterpret_cast<sUVSet*>(uvStorage[slot]);
        Label(kLabGradient + slot, gradient);
    };
    if (horizontal != 0) {
        gWorld.horizontal[slot] = new InterfaceDrawEngineHGradient(nullptr);
        fill(gWorld.horizontal[slot]);
    } else {
        gWorld.vertical[slot] = new InterfaceDrawEngineVGradient(nullptr);
        fill(gWorld.vertical[slot]);
    }
}

// One operation. 0 InstanceMe on draw object a (an object slot, or kObjects + the n-th instance); 1 CalculateFrame; 2 Change; 3 the horizontal
// gradient a's Change; 4 the vertical one's; 5 the horizontal one's CalcVerts; 6 UpdateMeshLocalMatrix on mesh a with the matrix in the record's
// first 16 words. The change record has 13 words (see ChangeRecord). The answer: for InstanceMe 0 (NULL), 1 (the object itself) or n + 2 (the
// n-th instance, new); for CalculateFrame the node (mesh * 8 + node) + 1, 0 for none; otherwise 0. -1: the operation was skipped (no such object,
// or a CalculateFrame with a frame flag on an object without a mesh or without a record, which the original would read through NULL).
int64_t tpw_uem_op(int32_t op, int32_t a, uint32_t flags, const uint32_t* record) {
    const int64_t before[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    for (int64_t& blocks : gLastHeap) blocks = 0;
    int64_t answer = 0;
    ChangeRecord change;
    if (op == 1 || op == 2 || op == 3 || op == 4) MakeChange(change, record);
    switch (op) {
    case 0: {
        InterfaceDrawEngineMesh* object = ObjectAt(a);
        if (object == nullptr) return -1;
        InterfaceDrawObject* result = object->InstanceMe();
        if (result == nullptr) answer = 0;
        else if (result == object) answer = 1;
        else {
            if (gWorld.instanceCount < kInstances) gWorld.instance[gWorld.instanceCount] = result;
            answer = gWorld.instanceCount + 2;
            if (gWorld.instanceCount < kInstances) ++gWorld.instanceCount;
        }
        break;
    }
    case 1: {
        InterfaceDrawEngineMesh* object = ObjectAt(a);
        if (object == nullptr) return -1;
        // (the original reads the mesh, and the record, through NULL)
        if ((object->mData == nullptr || change.pointer == nullptr) && (flags & 0x80001) != 0) return -1;
        answer = NodeRef(object->CalculateFrame(flags, change.pointer)) + 1;
        break;
    }
    case 2: {
        InterfaceDrawEngineMesh* object = ObjectAt(a);
        if (object == nullptr) return -1;
        object->Change(flags, change.pointer);
        break;
    }
    case 3:
        if (gWorld.horizontal[a] == nullptr) return -1;
        gWorld.horizontal[a]->Change(flags, change.pointer);
        break;
    case 4:
        if (gWorld.vertical[a] == nullptr) return -1;
        gWorld.vertical[a]->Change(flags, change.pointer);
        break;
    case 5:
        if (gWorld.horizontal[a] == nullptr) return -1;
        gWorld.horizontal[a]->CalcVerts();
        break;
    case 6: {
        sMatrix matrix;
        SetMatrix(matrix, record);
        UpdateMeshLocalMatrix(gWorld.mesh[a], &matrix);
        break;
    }
    }
    for (int i = 0; i < 4; ++i) gLastHeap[i] = tpw_cl_heap[i] - before[i];
    return answer;
}

// The blocks the last operation handled (operator new, new[], delete, delete[]).
void tpw_uem_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = gLastHeap[i];
}

// The whole state in the order the Python side repeats: the nodes of the meshes (flags, matrix, box: 23 words), the draw objects (40 words),
// the horizontal and vertical gradients of each slot that exist (86 words), the instances (40 words) and the system's flags.
int32_t tpw_uem_dump(uint32_t* out) {
    uint32_t* start = out;
    for (int m = 0; m < kMeshes; ++m) {
        for (int i = 0; i < gWorld.nodeCount[m]; ++i) {
            const tag_sNode* node = gWorld.node[m][i];
            *out++ = node->mFlags;
            for (int r = 0; r < 16; ++r) *out++ = Bits(node->mMatrix.m[r / 4][r % 4]);
            for (int k = 0; k < 3; ++k) *out++ = Bits(node->mBox.mMin[k]);
            for (int k = 0; k < 3; ++k) *out++ = Bits(node->mBox.mMax[k]);
        }
    }
    for (int i = 0; i < kObjects; ++i)
        if (gWorld.object[i] != nullptr) DumpObject(*gWorld.object[i], out);
    for (int i = 0; i < kGradients; ++i) {
        if (gWorld.horizontal[i] != nullptr) DumpGradient(*gWorld.horizontal[i], out);
        if (gWorld.vertical[i] != nullptr) DumpGradient(*gWorld.vertical[i], out);
    }
    for (int i = 0; i < gWorld.instanceCount; ++i) DumpObject(*static_cast<InterfaceDrawEngineMesh*>(gWorld.instance[i]), out);
    *out++ = gei_System.mFlags;
    return static_cast<int32_t>(out - start);
}

// The globals as the case left them: the system's flags, and the sine table's size, mask, scale and entries' bit sum.
void tpw_uem_get_globals(uint32_t* out) {
    out[0] = gei_System.mFlags;
    out[1] = gTPGlobals.mSinTableSize;
    out[2] = gTPGlobals.mSinTableMask;
    out[3] = Bits(gTPGlobals.mSinTableScale);
    uint32_t sum = 0;
    for (int i = 0; i < 0x1000; ++i) sum += Bits(gTPGlobals.mSinTable[i]);
    out[4] = sum;
}

// GetUIMatrix with the sine table the case set; the sixteen words of the matrix (NaNs as one value) go to `out`, which already holds the
// words the function must leave alone (the fourth column is checked by the caller).
void tpw_uem_get_matrix(uint32_t* out, int32_t left, int32_t bottom, int32_t width, int32_t height, uint32_t x, uint32_t y, uint32_t w,
                        uint32_t h, int32_t pivotX, int32_t pivotY, int32_t angle) {
    sMatrix matrix;
    std::memcpy(&matrix, out, sizeof matrix);
    GetUIMatrix(&matrix, static_cast<int16_t>(left), static_cast<int16_t>(bottom), static_cast<int16_t>(width), static_cast<int16_t>(height),
                FromBits(x), FromBits(y), FromBits(w), FromBits(h), static_cast<int16_t>(pivotX), static_cast<int16_t>(pivotY),
                static_cast<int16_t>(angle));
    for (int i = 0; i < 16; ++i) out[i] = Canon(matrix.m[i / 4][i % 4]);
}

// CombineMatricesNoWDest1st on two matrices of sixteen words each (NaNs as one value afterwards); `alias` makes the source the destination.
void tpw_uem_combine(uint32_t* destination, uint32_t* source, int32_t alias) {
    sMatrix d;
    sMatrix s;
    std::memcpy(&d, destination, sizeof d);
    std::memcpy(&s, source, sizeof s);
    CombineMatricesNoWDest1st(&d, alias != 0 ? &d : &s);
    for (int i = 0; i < 16; ++i) {
        destination[i] = Canon(d.m[i / 4][i % 4]);
        source[i] = Canon(s.m[i / 4][i % 4]);
    }
}

}  // extern "C"
