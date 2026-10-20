// Test library for the ride and mesh teardown (unit w105; libtpw_unit_RideDestroy.dylib, tests/test_RideDestroy.py), main app:
//   src/RideDestroy.cpp     DestroyRideType, DestroyRideAnimSet, DestroyRideAnimSetInstance, DestroyMesh, DestroyMeshHeader,
//                           DestroyRideHeightMap, DestroySkirtVertices, DMFreeMesh, the two DMDisableMeshRender
//   src/CoasterDestroy.cpp  DestroyCoasterInstance, DestroyCoasterInstanceFromEntranceID, DestroyCoasterInstances
//
// Every callee outside the unit is a fake that records the same event on both sides: DisableMeshDraw (both overloads; from the case's
// queues they clear the draw flag as the real one does or not), DestroyEngineMesh, MReleaseFreeF (it frees nothing: the event carries
// the label of the block, so every block of the world is labelled), TPDestroyEmbeddedData, DestroyRideSign, CMesh::Close and the
// engine mesh's destructor (a real delete, whose "delete" event the call log records), the coaster destroyers, DestroyQueue and
// RemoveObjectInstanceFromTable. One entry point per family: a description of numbers in, a dump of numbers out, the events in the
// call log. The world of a case is built here (host) and in emulator memory at the Mac's layouts (the test module).
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "tb_mesh.h"

using namespace calllog;

// ---- globals of other files, defined here
tag_sRideAnimSet* gRideTypes[kMaxRideTypes + 1];
tag_sCoasterInstance* gCoasterInstances;
int32_t gCoasterInstanceCount;
uint32_t gObjectTableCount;
uint32_t gObjectTableFirstFree;
uint32_t gObjectTableLowest;
void* gObjectTable[0x100];

// Every fake records, besides its arguments, a hash of the whole world as it is at the call (two numbers: the set and its variant, or
// for the coasters the list), so that the order of the clears and counts against the calls shows in the log.
namespace {
int64_t Trace(int which);
tag_sCoasterInstance* gRelinkTargets[5];
}

// ---- the engine mesh
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() { ev("EngineMesh.dtor", {lab(this)}); }
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }
int32_t CMesh::Close() {
    ev("CMesh.Close", {lab(this), Trace(0), Trace(1)});
    return 0;
}

// ---- the fakes
// From the case's queues: whether the fake takes the mesh out of the draw lists as the real one does (clears flag 1).
void DisableMeshDraw(tag_sMesh* mesh) {
    ev("DisableMeshDraw", {lab(mesh), Trace(0), Trace(1)});
    if (ret("Disable", 0) == 1) mesh->mF00 &= ~1u;
}
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("DisableMeshDrawXZ", {lab(mesh), (static_cast<int64_t>(from.mF00) << 16) | from.mF02,
                             (static_cast<int64_t>(to.mF00) << 16) | to.mF02, Trace(0), Trace(1)});
    if (ret("DisableXZ", 0) == 1) mesh->mF00 &= ~1u;
}
void DestroyEngineMesh(tag_sMesh* mesh) { ev("DestroyEngineMesh", {lab(mesh), Trace(0), Trace(1)}); }
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block), Trace(0), Trace(1)});
    return nullptr;
}
void TPDestroyEmbeddedData(tag_sMesh* mesh) { ev("TPDestroyEmbeddedData", {lab(mesh), Trace(0), Trace(1)}); }
void DestroyRideSign(tag_sRideAnimSet* set) { ev("DestroyRideSign", {lab(set), Trace(0), Trace(1)}); }
// From the case's queue: the destroyer trashes the instance's next pointer.
void DestroyCoasterTrains(tag_sCoasterInstance* instance) {
    ev("DestroyCoasterTrains", {lab(instance), Trace(0), Trace(1)});
    if (ret("Poison", 0) == 1) instance->mNext = nullptr;
}
void DestroyCoasterTrack(tag_sCoasterInstance* instance) { ev("DestroyCoasterTrack", {lab(instance), Trace(0), Trace(1)}); }
void DestroyCoasterPylons(tag_sCoasterInstance* instance) { ev("DestroyCoasterPylons", {lab(instance), Trace(0), Trace(1)}); }
void DestroyQueue(sWraparoundQueue* queue) { ev("DestroyQueue", {lab(queue), Trace(0), Trace(1)}); }
// Which of the table's three counters each reference is (1 to 3, 0 for another), whether the array is the table, the number, and the
// counters' values at the call.
void RemoveObjectInstanceFromTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, void** table, uint32_t id) {
    auto which = [](const uint32_t& r) {
        return &r == &gObjectTableCount ? 1 : &r == &gObjectTableFirstFree ? 2 : &r == &gObjectTableLowest ? 3 : 0;
    };
    ev("RemoveObjectInstanceFromTable", {which(count), which(firstFree), which(lowest), table == gObjectTable, id, count, firstFree,
                                         lowest, gCoasterInstanceCount, Trace(0), Trace(1)});
    // From the case's queue: the list's head moves to one of the case's instances behind the caller's back.
    const int64_t relink = ret("Relink", 0);
    if (relink > 0) gCoasterInstances = gRelinkTargets[relink - 1];
}

