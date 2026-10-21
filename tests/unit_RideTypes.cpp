// Test library for src/game/rides/TPRide.cpp and src/game/rides/RideAnim.cpp (libtpw_unit_RideTypes.dylib; see
// tests/test_RideTypes.py): LoadRideType, InstanceRide, DestroyRideInstance, DestroyAllRideTypes,
// AnimateRideSet and TickRideAnims.
//
// Everything those six call is a fake that records its name, its arguments and the contents
// of the records it is given (tests/calllog.h); return values come from the case's queues.
// A case builds a small world of rides (a set, its mesh, header, nodes and engine mesh each),
// runs one function and dumps what the function could have changed. Objects whose address
// the code stores in a file-image field (a header's ride set) come from the arena.
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "Clock.h"
#include "Debug.h"
#include "Game.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "calllog.h"
#include "fake_strings.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}
CClock gClock;
CSystem::CSystem() {}
CSystem gei_System;
tag_sM3D2Header* gHeightfieldHeader;
uint32_t gGameOptions;
tag_sRideAnimSet* gHighlightedRide;
tag_sRideAnimSet* gPotentialHighlightedRide;
CRideDatabase gRideDatabase;

// The engine mesh is a CMesh whose only member the code uses is the callback.
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }

namespace {

enum { kRides = 6, kNodes = 3, kMinis = 2, kPoints = 8, kWindow = 40, kControls = 3, kGroups = 12, kEntries = 3 };
// Labels of the world's objects (ride i adds i; nodes 8 per ride, animations 36 per ride).
enum {
    kSetLabel = 10, kMeshLabel = 20, kHeaderLabel = 30, kRootLabel = 40, kSpecialLabel = 50, kEngineLabel = 60,
    kTypeLabel = 80, kFieldLabel = 90, kClockLabel = 4, kGameLabel = 6, kSizeLabel = 7, kObjectLabel = 8,
    kDatabaseLabel = 9, kNodeLabel = 100, kAnimLabel = 200
};

// The case's numbers, read in the order the test wrote them.
struct Reader {
    const int64_t* p;
    int64_t next() { return *p++; }
};

// One ride: its set, mesh, header and the records the functions touch.
struct Ride {
    tag_sRideAnimSet* set = nullptr;
    tag_sMesh* mesh = nullptr;
    tag_sM3D2Header* header = nullptr;
    tag_sNode* nodes = nullptr;
    tag_sMiniNode* minis = nullptr;
    tag_sNode* root = nullptr;
    tag_sNode* special = nullptr;
    s3DPoint* points = nullptr;
    CMesh* engine = nullptr;
    sAnimTimeControl* controls = nullptr;
    sRideAnimEntry* entries = nullptr;
    tag_sAnimHeader* anims[kGroups][kEntries] = {};
    uint8_t embedded = 0;
};
Ride gRides[kRides];

// The ride types of InstanceRide: types 1 to 3, and the "p" variant some of them have.
tag_sRideAnimSet gTypeSets[4];

// The height field InstanceRide reads: a header, its sHField and 20 heights (4 x 3 cells).
tag_sM3D2Header* gFieldHeader;
sHField* gField;
float* gHeights;

int gLoadCalls;                       // LoadRideAnimSet's calls so far in the case
std::string gLoadedName;              // what it leaves in the name buffer
TbDynamicStringTemplate<char> gThemeName;   // what CGame::GetCurrentThemeName answers
std::u16string gTypeName;             // what CRideDatabase::GetTypeName answers
int32_t gSizeInts[2];                 // the two ints LoadRideType's `a` points at
char gObject;                         // what `b`, a placement's object and the coaster data point at

int64_t Word(HFieldXZ value) { return (static_cast<int64_t>(value.mF00) << 16) | value.mF02; }
int64_t Number(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p)); }
float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
template <class T>
T* ArenaZero(size_t bytes = sizeof(T)) {
    return static_cast<T*>(tpw::ArenaAllocZeroed(bytes));
}

// The name of a control: its place among its set's controls.
int64_t ControlIndex(const tag_sRideAnimSet* set, const sAnimTimeControl* control) {
    return control - static_cast<const sAnimTimeControl*>(set->mTimeControls);
}

