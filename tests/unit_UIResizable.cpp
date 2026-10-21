// Test library for the resizable frame and slider draw objects and TbColour's constructor (libtpw_unit_UIResizable.dylib; see
// tests/test_UIResizable.py):
//
//   src/game/ui/InterfaceDrawEngine.cpp   InterfaceDrawEngineResizableFrame and ...Slider: InstanceMe and Change
//   src/game/ui/intrface.cpp                 TbColour::TbColour
//
// Fakes that record the same event on both sides: InstanceMesh, the engine mesh object's constructor (a new label, the mesh; it writes
// recognisable words in the frame fields), the engine mesh's CalculateFrame (records the flags and the change's frame words, answers the node
// the queue "CalculateFrame" names: 0 NULL, n the node n - 1 as mesh * 8 + node) and Change (records the flags, whether there is a record and
// the object's two rectangles at that moment). The draw object base class and the frame's and slider's constructors and destructors are
// fakes that record nothing (the frame's and slider's make the object of their class; the Python side reads the class by the vtable on the
// PowerPC side and by the dynamic type here).
//
// The world: up to 4 meshes (a tag_sMesh, a header and 0 to 3 nodes made in the arena; each node has 16 vertices in four sets and a box),
// 4 draw objects (0 and 1 frames, 2 and 3 sliders), the UI's heap and the objects InstanceMe makes. Everything is described to the library
// as arrays of 32-bit words (floats as their bits), the same words the PowerPC side writes into the emulator, and the state is dumped as
// arrays of words in an order the Python side repeats. Objects compare by label.
#include <cstdint>
#include <cstring>
#include <new>

#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"

using namespace calllog;

namespace {

constexpr int kMeshes = 4;
constexpr int kNodes = 3;
constexpr int kObjects = 4;
constexpr int kInstances = 8;

// The labels the Python side uses.
constexpr int kLabMesh = 10;      // + mesh
constexpr int kLabObject = 30;    // + slot
constexpr int kLabHeap = 50;
constexpr int kLabNode = 100;     // + mesh * 8 + node

struct World {
    tag_sMesh* mesh[kMeshes];
    tag_sM3D2Header* header[kMeshes];
    tag_sNode* node[kMeshes][kNodes];
    int nodeCount[kMeshes];
    InterfaceDrawEngineMesh* object[kObjects];
    InterfaceDrawObject* instance[kInstances];
    int instanceCount;
    char heap[16];   // what the UI's heap pointer points at (the type is not defined)
};

World gWorld;
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

// A NaN is one value.
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

}  // namespace

// ---- fakes and globals of the unit

sHeapInfo* gUIHeap;

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
InterfaceDrawObject* InterfaceDrawEngineMesh::InstanceMe() { return this; }

// The base class's CalculateFrame: records who, the flags and the change's frame and frame base; answers the node the queue names.
tag_sNode* InterfaceDrawEngineMesh::CalculateFrame(uint32_t flags, DOChange* change) {
    ev("CalculateFrame", {lab(this), flags, change != nullptr, change != nullptr ? change->mFrame : 0, change != nullptr ? change->mFrameBase : 0});
    const int64_t which = ret("CalculateFrame");
    if (which <= 0) return nullptr;
    const int m = static_cast<int>((which - 1) / 8);
    const int i = static_cast<int>((which - 1) % 8);
    if (m >= kMeshes || i >= gWorld.nodeCount[m]) return nullptr;
    return gWorld.node[m][i];
}

// The base class's Change: records who, the flags, whether there is a record and the object's two rectangles as they are now.
void InterfaceDrawEngineMesh::Change(uint32_t flags, DOChange* change) {
    ev("BaseChange", {lab(this), flags, change != nullptr, mRect.mLeft, mRect.mTop, mRect.mRight, mRect.mBottom, mRect2.mLeft});
    ev("BaseChange2", {mRect2.mTop, mRect2.mRight, mRect2.mBottom});
}