namespace {

// Labels of the objects of a set world (the test module has the same table); a variant's labels add kVariant.
enum Label : int32_t {
    kSet = 1, kMesh = 2, kHeader = 3, kBlock14 = 4, kSkirt = 5, kHeaderBlock = 6, kEngine = 7, kTypeEngine = 8, kAllocation = 9,
    kNodes = 10, kNodeBlock = 11 /* 3 */, kAnimTable = 15, kAnim = 16 /* 3 */, kAnim44 = 19 /* 3 */, kEntryAlloc = 22, kHeaderArray = 23,
    kHeaderOf = 24 /* 3 */, kHeaderOfBlock = 27 /* 3 */, kHoard = 30, kHoardBlock = 31, kHeightMap = 32, kSelfAlloc = 33,
    kDm = 34, kDmEntries = 35, kDmAlloc = 36, kDmBlock20 = 37, kDmMesh = 38, kDmHeader = 39, kDmMeshAlloc = 40, kDmInfos = 41,
    kDmName = 42 /* 3 */, kShared = 45, kStray = 46,
    kVariant = 50,
};

struct Reader {
    const int64_t* p;
    int64_t next() { return *p++; }
};

std::vector<void*> gArenaBlocks;
std::vector<CMesh*> gEngines;

void* Zeroed(size_t bytes, int label) {
    void* block = tpw::ArenaAllocZeroed(bytes);
    gArenaBlocks.push_back(block);
    if (label > 0) tpw_cl_label(label, block);
    return block;
}
// An object of the world over 0xa5 bytes (the fields the case sets are the only ones the code may read).
template <class T>
T* Obj(int label) {
    T* object = static_cast<T*>(Zeroed(sizeof(T), label));
    std::memset(static_cast<void*>(object), 0xa5, sizeof(T));
    return object;
}
void* Blk(int label) { return Zeroed(16, label); }
CMesh* MakeEngine(int label) {
    CMesh* engine = new CMesh;
    gEngines.push_back(engine);
    tpw_cl_label(label, engine);
    return engine;
}
HFieldXZ XZ(int64_t word) { return HFieldXZ{static_cast<uint16_t>(word >> 16), static_cast<uint16_t>(word)}; }

struct MeshW {
    tag_sMesh* mesh = nullptr;
    tag_sM3D2Header* header = nullptr;
    tag_sNode* nodes = nullptr;
    int nodeCount = 0;
};
struct DmW {
    tag_sDMeshSet* dm = nullptr;
    tag_sMesh* mesh = nullptr;
    tag_sM3D2Header* header = nullptr;
    sTextureInfo* infos = nullptr;
    sDMeshShared* shared = nullptr;
    int texCount = 0;
};
struct SetW {
    tag_sRideAnimSet* set = nullptr;
    MeshW mesh;
    DmW dm;
    bool hasDm = false;
    tag_sAnimHeader* anims[3] = {};
    tag_sM3D2Header* headers[3] = {};
    sHoardingGeometry* hoard = nullptr;
};

// description (16 numbers): flags, engine, type engine, engine count, block14, skirt, header block, allocation, node count, root
// (-1 none, -2 a node outside the array, else the index), three node flags, three node blocks
void BuildMesh(Reader& r, int base, MeshW& w) {
    const int64_t flags = r.next(), engine = r.next(), typeEngine = r.next(), engineCount = r.next(), block14 = r.next();
    const int64_t skirt = r.next(), headerBlock = r.next(), allocation = r.next(), nodeCount = r.next(), root = r.next();
    int64_t nodeFlags[3], nodeBlock[3];
    for (int64_t& v : nodeFlags) v = r.next();
    for (int64_t& v : nodeBlock) v = r.next();
    w.mesh = Obj<tag_sMesh>(base + kMesh);
    w.header = Obj<tag_sM3D2Header>(base + kHeader);
    w.nodeCount = static_cast<int>(nodeCount);
    w.mesh->mF00 = static_cast<uint32_t>(flags);
    w.mesh->mHeader = w.header;
    w.mesh->mEngineMesh = engine != 0 ? MakeEngine(base + kEngine) : nullptr;
    w.mesh->mTypeEngineMesh = typeEngine != 0 ? MakeEngine(base + kTypeEngine) : nullptr;
    w.mesh->mEngineMeshes = static_cast<int32_t>(engineCount);
    w.mesh->mBlock14 = block14 != 0 ? Blk(base + kBlock14) : nullptr;
    w.mesh->mSkirtVertices = skirt != 0 ? Blk(base + kSkirt) : nullptr;
    w.mesh->mAllocation = allocation != 0 ? Blk(base + kAllocation) : nullptr;
    w.header->mNodeCount = static_cast<uint16_t>(nodeCount);
    w.header->mBlock = headerBlock != 0 ? Blk(base + kHeaderBlock) : nullptr;
    if (nodeCount > 0) {
        w.nodes = static_cast<tag_sNode*>(Zeroed(sizeof(tag_sNode) * nodeCount, base + kNodes));
        std::memset(static_cast<void*>(w.nodes), 0xa5, sizeof(tag_sNode) * nodeCount);
        for (int i = 0; i < nodeCount; ++i) {
            w.nodes[i].mFlags = static_cast<uint32_t>(nodeFlags[i]);
            w.nodes[i].mF98 = nodeBlock[i] != 0 ? static_cast<sTexture*>(Blk(base + kNodeBlock + i)) : nullptr;
        }
        w.header->mNodes = w.nodes;
    } else {
        w.header->mNodes = nullptr;
    }
    tag_sNode* stray = static_cast<tag_sNode*>(Zeroed(sizeof(tag_sNode), base + kStray));
    w.mesh->mRootNode = root == -1 ? nullptr : root == -2 ? stray : w.nodes + root;
}

// description (9 numbers): flags, entries, allocation, block 0x20, the shared count, texture infos, which have a name (bits), the
// mesh's flags, the mesh's allocation
void BuildDm(Reader& r, int base, DmW& w, bool build) {
    const int64_t flags = r.next(), entries = r.next(), allocation = r.next(), block20 = r.next(), shared = r.next();
    const int64_t texCount = r.next(), names = r.next(), meshFlags = r.next(), meshAllocation = r.next();
    if (!build) return;
    w.texCount = static_cast<int>(texCount);
    w.dm = Obj<tag_sDMeshSet>(base + kDm);
    w.mesh = Obj<tag_sMesh>(base + kDmMesh);
    w.header = Obj<tag_sM3D2Header>(base + kDmHeader);
    w.shared = Obj<sDMeshShared>(base + kShared);
    w.infos = static_cast<sTextureInfo*>(Zeroed(3 * sizeof(sTextureInfo), base + kDmInfos));
    std::memset(static_cast<void*>(w.infos), 0xa5, 3 * sizeof(sTextureInfo));
    for (int i = 0; i < 3; ++i) w.infos[i].mName = ((names >> i) & 1) != 0 ? static_cast<char*>(Blk(base + kDmName + i)) : nullptr;
    w.header->mTextureCount = static_cast<uint16_t>(texCount);
    w.header->mTextureInfos = w.infos;
    w.shared->mInstances = static_cast<uint32_t>(shared);
    w.mesh->mF00 = static_cast<uint32_t>(meshFlags);
    w.mesh->mHeader = w.header;
    w.mesh->mAllocation = meshAllocation != 0 ? Blk(base + kDmMeshAlloc) : nullptr;
    w.dm->mF00 = static_cast<uint32_t>(flags);
    w.dm->mEntries = entries != 0 ? static_cast<sDMeshEntry*>(Blk(base + kDmEntries)) : nullptr;
    w.dm->mAllocation = allocation != 0 ? Blk(base + kDmAlloc) : nullptr;
    w.dm->mBlock20 = block20 != 0 ? Blk(base + kDmBlock20) : nullptr;
    w.dm->mMesh = w.mesh;
    w.dm->mShared = w.shared;
}

// description (14 numbers, then the mesh's 16 and the dynamic set's 9): flags, animations, which animations exist (bits), table,
// header array, which headers have a block (bits), entry allocation, hoarding, its block, height map, self allocation, the two
// map places (as one word each), whether there is a dynamic set
void BuildSet(Reader& r, int base, SetW& w) {
    const int64_t flags = r.next(), animCount = r.next(), animMask = r.next(), table = r.next(), headerArray = r.next();
    const int64_t headerBlocks = r.next(), entryAlloc = r.next(), hoard = r.next(), hoardBlock = r.next(), heightMap = r.next();
    const int64_t selfAlloc = r.next(), fc0 = r.next(), fc4 = r.next(), hasDm = r.next();
    BuildMesh(r, base, w.mesh);
    BuildDm(r, base, w.dm, hasDm != 0);
    w.hasDm = hasDm != 0;
    w.set = Obj<tag_sRideAnimSet>(base + kSet);
    tag_sRideAnimSet* set = w.set;
    set->mFlags = static_cast<uint32_t>(flags);
    set->mHeads = w.mesh.mesh;
    set->mAnimationCount = static_cast<uint32_t>(animCount);
    if (table != 0) {
        auto** array = static_cast<tag_sAnimHeader**>(Zeroed(3 * sizeof(void*), base + kAnimTable));
        for (int i = 0; i < 3; ++i) {
            if (((animMask >> i) & 1) != 0) {
                w.anims[i] = Obj<tag_sAnimHeader>(base + kAnim + i);
                w.anims[i]->mF44 = Blk(base + kAnim44 + i);
            }
            array[i] = w.anims[i];
        }
        set->mAnimations = array;
    } else {
        set->mAnimations = nullptr;
    }
    if (headerArray != 0) {
        auto** array = static_cast<tag_sM3D2Header**>(Zeroed(3 * sizeof(void*), base + kHeaderArray));
        for (int i = 0; i < 3; ++i) {
            w.headers[i] = Obj<tag_sM3D2Header>(base + kHeaderOf + i);
            w.headers[i]->mBlock = ((headerBlocks >> i) & 1) != 0 ? Blk(base + kHeaderOfBlock + i) : nullptr;
            array[i] = w.headers[i];
        }
        set->mAnimationHeaders = array;
    } else {
        set->mAnimationHeaders = nullptr;
    }
    set->mEntryAllocation = entryAlloc != 0 ? static_cast<sRideAnimEntry*>(Blk(base + kEntryAlloc)) : nullptr;
    if (hoard != 0) {
        w.hoard = Obj<sHoardingGeometry>(base + kHoard);
        w.hoard->mBlock = hoardBlock != 0 ? Blk(base + kHoardBlock) : nullptr;
        set->mHoardingGeometry = w.hoard;
    } else {
        set->mHoardingGeometry = nullptr;
    }
    set->mDynamicMeshSet = w.hasDm ? w.dm.dm : nullptr;
    set->mHeightMap = heightMap != 0 ? Blk(base + kHeightMap) : nullptr;
    set->mFD0 = nullptr;
    set->mInstanceSelf = selfAlloc != 0 ? static_cast<tag_sRideAnimSet*>(Blk(base + kSelfAlloc)) : nullptr;
    set->mFC0 = XZ(fc0);
    set->mFC4 = XZ(fc4);
}

void Push(int64_t* out, int& n, int64_t value) { out[n++] = value; }

// The dump of a set's world after the call: the set's pointers as labels, the mesh, the nodes (flags and block), the animations' and
// headers' blocks, the hoarding block and the dynamic set (-5 for what the case did not build).
void Snapshot(const SetW& w, int64_t* out, int& n) {
    const tag_sRideAnimSet* set = w.set;
    Push(out, n, set->mFlags);
    Push(out, n, lab(set->mAnimations));
    Push(out, n, lab(set->mAnimationHeaders));
    Push(out, n, lab(set->mEntryAllocation));
    Push(out, n, lab(set->mDynamicMeshSet));
    Push(out, n, lab(set->mHoardingGeometry));
    Push(out, n, lab(set->mHeightMap));
    Push(out, n, lab(set->mFD0));
    Push(out, n, lab(set->mInstanceSelf));
    Push(out, n, w.hoard != nullptr ? lab(w.hoard->mBlock) : -5);
    const tag_sMesh* mesh = w.mesh.mesh;
    Push(out, n, mesh->mF00);
    Push(out, n, lab(mesh->mHeader));
    Push(out, n, lab(mesh->mBlock14));
    Push(out, n, lab(mesh->mSkirtVertices));
    Push(out, n, lab(mesh->mEngineMesh));
    Push(out, n, lab(mesh->mTypeEngineMesh));
    Push(out, n, lab(mesh->mAllocation));
    Push(out, n, mesh->mEngineMeshes);
    Push(out, n, lab(w.mesh.header->mBlock.get()));
    for (int i = 0; i < 3; ++i) {
        Push(out, n, i < w.mesh.nodeCount ? static_cast<int64_t>(w.mesh.nodes[i].mFlags) : -5);
        Push(out, n, i < w.mesh.nodeCount ? lab(w.mesh.nodes[i].mF98.get()) : -5);
    }
    for (int i = 0; i < 3; ++i) Push(out, n, w.anims[i] != nullptr ? lab(w.anims[i]->mF44.get()) : -5);
    for (int i = 0; i < 3; ++i) Push(out, n, w.headers[i] != nullptr ? lab(w.headers[i]->mBlock.get()) : -5);
    if (w.hasDm) {
        Push(out, n, w.dm.dm->mF00);
        Push(out, n, lab(w.dm.dm->mEntries));
        Push(out, n, lab(w.dm.dm->mBlock20));
        Push(out, n, lab(w.dm.dm->mAllocation));
        Push(out, n, lab(w.dm.dm->mMesh));
        Push(out, n, w.dm.shared->mInstances);
        Push(out, n, w.dm.mesh->mF00);
        Push(out, n, lab(w.dm.mesh->mAllocation));
        for (int i = 0; i < 3; ++i) Push(out, n, i < w.dm.texCount ? lab(w.dm.infos[i].mName.get()) : -5);
    } else {
        for (int i = 0; i < 11; ++i) Push(out, n, -5);
    }
}

const SetW* gTraceSets[2];
tag_sCoasterInstance* const* gTraceCoasters;

int64_t HashOf(const int64_t* values, int n) {
    uint64_t h = 0;
    for (int i = 0; i < n; ++i) h = (h * 1000003u + (static_cast<uint64_t>(values[i]) & 0xffffffffu)) & ((1ull << 61) - 1);
    return static_cast<int64_t>(h);
}

int64_t Trace(int which) {
    int64_t values[256];
    int n = 0;
    if (gTraceCoasters != nullptr) {
        if (which != 0) return 0;
        Push(values, n, gCoasterInstanceCount);
        Push(values, n, lab(gCoasterInstances));
        for (int i = 0; i < 5; ++i) Push(values, n, lab(gTraceCoasters[i]->mNext));
        Push(values, n, gObjectTableCount);
        Push(values, n, gObjectTableFirstFree);
        Push(values, n, gObjectTableLowest);
        return HashOf(values, n);
    }
    if (gTraceSets[which] == nullptr) return 0;
    Snapshot(*gTraceSets[which], values, n);
    return HashOf(values, n);
}

void Cleanup() {
    gTraceSets[0] = gTraceSets[1] = nullptr;
    gTraceCoasters = nullptr;
    const int on = tpw_cl_on;
    tpw_cl_on = 0;
    for (CMesh* engine : gEngines)
        if (lab(engine) > 0) delete engine;
    gEngines.clear();
    for (void* block : gArenaBlocks) tpw::ArenaFree(block);
    gArenaBlocks.clear();
    tpw_cl_on = on;
}

// description: op, three arguments (the mode of DestroyRideAnimSet, the type number of DestroyRideType, the instance call's heap and
// flags), whether the set has a "p" variant, the set (14 + 16 + 9 numbers), the variant (the same)
// ops: 0 DestroyMeshHeader (the set's mesh's header), 1 DestroyMesh, 2 DestroyRideHeightMap, 3 DestroySkirtVertices, 4 DMFreeMesh,
// 5 DMDisableMeshRender(set), 6 DMDisableMeshRender(set, places), 7 DestroyRideAnimSet, 8 DestroyRideAnimSetInstance, 9 DestroyRideType
// dump: the answer (1 for the functions that answer nothing here), then the set's snapshot and the variant's
int32_t RunSet(const int64_t* d, int64_t* out) {
    tpw_cl_on = 0;
    Reader r{d};
    const int64_t op = r.next(), mode = r.next(), type = r.next(), heap = r.next(), hasVariant = r.next();
    SetW world, variant;
    BuildSet(r, 0, world);
    BuildSet(r, kVariant, variant);
    if (hasVariant != 0) world.set->mFD0 = variant.set;
    gTraceSets[0] = &world;
    gTraceSets[1] = &variant;
    std::memset(gRideTypes, 0, sizeof gRideTypes);
    gRideTypes[type] = world.set;
    int64_t answer = 1;
    tpw_cl_on = 1;
    switch (op) {
    case 0: DestroyMeshHeader(world.mesh.header); break;
    case 1: DestroyMesh(world.mesh.mesh); break;
    case 2: DestroyRideHeightMap(world.set); break;
    case 3: DestroySkirtVertices(world.set); break;
    case 4: answer = DMFreeMesh(world.dm.dm); break;
    case 5: answer = DMDisableMeshRender(world.dm.dm); break;
    case 6: answer = DMDisableMeshRender(world.dm.dm, world.set->mFC0, world.set->mFC4); break;
    case 7: DestroyRideAnimSet(world.set, static_cast<int32_t>(mode)); break;
    case 8: DestroyRideAnimSetInstance(world.set, reinterpret_cast<sHeapInfo*>(static_cast<intptr_t>(heap)), static_cast<uint32_t>(mode)); break;
    case 9: DestroyRideType(static_cast<uint32_t>(type)); break;
    }
    tpw_cl_on = 0;
    int n = 0;
    Push(out, n, answer);
    Snapshot(world, out, n);
    Snapshot(variant, out, n);
    Cleanup();
    return n;
}

// ---- the coasters
// description: op (0 DestroyCoasterInstance of the target, 1 DestroyCoasterInstanceFromEntranceID, 2 DestroyCoasterInstances), the
// target (an index in 0..4, 4 is never in the list; or the entrance id), how many are linked, the count and the three table
// counters, then five pairs of (entrance id, object number)
// dump: the count, the head as a label, the five next pointers as labels, the table's counters
int32_t RunCoaster(const int64_t* d, int64_t* out) {
    tpw_cl_on = 0;
    Reader r{d};
    const int64_t op = r.next(), target = r.next(), linked = r.next();
    gCoasterInstanceCount = static_cast<int32_t>(r.next());
    gObjectTableCount = static_cast<uint32_t>(r.next());
    gObjectTableFirstFree = static_cast<uint32_t>(r.next());
    gObjectTableLowest = static_cast<uint32_t>(r.next());
    tag_sCoasterInstance* coaster[5];
    for (int i = 0; i < 5; ++i) {
        coaster[i] = Obj<tag_sCoasterInstance>(200 + 10 * i);
        tpw_cl_label(201 + 10 * i, &coaster[i]->mQueueB0);
        tpw_cl_label(202 + 10 * i, &coaster[i]->mQueueD0);
        coaster[i]->mEntranceID = static_cast<uint32_t>(r.next());
        coaster[i]->mObjectInstanceID = static_cast<uint32_t>(r.next());
        coaster[i]->mNext = nullptr;
    }
    for (int i = 0; i + 1 < linked; ++i) coaster[i]->mNext = coaster[i + 1];
    gCoasterInstances = linked > 0 ? coaster[0] : nullptr;
    gTraceCoasters = coaster;
    for (int i = 0; i < 5; ++i) gRelinkTargets[i] = coaster[i];
    tpw_cl_on = 1;
    if (op == 0) DestroyCoasterInstance(coaster[target]);
    if (op == 1) DestroyCoasterInstanceFromEntranceID(static_cast<uint32_t>(target));
    if (op == 2) DestroyCoasterInstances();
    tpw_cl_on = 0;
    int n = 0;
    Push(out, n, gCoasterInstanceCount);
    Push(out, n, lab(gCoasterInstances));
    for (int i = 0; i < 5; ++i) Push(out, n, lab(coaster[i]->mNext));
    Push(out, n, gObjectTableCount);
    Push(out, n, gObjectTableFirstFree);
    Push(out, n, gObjectTableLowest);
    Cleanup();
    return n;
}

}  // namespace

extern "C" {

int32_t tpw_rd_set(const int64_t* d, int64_t* out) { return RunSet(d, out); }
int32_t tpw_rd_coaster(const int64_t* d, int64_t* out) { return RunCoaster(d, out); }

}  // extern "C"