// Makes ride i's blocks once (arena blocks, since a header and a set point at each other) and
// gives them their labels (every case starts with none).
Ride& EnsureRide(int i) {
    Ride& ride = gRides[i];
    if (ride.set == nullptr) {
        ride.set = ArenaZero<tag_sRideAnimSet>();
        ride.mesh = ArenaZero<tag_sMesh>();
        ride.header = ArenaZero<tag_sM3D2Header>();
        ride.nodes = ArenaZero<tag_sNode>(kNodes * sizeof(tag_sNode));
        ride.minis = ArenaZero<tag_sMiniNode>(kMinis * sizeof(tag_sMiniNode));
        ride.root = ArenaZero<tag_sNode>();
        ride.special = ArenaZero<tag_sNode>();
        ride.points = ArenaZero<s3DPoint>(kPoints * sizeof(s3DPoint));
        ride.engine = new CMesh;
        ride.controls = ArenaZero<sAnimTimeControl>(kControls * sizeof(sAnimTimeControl));
        ride.entries = ArenaZero<sRideAnimEntry>(kGroups * kEntries * sizeof(sRideAnimEntry));
        for (int k = 0; k < kGroups; ++k) {
            for (int j = 0; j < kEntries; ++j) ride.anims[k][j] = ArenaZero<tag_sAnimHeader>();
        }
    }
    tpw_cl_label(kSetLabel + i, ride.set);
    tpw_cl_label(kMeshLabel + i, ride.mesh);
    tpw_cl_label(kHeaderLabel + i, ride.header);
    tpw_cl_label(kRootLabel + i, ride.root);
    tpw_cl_label(kSpecialLabel + i, ride.special);
    tpw_cl_label(kEngineLabel + i, ride.engine);
    for (int k = 0; k < kNodes; ++k) tpw_cl_label(kNodeLabel + 8 * i + k, &ride.nodes[k]);
    for (int k = 0; k < kGroups; ++k) {
        for (int j = 0; j < kEntries; ++j) tpw_cl_label(kAnimLabel + (i * kGroups + k) * kEntries + j, ride.anims[k][j]);
    }
    return ride;
}

// Builds ride i from 31 numbers (ride_words in the test): the set's flags and two map words,
// width, depth and the word at +0xcc; the mesh's words at 0, 0x10 and whether +0x28, a root
// node, an engine mesh are present (and the root's vertex count); the header's flags, node
// count, three node flags, all-node count and two mini node flags; the set's animation count,
// time control count and three controls (flags, kind, index). Everything else is 0xa5.
void BuildRide(int i, Reader& r) {
    Ride& ride = EnsureRide(i);
    std::memset(ride.set, 0xa5, sizeof(tag_sRideAnimSet));
    std::memset(ride.mesh, 0xa5, sizeof(tag_sMesh));
    std::memset(ride.header, 0xa5, sizeof(tag_sM3D2Header));
    std::memset(ride.nodes, 0xa5, kNodes * sizeof(tag_sNode));
    std::memset(ride.minis, 0xa5, kMinis * sizeof(tag_sMiniNode));
    std::memset(ride.root, 0xa5, sizeof(tag_sNode));
    std::memset(ride.special, 0xa5, sizeof(tag_sNode));
    std::memset(ride.controls, 0xa5, kControls * sizeof(sAnimTimeControl));
    std::memset(ride.points, 0, kPoints * sizeof(s3DPoint));
    ride.engine->mCallback = nullptr;

    tag_sRideAnimSet* set = ride.set;
    set->mSelf = set;
    set->mFlags = static_cast<uint32_t>(r.next());
    const uint32_t fc0 = static_cast<uint32_t>(r.next());
    const uint32_t fc4 = static_cast<uint32_t>(r.next());
    set->mFC0 = {static_cast<uint16_t>(fc0 >> 16), static_cast<uint16_t>(fc0)};
    set->mFC4 = {static_cast<uint16_t>(fc4 >> 16), static_cast<uint16_t>(fc4)};
    set->mFF0 = static_cast<int32_t>(r.next());
    set->mFF4 = static_cast<int32_t>(r.next());
    set->mFCC = static_cast<uint32_t>(r.next());
    set->mFD0 = nullptr;
    set->mHeads = ride.mesh;

    tag_sMesh* mesh = ride.mesh;
    mesh->mF00 = static_cast<uint32_t>(r.next());
    mesh->mF10 = static_cast<int32_t>(r.next());
    mesh->mF28 = r.next() != 0 ? &ride.embedded : nullptr;
    const bool hasRoot = r.next() != 0;
    const int64_t vertexCount = r.next();
    const bool hasEngine = r.next() != 0;
    mesh->mHeader = ride.header;
    mesh->mRootNode = hasRoot ? ride.root : nullptr;
    mesh->mF30 = nullptr;
    mesh->mEngineMesh = hasEngine ? ride.engine : nullptr;
    ride.root->mVertexCount = static_cast<uint16_t>(vertexCount);
    ride.root->mPoints = ride.points;

    tag_sM3D2Header* header = ride.header;
    header->mFlags = static_cast<uint32_t>(r.next());
    const int64_t nodeCount = r.next();
    header->mNodeCount = static_cast<uint16_t>(nodeCount);
    header->mNodes = ride.nodes;
    header->mMiniNodes = ride.minis;
    header->mF78 = ride.special;
    header->mRideSet.mOffset = 0;
    for (int k = 0; k < kNodes; ++k) ride.nodes[k].mFlags = static_cast<uint32_t>(r.next());
    header->mAllNodeCount = static_cast<uint16_t>(r.next());
    for (int k = 0; k < kMinis; ++k) ride.minis[k].mFlags = static_cast<uint32_t>(r.next());

    set->mAnimationCount = static_cast<uint32_t>(r.next());
    set->mTimeCount = static_cast<uint16_t>(r.next());
    set->mTimeControls = ride.controls;
    for (int c = 0; c < kControls; ++c) {
        ride.controls[c].mFlags = static_cast<uint32_t>(r.next());
        ride.controls[c].mF04 = static_cast<uint32_t>(r.next());
        ride.controls[c].mF08 = static_cast<uint32_t>(r.next());
    }
    for (int k = 0; k < kGroups; ++k) {
        set->mGroups[k] = {0xa5a5a5a5, kEntries, &ride.entries[k * kEntries]};
        for (int j = 0; j < kEntries; ++j) ride.entries[k * kEntries + j] = {static_cast<uint32_t>(j), ride.anims[k][j]};
    }
}