// The frame's and the slider's constructors and destructors (src/game/ui/InterfaceDrawEngine.cpp, not linked here): the base class's work only.
InterfaceDrawEngineResizableFrame::InterfaceDrawEngineResizableFrame(void* mesh) : InterfaceDrawEngineMesh(mesh) {}
InterfaceDrawEngineResizableFrame::~InterfaceDrawEngineResizableFrame() {}
InterfaceDrawEngineResizableSlider::InterfaceDrawEngineResizableSlider(void* mesh) : InterfaceDrawEngineMesh(mesh) {}
InterfaceDrawEngineResizableSlider::~InterfaceDrawEngineResizableSlider() {}

namespace {

void FreeWorld() {
    for (auto& object : gWorld.object) {
        delete object;
        object = nullptr;
    }
    for (int i = 0; i < gWorld.instanceCount; ++i) delete gWorld.instance[i];
    gWorld.instanceCount = 0;
    for (int m = 0; m < kMeshes; ++m) {
        for (int i = 0; i < gWorld.nodeCount[m]; ++i) {
            tpw::ArenaFree(gWorld.node[m][i]->mVertices.get());
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

// The class of an object: 1 the frame, 2 the slider, 0 any other.
uint32_t ClassOf(const InterfaceDrawObject* object) {
    if (dynamic_cast<const InterfaceDrawEngineResizableFrame*>(object) != nullptr) return 1;
    if (dynamic_cast<const InterfaceDrawEngineResizableSlider*>(object) != nullptr) return 2;
    return 0;
}

void DumpObject(const InterfaceDrawEngineMesh& object, uint32_t*& out) {
    *out++ = ClassOf(&object);
    *out++ = static_cast<uint32_t>(MeshRef(object.mData));
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
    for (int i = 0; i < 16; ++i) *out++ = Canon(object.mMatrix.m[i / 4][i % 4]);
    *out++ = static_cast<uint32_t>(NodeRef(object.mNodeFlags));
    *out++ = static_cast<uint32_t>(object.mLocalised);
}

// The change record: the frame, the frame base, the rectangle (4), whether it has one, whether there is a record at all (8 words).
struct ChangeRecord {
    DOChange change{};
    CoolRect rect;
    DOChange* pointer = nullptr;
};

void MakeChange(ChangeRecord& record, const uint32_t* w) {
    if (w[7] == 0) return;
    record.change.mFrame = static_cast<int16_t>(w[0]);
    record.change.mFrameBase = static_cast<int16_t>(w[1]);
    record.rect.mLeft = static_cast<int16_t>(w[2]);
    record.rect.mTop = static_cast<int16_t>(w[3]);
    record.rect.mRight = static_cast<int16_t>(w[4]);
    record.rect.mBottom = static_cast<int16_t>(w[5]);
    record.change.mRect = w[6] != 0 ? &record.rect : nullptr;
    record.pointer = &record.change;
}

InterfaceDrawEngineMesh* ObjectAt(int index) {
    if (index < kObjects) return gWorld.object[index];
    return index - kObjects < gWorld.instanceCount ? static_cast<InterfaceDrawEngineMesh*>(gWorld.instance[index - kObjects]) : nullptr;
}

}  // namespace

extern "C" {

// Forgets the last case's world. (Call before tpw_cl_reset, with the log off.)
void tpw_ur_reset() {
    FreeWorld();
    std::memset(&gWorld, 0, sizeof gWorld);
}

// Whether the UI has a heap.
void tpw_ur_globals(int32_t heapPresent) {
    gUIHeap = heapPresent != 0 ? reinterpret_cast<sHeapInfo*>(gWorld.heap) : nullptr;
    tpw_cl_label(kLabHeap, gWorld.heap);
}

// Mesh m with `count` nodes (0 to 3) and no header when `noHeader`; 6 + 48 words a node: the box, then the 16 vertices as x, y, z of each.
// Vertex i is block i / 4 (x[4], y[4], z[4]) lane i % 4.
void tpw_ur_mesh(int32_t m, int32_t count, int32_t noHeader, const uint32_t* words) {
    tag_sMesh* mesh = new tag_sMesh{};
    tag_sM3D2Header* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    gWorld.mesh[m] = mesh;
    gWorld.header[m] = header;
    gWorld.nodeCount[m] = count;
    mesh->mHeader = noHeader != 0 ? nullptr : header;
    tpw_cl_label(kLabMesh + m, mesh);
    for (int i = 0; i < count; ++i) {
        tag_sNode* node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        gWorld.node[m][i] = node;
        const uint32_t* w = words + 54 * i;
        for (int k = 0; k < 3; ++k) {
            node->mBox.mMin[k] = FromBits(w[k]);
            node->mBox.mMax[k] = FromBits(w[3 + k]);
        }
        sVertexPosnSet* sets = static_cast<sVertexPosnSet*>(tpw::ArenaAllocZeroed(4 * sizeof(sVertexPosnSet)));
        for (int v = 0; v < 16; ++v) {
            sets[v >> 2].x[v & 3] = FromBits(w[6 + 3 * v]);
            sets[v >> 2].y[v & 3] = FromBits(w[7 + 3 * v]);
            sets[v >> 2].z[v & 3] = FromBits(w[8 + 3 * v]);
        }
        node->mVertices = sets;
        tpw_cl_label(kLabNode + m * 8 + i, node);
    }
    header->mF78 = count > 0 ? gWorld.node[m][0] : nullptr;
}

// Draw object `slot` (a frame when `frame`, else a slider); 23 words: the mesh (-1: none), shown, frame base, frame, frame count, the two
// library words, rect (4), rect2 (4), the seven words 0x28 to 0x34.
void tpw_ur_object(int32_t slot, int32_t frame, const uint32_t* w) {
    const int32_t meshIndex = static_cast<int32_t>(w[0]);
    void* mesh = meshIndex >= 0 ? gWorld.mesh[meshIndex] : nullptr;
    gBuilding = true;
    InterfaceDrawEngineMesh* object = frame != 0 ? static_cast<InterfaceDrawEngineMesh*>(new InterfaceDrawEngineResizableFrame(mesh))
                                                 : static_cast<InterfaceDrawEngineMesh*>(new InterfaceDrawEngineResizableSlider(mesh));
    gBuilding = false;
    gWorld.object[slot] = object;
    tpw_cl_label(kLabObject + slot, object);
    object->mShown = static_cast<int16_t>(w[1]);
    object->mFrameBase = static_cast<int16_t>(w[2]);
    object->mFrame = static_cast<int16_t>(w[3]);
    object->mFrameCount = static_cast<int16_t>(w[4]);
    object->mInLibrary = static_cast<int16_t>(w[5]);
    object->mResizable = static_cast<int16_t>(w[6]);
    object->mRect.mLeft = static_cast<int16_t>(w[7]);
    object->mRect.mTop = static_cast<int16_t>(w[8]);
    object->mRect.mRight = static_cast<int16_t>(w[9]);
    object->mRect.mBottom = static_cast<int16_t>(w[10]);
    object->mRect2.mLeft = static_cast<int16_t>(w[11]);
    object->mRect2.mTop = static_cast<int16_t>(w[12]);
    object->mRect2.mRight = static_cast<int16_t>(w[13]);
    object->mRect2.mBottom = static_cast<int16_t>(w[14]);
    object->mF28 = static_cast<int16_t>(w[15]);
    object->mF2A = static_cast<int16_t>(w[16]);
    object->mF2C = static_cast<int16_t>(w[17]);
    object->mF2E = static_cast<int16_t>(w[18]);
    object->mF30 = static_cast<int16_t>(w[19]);
    object->mF32 = static_cast<int16_t>(w[20]);
    object->mDepth = static_cast<int16_t>(w[21]);
    std::memset(&object->mMatrix, 0, sizeof object->mMatrix);
    object->mNodeFlags = nullptr;
    object->mLocalised = static_cast<int32_t>(w[22]);
}

// One operation. 0 InstanceMe on draw object a (an object slot, or kObjects + the n-th instance); 1 Change with the flags and the record
// (8 words, see ChangeRecord). The answer: for InstanceMe 0 (NULL), 1 (the object itself) or n + 2 (the n-th instance, new); otherwise 0.
// -1: the operation was skipped (no such object, a slider on a mesh without a header, or flag 0x10 with a record that has no rectangle,
// which the original reads through NULL).
int64_t tpw_ur_op(int32_t op, int32_t a, uint32_t flags, const uint32_t* record) {
    const int64_t before[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    for (int64_t& blocks : gLastHeap) blocks = 0;
    int64_t answer = 0;
    InterfaceDrawEngineMesh* object = ObjectAt(a);
    if (object == nullptr) return -1;
    if (op == 0) {
        InterfaceDrawObject* result = object->InstanceMe();
        if (result == nullptr) answer = 0;
        else if (result == object) answer = 1;
        else {
            if (gWorld.instanceCount < kInstances) gWorld.instance[gWorld.instanceCount] = result;
            answer = gWorld.instanceCount + 2;
            if (gWorld.instanceCount < kInstances) ++gWorld.instanceCount;
        }
    } else {
        ChangeRecord change;
        MakeChange(change, record);
        if (change.pointer != nullptr && (flags & 0x10) != 0 && change.change.mRect == nullptr) return -1;
        if (ClassOf(object) == 2) {
            const tag_sMesh* mesh = static_cast<const tag_sMesh*>(object->mData);
            if (mesh == nullptr || mesh->mHeader == nullptr) return -1;
        }
        object->Change(flags, change.pointer);
    }
    for (int i = 0; i < 4; ++i) gLastHeap[i] = tpw_cl_heap[i] - before[i];
    return answer;
}

// The blocks the last operation handled (operator new, new[], delete, delete[]).
void tpw_ur_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = gLastHeap[i];
}

// The whole state in the order the Python side repeats: the nodes of the meshes (flags 0, box 6, 48 vertex words: 55 words a node), the draw
// objects (41 words), the instances (41 words).
int32_t tpw_ur_dump(uint32_t* out) {
    uint32_t* start = out;
    for (int m = 0; m < kMeshes; ++m) {
        for (int i = 0; i < gWorld.nodeCount[m]; ++i) {
            const tag_sNode* node = gWorld.node[m][i];
            for (int k = 0; k < 3; ++k) *out++ = Canon(node->mBox.mMin[k]);
            for (int k = 0; k < 3; ++k) *out++ = Canon(node->mBox.mMax[k]);
            for (int v = 0; v < 16; ++v) {
                const sVertexPosnSet& set = node->mVertices[v >> 2];
                *out++ = Canon(set.x[v & 3]);
                *out++ = Canon(set.y[v & 3]);
                *out++ = Canon(set.z[v & 3]);
            }
        }
    }
    for (int i = 0; i < kObjects; ++i)
        if (gWorld.object[i] != nullptr) DumpObject(*gWorld.object[i], out);
    for (int i = 0; i < gWorld.instanceCount; ++i) DumpObject(*static_cast<InterfaceDrawEngineMesh*>(gWorld.instance[i]), out);
    return static_cast<int32_t>(out - start);
}

// TbColour's constructor on raw storage of 8 bytes that all start as `fill`: the bytes afterwards.
void tpw_ur_colour(uint32_t fill, uint8_t* out) {
    alignas(TbColour) unsigned char storage[8];
    std::memset(storage, static_cast<int>(fill), sizeof storage);
    new (storage) TbColour();
    std::memcpy(out, storage, sizeof storage);
}

}  // extern "C"