// The 16 numbers that say what a function did to ride i: the set's flags and words, the
// mesh's +0x30 (as a number of points past the root's, -1 when null), the header's flags,
// the nodes' and mini nodes' flags, the header's ride set and the engine mesh's callback
// (1 AnimateRideCallback, 0 none, 2 other, -1 no engine mesh).
void DumpRide(int i, int64_t*& out) {
    Ride& ride = gRides[i];
    const tag_sRideAnimSet* set = ride.set;
    *out++ = set->mFlags;
    *out++ = set->mFE4;
    *out++ = static_cast<uint32_t>(set->mFD4);
    *out++ = static_cast<uint32_t>(set->mFE8);
    *out++ = static_cast<uint32_t>(set->mFEC);
    *out++ = lab(set->mFD0);
    *out++ = set->mFCC;
    *out++ = ride.mesh->mF30 == nullptr ? -1 : ride.mesh->mF30 - ride.points;
    *out++ = ride.header->mFlags;
    for (int k = 0; k < kNodes; ++k) *out++ = ride.nodes[k].mFlags;
    for (int k = 0; k < kMinis; ++k) *out++ = ride.minis[k].mFlags;
    *out++ = lab(ride.header->mRideSet.get());
    if (ride.mesh->mEngineMesh == nullptr) *out++ = -1;
    else if (ride.engine->mCallback == AnimateRideCallback) *out++ = 1;
    else *out++ = ride.engine->mCallback == nullptr ? 0 : 2;
}

// The window of instance numbers: each entry 0 or a ride's number + 1.
void LoadWindow(Reader& r) {
    for (int s = 0; s < kWindow; ++s) {
        const int64_t v = r.next();
        gRideAnimSets[s] = v == 0 ? nullptr : EnsureRide(static_cast<int>(v - 1)).set;
    }
}

void DumpWindow(int64_t*& out) {
    for (int s = 0; s < kWindow; ++s) *out++ = lab(gRideAnimSets[s]);
}

// A 4 x 3 cell height field with heights from the case, as a header gHeightfieldHeader can
// point at: mode 0 none, 1 a header without a field, 2 the whole.
void BuildField(int64_t mode, int64_t width, const int64_t* heights) {
    if (gFieldHeader == nullptr) {
        gFieldHeader = ArenaZero<tag_sM3D2Header>();
        gField = ArenaZero<sHField>();
        gHeights = ArenaZero<float>(20 * sizeof(float));
    }
    tpw_cl_label(kFieldLabel, gFieldHeader);
    std::memset(gFieldHeader, 0xa5, sizeof(tag_sM3D2Header));
    std::memset(gField, 0xa5, sizeof(sHField));
    gFieldHeader->mHField = mode == 2 ? gField : nullptr;
    gField->mF18[0] = static_cast<uint32_t>(width);
    gField->mF18[1] = 3;
    gField->mHeights = gHeights;
    for (int k = 0; k < 20; ++k) gHeights[k] = FromBits(static_cast<uint32_t>(heights[k]));
    gHeightfieldHeader = mode == 0 ? nullptr : gFieldHeader;
}

}  // namespace

// ---- fakes: the clock
// The clock: records the call (and that it was gClock) and answers the case's queue.
int32_t CClock::Read() {
    ev("Read", {lab(this)});
    return static_cast<int32_t>(ret("Read"));
}
int32_t CClock::ReadFixed() {
    ev("ReadFixed", {lab(this)});
    return static_cast<int32_t>(ret("ReadFixed"));
}

// ---- fakes: the game and the ride database
// The game object answers the theme's name; the ride database the name of a type.
const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return gThemeName;
}
const char16_t* CRideDatabase::GetTypeName(uint16_t id) {
    ev("GetTypeName", {lab(this), id});
    return gTypeName.c_str();
}

// ---- fakes: trace output
// The trace and error boxes record their format (CTRACE also the one int the code gives it).
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {level, value}, {format});
}
void ERRORBox(const char* format, ...) { ev("ERRORBox", {}, {format}); }

// ---- fakes: ride types and the loader
// The loader's callees: recorded, with answers from the case where the code looks at them.
void DestroyRideType(uint32_t type) { ev("DestroyRideType", {type}); }
void BuildSharedTexturePath(char* out) {
    ev("BuildSharedTexturePath");
    std::strcpy(out, "Data:Fantasy:sharetex");
}
int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* sharedTextureFolder) {
    ev("SetupDirectory", {}, {file, folder, sharedTextureFolder});
    return static_cast<int32_t>(ret("Setup"));
}
// The first call makes ride 0's set, the second (into the set's +0xd0) ride 1's. It writes the set even
// when it answers 0 (the original's caller stores through its output regardless).
int32_t LoadRideAnimSet(char* name, uint32_t flags, tag_sRideAnimSet** output, char* loadedName, uint32_t timeCount) {
    const int call = gLoadCalls++;
    ev("LoadRideAnimSet", {call, flags, timeCount, loadedName != nullptr, output == &gRides[0].set->mFD0}, {name});
    if (loadedName != nullptr) std::strcpy(loadedName, gLoadedName.c_str());
    *output = gRides[call == 0 ? 0 : 1].set;
    return static_cast<int32_t>(ret("LoadSet"));
}
void CalculateSkirtVertices(tag_sRideAnimSet* set, tag_sM3D2Header* header, void* a, int32_t w, int32_t h) {
    ev("CalculateSkirtVertices", {lab(set), lab(header), lab(a), w, h});
}
void SetBaseBlendFlags(tag_sMesh* mesh, tag_sNode* node, int32_t on) {
    ev("SetBaseBlendFlags", {lab(mesh), lab(node), on});
}
void CorrectFaceFlags1(tag_sMesh* mesh, int32_t a) { ev("CorrectFaceFlags1", {lab(mesh), a}); }
void GenerateHoardingMeshes(const char* theme, tag_sRideAnimSet* set, void* b) {
    ev("GenerateHoardingMeshes", {lab(set), lab(b)}, {theme});
}
// The mesh pointer is passed by reference: the event names the set it is a member of.
void SetupRideHeads(tag_sMesh*& mesh) {
    auto* set = reinterpret_cast<tag_sRideAnimSet*>(reinterpret_cast<char*>(&mesh) - offsetof(tag_sRideAnimSet, mHeads));
    ev("SetupRideHeads", {lab(set), lab(mesh)});
}
void GetRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, char* path, void* a, int32_t flags) {
    ev("GetRideHeightMap", {lab(set), w, h, lab(a), flags}, {path});
}
int32_t GetFontPath(char* out) {
    ev("GetFontPath");
    std::strcpy(out, "fonts:Ride");
    return static_cast<int32_t>(ret("FontPath"));
}
int32_t GenerateRideSign(tag_sRideAnimSet* set, char* folder, char* name, char* fontPath, const char16_t* sign1,
                      const char16_t* sign2) {
    ev("GenerateRideSign", {lab(set)}, {folder, name, fontPath});
    const std::string first = narrow(sign1);
    const std::string second = narrow(sign2);
    ev("GenerateRideSignText", {}, {first.c_str(), second.c_str()});
    return 1;
}
void CorrectNodeFlags2(tag_sRideAnimSet* set) { ev("CorrectNodeFlags2", {lab(set)}); }

// ---- fakes: instances
// The instance fakes: InstanceRideAnimSet hands out ride 0's set when its answer is 1.
int32_t InstanceRideAnimSet(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags, tag_sRideAnimSet** output,
                            uint32_t timeCount) {
    ev("InstanceRideAnimSet", {lab(set), Number(heap), flags, timeCount});
    const int32_t result = static_cast<int32_t>(ret("InstanceSet", 1));
    if (result == 1) *output = gRides[0].set;
    return result;
}
int32_t InitRideAnimSet(tag_sRideAnimSet* set, int32_t init, int32_t mode, int32_t keepNodes) {
    ev("InitRideAnimSet", {lab(set), init, mode, keepNodes});
    return 1;
}
int32_t IsValidAnimation(tag_sRideAnimSet*& set, uint32_t a, uint32_t b) {
    ev("IsValidAnimation", {lab(set), a, b});
    return static_cast<int32_t>(ret("Valid"));
}
void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
}
// Moves the cell by the case's offsets and reports the case's two outputs.
void CorrectRidePosnForRotation(int32_t* x, int32_t* z, int32_t rotation, int32_t* outX, int32_t* outZ, int32_t w,
                                int32_t h) {
    ev("CorrectRidePosnForRotation", {*x, *z, rotation, w, h});
    *x += static_cast<int32_t>(ret("PosDx"));
    *z += static_cast<int32_t>(ret("PosDz"));
    *outX = static_cast<int32_t>(ret("OutA"));
    *outZ = static_cast<int32_t>(ret("OutB"));
}
float GetRideRequiredYOffset(int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags, int32_t rotation) {
    ev("GetRideRequiredYOffset", {x, z, lab(a), w, h, flags, rotation});
    return FromBits(static_cast<uint32_t>(ret("YOffset")));
}
void SetRidePositionAndRotation(tag_sRideAnimSet* set, int32_t outX, int32_t outZ, int32_t x, int32_t z,
                                int32_t rotation, float y, uint32_t flags, int32_t w, int32_t h) {
    ev("SetRidePositionAndRotation", {lab(set), outX, outZ, x, z, rotation, fbits(y), flags, w, h});
}
void DoMeshSkirtDeform(tag_sMesh* mesh, tag_sM3D2Header* header, float y, uint32_t a) {
    ev("DoMeshSkirtDeform", {lab(mesh), lab(header), fbits(y), a});
}
void GetBlendedBaseTextures(tag_sRideAnimSet* set) { ev("GetBlendedBaseTextures", {lab(set)}); }
void SetWorldBoundingArea(tag_sM3D2Header* a, tag_sM3D2Header* b) { ev("SetWorldBoundingArea", {lab(a), lab(b)}); }
void SetHoardingYOffsets(tag_sRideAnimSet* set) { ev("SetHoardingYOffsets", {lab(set)}); }
void CalculateRootNodeFaceNormals(tag_sNode* node, int32_t a, int32_t& result) {
    ev("CalculateRootNodeFaceNormals", {lab(node), a});
    result = static_cast<int32_t>(ret("Normals"));
}
void EnableMeshDraw(tag_sMesh* mesh) { ev("EnableMeshDraw", {lab(mesh)}); }
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("EnableMeshDrawRange", {lab(mesh), Word(from), Word(to)});
}
void TPProcessEmbeddedData(tag_sMesh* mesh) { ev("TPProcessEmbeddedData", {lab(mesh)}); }
int32_t IsCoasterType(uint32_t type) {
    ev("IsCoasterType", {type});
    return static_cast<int32_t>(ret("Coaster"));
}
uint32_t InstanceCoaster(uint32_t type, uint32_t instance, uint32_t x, uint32_t z, int32_t a, void* b) {
    ev("InstanceCoaster", {type, instance, x, z, a, lab(b)});
    return 0;
}
void OverrideActiveChannels(tag_sRideAnimSet* set) { ev("OverrideActiveChannels", {lab(set)}); }
void StartFlashingObject(tag_sRideAnimSet* set) { ev("StartFlashingObject", {lab(set)}); }
void RecalculateInvalidFaceNormals(tag_sM3D2Header* header) { ev("RecalculateInvalidFaceNormals", {lab(header)}); }
void AnimateRideCallback(tag_sM3D2Header*) { ev("AnimateRideCallback"); }

// ---- fakes: taking an instance away
// Taking an instance away.
void UnhighlightRide() { ev("UnhighlightRide"); }
int32_t IsRideFirstPersonRide(uint32_t instance) {
    ev("IsRideFirstPersonRide", {instance});
    return static_cast<int32_t>(ret("FirstPerson"));
}
void StopRidingRideIn1stPerson() { ev("StopRidingRideIn1stPerson"); }
void StopFlashingObject(tag_sRideAnimSet* set) { ev("StopFlashingObject", {lab(set)}); }
void DestroyCoasterInstanceFromEntranceID(uint32_t instance) { ev("DestroyCoasterInstanceFromEntranceID", {instance}); }
void DestroyRideAnimSetInstance(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags) {
    ev("DestroyRideAnimSetInstance", {lab(set), Number(heap), flags});
}

// ---- fakes: animating a set
// Animating a set: a control is named by its place among the set's controls.
int32_t TickRideAnimSet(tag_sRideAnimSet* set) {
    ev("TickRideAnimSet", {lab(set)});
    return static_cast<int32_t>(ret("Tick"));
}
void ResetRideAnimationSingleChannel(tag_sRideAnimSet* set, tag_sAnimHeader* animation, tag_sAnimHeader* other) {
    ev("ResetRideAnimationSingleChannel", {lab(set), lab(animation), lab(other)});
}
void DoAnimateRideSet(tag_sRideAnimSet* set, sAnimTimeControl* control, int32_t a) {
    ev("DoAnimateRideSet", {lab(set), ControlIndex(set, control), a});
}
void RecalculateInvalidNodeFaceNormals(tag_sNode* node) { ev("RecalculateInvalidNodeFaceNormals", {lab(node)}); }

extern "C" {

// TickRideAnims: p = {the word at 0x4010 before}; the clock's two answers are queued.
// out: the words at 0x4010, 0x4014 (as bits) and 0x4018 after.
int32_t tpw_rt_tick(const int64_t* p, int64_t* out) {
    tpw_cl_label(kClockLabel, &gClock);
    gTPGlobals.mF4010 = static_cast<uint32_t>(p[0]);
    gTPGlobals.mF4014 = 0.0f;
    gTPGlobals.mF4018 = 0;
    TickRideAnims();
    out[0] = gTPGlobals.mF4010;
    out[1] = fbits(gTPGlobals.mF4014);
    out[2] = gTPGlobals.mF4018;
    return 3;
}

// DestroyAllRideTypes: p = {the number of types}. out: the number after.
int32_t tpw_rt_destroy_types(const int64_t* p, int64_t* out) {
    gRideTypeCount = static_cast<uint32_t>(p[0]);
    DestroyAllRideTypes();
    out[0] = gRideTypeCount;
    return 1;
}

// DestroyRideInstance: p = {instance, first free, count A, count B, highlighted ride + 1,
// potential ride + 1, 40 slots, the flags of the six rides' sets}. out: the three counters,
// the two globals as labels, the slots as labels.
int32_t tpw_rt_destroy_instance(const int64_t* p, int64_t* out) {
    Reader r{p};
    const uint32_t instance = static_cast<uint32_t>(r.next());
    gRideFirstFree = static_cast<uint32_t>(r.next());
    gRideCountA = static_cast<uint32_t>(r.next());
    gRideCountB = static_cast<uint32_t>(r.next());
    const int64_t highlighted = r.next();
    const int64_t potential = r.next();
    for (int i = 0; i < kRides; ++i) EnsureRide(i);
    gHighlightedRide = highlighted == 0 ? nullptr : gRides[highlighted - 1].set;
    gPotentialHighlightedRide = potential == 0 ? nullptr : gRides[potential - 1].set;
    LoadWindow(r);
    for (int i = 0; i < kRides; ++i) gRides[i].set->mFlags = static_cast<uint32_t>(r.next());
    DestroyRideInstance(instance);
    int64_t* at = out;
    *at++ = gRideFirstFree;
    *at++ = gRideCountA;
    *at++ = gRideCountB;
    *at++ = lab(gHighlightedRide);
    *at++ = lab(gPotentialHighlightedRide);
    DumpWindow(at);
    gHighlightedRide = nullptr;
    gPotentialHighlightedRide = nullptr;
    std::memset(gRideAnimSets, 0, kWindow * sizeof gRideAnimSets[0]);
    return static_cast<int32_t>(at - out);
}

// AnimateRideSet: p = {the second argument, the flags, whether there is a set, the system's
// flags, then ride 0's 31 numbers}. out: the set's flags, the header's flags, the three nodes'
// and two mini nodes' flags, the system's flags.
int32_t tpw_rt_animate(const int64_t* p, int64_t* out) {
    Reader r{p};
    const int32_t a = static_cast<int32_t>(r.next());
    const uint32_t flags = static_cast<uint32_t>(r.next());
    const bool present = r.next() != 0;
    gei_System.mFlags = static_cast<uint32_t>(r.next());
    BuildRide(0, r);
    AnimateRideSet(present ? gRides[0].set : nullptr, a, flags);
    Ride& ride = gRides[0];
    int64_t* at = out;
    *at++ = ride.set->mFlags;
    *at++ = ride.header->mFlags;
    for (int k = 0; k < kNodes; ++k) *at++ = ride.nodes[k].mFlags;
    for (int k = 0; k < kMinis; ++k) *at++ = ride.minis[k].mFlags;
    *at++ = gei_System.mFlags;
    *at++ = 1;   // (the PowerPC side puts here whether the original's answer was 1, or 0 for no set)
    return static_cast<int32_t>(at - out);
}

// InstanceRide: p = {type, flags, whether there are params, x, z, rotation, whether the
// params have an object, the time control count, user value, whether c is set, d, e, slot,
// g, first free, count A, count B, game options, height field mode, the grid's width, 20
// height words, whether types 1 to 3 exist, whether they have a variant, 40 slots, then the
// instance's 31 numbers (ride 0)}. out: the answer, the counters, the slots and ride 0's 16.
int32_t tpw_rt_instance(const int64_t* p, int64_t* out) {
    Reader r{p};
    const uint32_t type = static_cast<uint32_t>(r.next());
    const uint32_t flags = static_cast<uint32_t>(r.next());
    const bool hasParams = r.next() != 0;
    sFixedRideParams params{};
    params.mX = static_cast<int32_t>(r.next());
    params.mZ = static_cast<int32_t>(r.next());
    params.mRotation = static_cast<int32_t>(r.next());
    params.mF0C = r.next() != 0 ? &gObject : nullptr;
    const uint32_t a = static_cast<uint32_t>(r.next());
    const int32_t b = static_cast<int32_t>(r.next());
    void* c = r.next() != 0 ? &gSizeInts : nullptr;
    const int32_t d = static_cast<int32_t>(r.next());
    const int32_t e = static_cast<int32_t>(r.next());
    const uint32_t slot = static_cast<uint32_t>(r.next());
    const int32_t g = static_cast<int32_t>(r.next());
    gRideFirstFree = static_cast<uint32_t>(r.next());
    gRideCountA = static_cast<uint32_t>(r.next());
    gRideCountB = static_cast<uint32_t>(r.next());
    gGameOptions = static_cast<uint32_t>(r.next());
    const int64_t mode = r.next();
    const int64_t width = r.next();
    int64_t heights[20];
    for (auto& h : heights) h = r.next();
    BuildField(mode, width, heights);
    bool exists[3];
    for (bool& x : exists) x = r.next() != 0;
    for (int k = 0; k < 3; ++k) {
        std::memset(&gTypeSets[k], 0xa5, sizeof gTypeSets[k]);
        const bool variant = r.next() != 0;
        gTypeSets[k].mFD0 = variant ? &gTypeSets[3] : nullptr;
        gRideTypes[k + 1] = exists[k] ? &gTypeSets[k] : nullptr;
        tpw_cl_label(kTypeLabel + k, &gTypeSets[k]);
    }
    std::memset(&gTypeSets[3], 0xa5, sizeof gTypeSets[3]);
    tpw_cl_label(kTypeLabel + 3, &gTypeSets[3]);
    LoadWindow(r);
    BuildRide(0, r);
    tpw_cl_label(kSizeLabel, gSizeInts);
    tpw_cl_label(kObjectLabel, &gObject);
    const uint32_t answer = InstanceRide(type, flags, hasParams ? &params : nullptr, a, b, c, d, e, slot, g);
    int64_t* at = out;
    *at++ = answer;
    *at++ = gRideFirstFree;
    *at++ = gRideCountA;
    *at++ = gRideCountB;
    DumpWindow(at);
    DumpRide(0, at);
    for (int k = 1; k < 4; ++k) gRideTypes[k] = nullptr;
    gHeightfieldHeader = nullptr;
    std::memset(gRideAnimSets, 0, kWindow * sizeof gRideAnimSets[0]);
    return static_cast<int32_t>(at - out);
}

// LoadRideType: p = {flags, id, the time control count, d, e, whether `a` is given and its two
// ints, whether `b` is given, whether there are signs, the types loaded before, whether the
// height field exists, then the 31 numbers of ride 0 (the set) and of ride 1 (its "p" variant)}.
// The queues give the three callees' answers. out: the answer, the type count, the type's set
// as a label, ride 0's flags, number, width, depth, variant and word at 0xcc, and ride 1's word.
int32_t tpw_rt_load(const int64_t* p, const char* folder, const char* name, const char* loaded, const char* sign1,
                    const char* sign2, const char* theme, const char* typeName, int64_t* out) {
    Reader r{p};
    const uint32_t flags = static_cast<uint32_t>(r.next());
    const uint32_t id = static_cast<uint32_t>(r.next());
    const uint32_t c = static_cast<uint32_t>(r.next());
    const int32_t d = static_cast<int32_t>(r.next());
    const int32_t e = static_cast<int32_t>(r.next());
    const bool hasA = r.next() != 0;
    gSizeInts[0] = static_cast<int32_t>(r.next());
    gSizeInts[1] = static_cast<int32_t>(r.next());
    const bool hasB = r.next() != 0;
    const bool hasSigns = r.next() != 0;
    gRideTypeCount = static_cast<uint32_t>(r.next());
    const int64_t mode = r.next();
    int64_t heights[20] = {};
    BuildField(mode == 0 ? 0 : 2, 4, heights);
    BuildRide(0, r);
    BuildRide(1, r);
    std::memset(gRideTypes, 0, 16 * sizeof gRideTypes[0]);
    gLoadCalls = 0;
    gLoadedName = loaded;
    gThemeName = theme;
    gTypeName.assign(typeName, typeName + std::strlen(typeName));
    const std::u16string first(sign1, sign1 + std::strlen(sign1));
    const std::u16string second(sign2, sign2 + std::strlen(sign2));
    tpw_cl_label(kSizeLabel, gSizeInts);
    tpw_cl_label(kObjectLabel, &gObject);
    tpw_cl_label(kGameLabel, &gGame);
    tpw_cl_label(kDatabaseLabel, &gRideDatabase);
    const uint32_t answer = LoadRideType(const_cast<char*>(folder), const_cast<char*>(name), hasA ? gSizeInts : nullptr,
                                         hasB ? &gObject : nullptr, flags, id, c, hasSigns ? first.c_str() : nullptr,
                                         hasSigns ? second.c_str() : nullptr, d, e);
    int64_t* at = out;
    *at++ = answer;
    *at++ = gRideTypeCount;
    *at++ = gRideTypeCount < 16 ? lab(gRideTypes[gRideTypeCount]) : -2;
    const tag_sRideAnimSet* set = gRides[0].set;
    *at++ = set->mFlags;
    *at++ = set->mFF8;
    *at++ = static_cast<uint32_t>(set->mFF0);
    *at++ = static_cast<uint32_t>(set->mFF4);
    *at++ = lab(set->mFD0);
    *at++ = set->mFCC;
    *at++ = gRides[1].set->mFCC;
    gHeightfieldHeader = nullptr;
    std::memset(gRideTypes, 0, 16 * sizeof gRideTypes[0]);
    return static_cast<int32_t>(at - out);
}

}  // extern "C"
