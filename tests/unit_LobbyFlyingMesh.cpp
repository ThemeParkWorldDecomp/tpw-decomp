// Test library for src/lobby_flyingmesh.cpp (libtpw_unit_LobbyFlyingMesh.dylib; see
// tests/test_LobbyFlyingMesh.py): the lobby's flying meshes, driven by scripts over a world the
// test builds, with the lobby's mesh system for real underneath.
//
// Real: the flying mesh system's constructor, destructor, Close, WindowMessagesFn, Register,
// UnRegister and DeleteFlyingMeshGroupListEntries; the groups and the instances; the two lists
// (TbTList's members); CLBLobby::WindowMessagesFn; PAR_GetEmitterParticleCount; and, under the
// instances, src/lobby_meshes.cpp (CLBLobbyMeshes, CLBMeshType, CLBMeshInstance) and GetUniqueID.
// Fakes that record: what the mesh code calls (the ride and height field loaders, the draw
// switches, the matrix helpers, the string compare), the lobby system base class, the window's
// default handler, the mail icon. Fakes that do not: the flying system's Init, Tick and Render
// (src/lobby_islands_tick.cpp), the options table's own members, and the strings, which are
// functional.
//
// The lobby's random numbers are CLBLobby::GetRand's text (src/lobby_islands_rest.cpp), copied
// here: linking that file would need the island parser's sixty callees. The PowerPC side runs
// the original GetRand, so every draw the code takes is compared.
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "FS_System.h"
#include "InterfaceWindow.h"
#include "OptionsBalance.h"
#include "TPEngine.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "ui_ErrorDisplay.h"

#include "fake_strings.h"

using namespace calllog;

namespace {
enum { kSets = 28 };   // ride set 0 is the height field's; instances get the sets 1.. in order
constexpr int kLabelSet = 40, kLabelMesh = 10, kLabelMatrix = 70;
constexpr int kLabelLobby = 1, kLabelWindow = 3, kLabelMeshes = 5, kLabelFakeSystem = 6;

tag_sRideAnimSet gSets[kSets];
tag_sMesh gMeshObjects[kSets];
tag_sM3D2Header* gHeaders[kSets];
tag_sNode* gNodes[kSets];
alignas(16) unsigned char gMeshesStore[sizeof(CLBLobbyMeshes)];
alignas(16) unsigned char gLobbyStore[2][sizeof(CLBLobby)];
alignas(16) unsigned char gWindowStore[2][16];
bool gQuiet;   // no events from the lobby system base class while the world is built

// A system the lobby holds, to see who is given the window's messages.
class FakeSystem : public CLBLobbySystem {
public:
    void Init() override {}
    void Tick() override {}
    void Render() override {}
    void Close() override {}
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override {
        ev("System.WindowMessagesFn", {lab(this), message, a, b});
    }
};
FakeSystem* gFakeSystems[4];

int64_t Hash(const char* text) {
    uint32_t h = 2166136261u;
    for (; text != nullptr && *text != 0; ++text) h = (h ^ static_cast<unsigned char>(*text)) * 16777619u;
    return h;
}
float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
// A float's bits with every NaN the same (the sign of a NaN that passes through a fused multiply is not reproduced).
int64_t Word(float value) {
    const uint32_t bits = fbits(value);
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0 ? 0x7fc00000u : bits;
}
int32_t Lower(int32_t c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int64_t Word(HFieldXZ value) { return (static_cast<int64_t>(value.mF00) << 16) | value.mF02; }
int64_t Number(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p)); }
s3DPoint Point(const uint32_t* bits) { return {FromBits(bits[0]), FromBits(bits[1]), FromBits(bits[2])}; }
CLBLobby* Lobby(int32_t which) { return reinterpret_cast<CLBLobby*>(gLobbyStore[which]); }
}  // namespace

// ---- globals of the code under test
CLBLobby* gLobby;
CLBLobbyMeshes* gLobbyMeshes;
CLBFlyingMeshSystem* gLobbyFlyingMeshes;
COptionsBalance gOptionsBalance;
CSYS_TPFileInfo gTPFileInfo;
tag_sRideAnimSet* gRideAnimSets[kSets + 2];
tag_sRideAnimSet* gHeightfieldRideSet;

// ---- the lobby's generator (src/lobby_islands_rest.cpp's text)
uint32_t CLBLobby::GetRand() {
    mStarted = mStarted * 0x19660du + 0x3c6ef35fu;
    return mStarted;
}

// ---- fakes: the lobby system base class, the options table's members, the flying system's frame members
CLBLobbySystem::CLBLobbySystem() {
    if (!gQuiet) ev("CLBLobbySystem", {newlab(this)});
}
CLBLobbySystem::~CLBLobbySystem() {
    if (!gQuiet) ev("~CLBLobbySystem", {lab(this)});
}
void CBalanceTable::Init() {}
COptionsBalance::~COptionsBalance() {}
const BalanceDescription* COptionsBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* COptionsBalance::GetBalanceValuesStart() { return mValues; }
void CLBFlyingMeshSystem::Init() {}
void CLBFlyingMeshSystem::Tick() {}
void CLBFlyingMeshSystem::Render() {}

// ---- fakes: the window and the mail icon
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Window.Default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default", 5));
}
void UpdateOutboxMailIcon() { ev("UpdateOutboxMailIcon"); }

// ---- fakes: the ride and height field code under the mesh system (as in unit_LobbyMeshes.cpp)
void CSYS_TPFileInfo::SetLevelPath(const TbDynamicStringTemplate<char>& path) { ev("SetLevelPath", {}, {path.mData}); }
int32_t LoadLevelHeightfield(int32_t a, int32_t b) {
    ev("LoadLevelHeightfield", {a, b});
    return 1;
}
void DestroyLevelHeightfield() { ev("DestroyLevelHeightfield"); }
void EnableMeshDraw(tag_sMesh* mesh) { ev("EnableMeshDraw", {lab(mesh)}); }
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("EnableMeshDrawRange", {lab(mesh), Word(from), Word(to)}); }
void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw", {lab(mesh)}); }
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("DisableMeshDrawRange", {lab(mesh), Word(from), Word(to)}); }
void ScaleMesh(tag_sMesh* mesh, s3DPoint* scale) { ev("ScaleMesh", {lab(mesh), fbits(scale->x), fbits(scale->y), fbits(scale->z)}); }
uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t c,
                      const char16_t* sign1, const char16_t* sign2, int32_t d, int32_t e) {
    ev("LoadRideType", {Number(a), Number(b), flags, id, c, Number(sign1), Number(sign2), d, e}, {folder, name});
    return static_cast<uint32_t>(ret("RideType", 7));
}
uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d,
                      int32_t e, uint32_t f, int32_t g) {
    ev("InstanceRide", {type, flags, Number(params), a, b, Number(c), d, e, f, g});
    return static_cast<uint32_t>(ret("Instance", 1));
}
void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {instance}); }
void DestroyAllRideTypes() { ev("DestroyAllRideTypes"); }
void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
}
uint32_t GetCurrentAnimStats(tag_sRideAnimSet* set, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t d) {
    ev("GetCurrentAnimStats", {lab(set), Number(a), Number(b), Number(c), d});
    return static_cast<uint32_t>(ret("AnimTime", 0));
}
void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {lab(matrix)});
    std::memset(matrix, 0, sizeof *matrix);
    for (int i = 0; i < 4; ++i) matrix->m[i][i] = 1.0f;
}
void RotateLMatrixXYZNoTrans(sMatrix* matrix, eRotateParams axis, float* angle) {
    ev("RotateLMatrixXYZNoTrans", {lab(matrix), axis, fbits(*angle)});
}
int32_t RegenerateRideSign(uint32_t instance, char* folder, char* name, const char16_t* sign1, const char16_t* sign2) {
    ev("RegenerateRideSign", {instance, Number(sign1), Number(sign2)}, {folder, name});
    return 1;
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    for (;; ++a, ++b) {
        const int32_t ca = Lower(static_cast<unsigned char>(*a));
        const int32_t cb = Lower(static_cast<unsigned char>(*b));
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}

extern "C" {
// ---- the world
// words: per ride set its flags and the two map words (kSets x 3), then the lobby's generator state,
// its step (float bits), the LOBBYOBJECTS option, the window sizes of the two lobbies (width, height,
// word at 0x1c each: 6 words). Labels: the lobbies 1 and 2, the windows 3 and 4, the mesh system 5, the
// fake systems 6.. (two per lobby), the meshes 10.., the sets 40.., the matrices 70...
void tpw_fm_begin(const uint32_t* words) {
    const int32_t wasOn = tpw_cl_on;
    tpw_cl_on = 0;
    gQuiet = true;
    for (int i = 0; i < kSets; ++i) {
        if (gHeaders[i] == nullptr) {
            gHeaders[i] = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
            gNodes[i] = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        }
        std::memset(gHeaders[i], 0xa5, sizeof(tag_sM3D2Header));
        std::memset(gNodes[i], 0xa5, sizeof(tag_sNode));
        gHeaders[i]->mF78 = gNodes[i];
        for (int k = 0; k < 16; ++k) gNodes[i]->mMatrix.m[k / 4][k % 4] = static_cast<float>(i * 100 + k);
        std::memset(&gMeshObjects[i], 0xa5, sizeof gMeshObjects[i]);
        gMeshObjects[i].mHeader = gHeaders[i];
        std::memset(&gSets[i], 0xa5, sizeof gSets[i]);
        gSets[i].mFlags = words[i * 3];
        gSets[i].mHeads = &gMeshObjects[i];
        gSets[i].mFC0 = {static_cast<uint16_t>(words[i * 3 + 1] >> 16), static_cast<uint16_t>(words[i * 3 + 1])};
        gSets[i].mFC4 = {static_cast<uint16_t>(words[i * 3 + 2] >> 16), static_cast<uint16_t>(words[i * 3 + 2])};
        gRideAnimSets[i] = i == 0 ? nullptr : &gSets[i];
        tpw_cl_label(kLabelMesh + i, &gMeshObjects[i]);
        tpw_cl_label(kLabelSet + i, &gSets[i]);
        tpw_cl_label(kLabelMatrix + i, &gNodes[i]->mMatrix);
    }
    gHeightfieldRideSet = &gSets[0];
    const uint32_t* w = words + kSets * 3;
    gOptionsBalance.mValues[27].mInteger = static_cast<int32_t>(w[2]);

    // the mesh system, for real
    std::memset(gMeshesStore, 0xa5, sizeof gMeshesStore);
    gLobbyMeshes = new (gMeshesStore) CLBLobbyMeshes;
    tpw_cl_label(kLabelMeshes, gMeshesStore);

    // two lobbies with two fake systems each (the first one is the lobby the code finds in gLobby)
    for (int which = 0; which < 2; ++which) {
        std::memset(gLobbyStore[which], 0xa5, sizeof gLobbyStore[which]);
        std::memset(gWindowStore[which], 0xa5, sizeof gWindowStore[which]);
        CLBLobby* lobby = Lobby(which);
        lobby->mWindow = reinterpret_cast<InterfaceWindow*>(gWindowStore[which]);
        lobby->mTickUnits = FromBits(w[1]);
        lobby->mStarted = which == 0 ? w[0] : w[0] ^ 0x5a5a5a5au;
        lobby->mWidth = static_cast<int32_t>(w[3 + which * 3]);
        lobby->mHeight = static_cast<int32_t>(w[4 + which * 3]);
        lobby->mF1C = static_cast<int32_t>(w[5 + which * 3]);
        lobby->mSystems = new TbPtrList<CLBLobbySystem>;
        for (int k = 0; k < 2; ++k) {
            auto* system = new FakeSystem;
            gFakeSystems[which * 2 + k] = system;
            lobby->mSystems->AddTail(system);
            tpw_cl_label(kLabelFakeSystem + which * 2 + k, system);
        }
        tpw_cl_label(kLabelLobby + which, lobby);
        tpw_cl_label(kLabelWindow + which, gWindowStore[which]);
    }
    gLobby = Lobby(0);
    gLobbyFlyingMeshes = nullptr;
    gParSystemRunning = 0;
    std::memset(gParEmitters, 0, sizeof gParEmitters);
    gQuiet = false;
    tpw_cl_on = wasOn;
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
}

// ---- operations (each takes and answers plain numbers; objects are addresses)
void tpw_fm_label(int32_t label, const void* object) { tpw_cl_label(label, object); }

uintptr_t tpw_fm_system_new() {
    void* memory = ::operator new(sizeof(CLBFlyingMeshSystem));
    std::memset(memory, 0xa5, sizeof(CLBFlyingMeshSystem));
    gLobbyFlyingMeshes = new (memory) CLBFlyingMeshSystem;
    return reinterpret_cast<uintptr_t>(gLobbyFlyingMeshes);
}
// Close() in the lobby's end: the system is deleted and the global cleared. A flag of 0 or below is the
// destructor alone, as the PowerPC side's destructor with that flag (what a derived class's would run).
void tpw_fm_system_delete(int32_t flag) {
    if (flag > 0) delete gLobbyFlyingMeshes;
    else gLobbyFlyingMeshes->~CLBFlyingMeshSystem();
    gLobbyFlyingMeshes = nullptr;
}
void tpw_fm_system_register(uintptr_t group) { gLobbyFlyingMeshes->Register(reinterpret_cast<CLBFlyingMeshGroup*>(group)); }
void tpw_fm_system_unregister(uintptr_t group) { gLobbyFlyingMeshes->UnRegister(reinterpret_cast<CLBFlyingMeshGroup*>(group)); }
void tpw_fm_system_clear() { gLobbyFlyingMeshes->DeleteFlyingMeshGroupListEntries(); }
// The members the lobby calls through the vtable.
void tpw_fm_system_frame(int32_t message, int32_t a, int32_t b) {
    CLBLobbySystem* system = gLobbyFlyingMeshes;
    system->Close();
    system->WindowMessagesFn(message, a, b);
}
// The lists' own members that the code under test reaches only on the way to something else: RemoveAll, and
// RemoveHead (which also answers NULL for an empty list).
void tpw_fm_system_removeall() { gLobbyFlyingMeshes->mGroups->RemoveAll(); }
uintptr_t tpw_fm_system_removehead() { return reinterpret_cast<uintptr_t>(gLobbyFlyingMeshes->mGroups->RemoveHead()); }
int32_t tpw_fm_system_groups(uintptr_t* out) {
    int32_t n = 0;
    for (auto* node = gLobbyFlyingMeshes->mGroups->mHead; node != nullptr; node = node->mNext) out[n++] = reinterpret_cast<uintptr_t>(node->mData);
    return n;
}

uintptr_t tpw_fm_group_new(int32_t number, const char* folder, const char* name, const uint32_t* position,
                           const uint32_t* direction, uint32_t speed, uint32_t limit) {
    void* memory = ::operator new(sizeof(CLBFlyingMeshGroup));
    std::memset(memory, 0xa5, sizeof(CLBFlyingMeshGroup));
    auto* group = new (memory) CLBFlyingMeshGroup(number, folder, name, Point(position), Point(direction), FromBits(speed), FromBits(limit));
    return reinterpret_cast<uintptr_t>(group);
}
void tpw_fm_group_delete(uintptr_t group, int32_t flag) {
    auto* object = reinterpret_cast<CLBFlyingMeshGroup*>(group);
    if (flag > 0) delete object;
    else object->~CLBFlyingMeshGroup();
}
void tpw_fm_group_tick(uintptr_t group) { reinterpret_cast<CLBFlyingMeshGroup*>(group)->Tick(); }
void tpw_fm_group_show(uintptr_t group) { reinterpret_cast<CLBFlyingMeshGroup*>(group)->Show(); }
void tpw_fm_group_clear(uintptr_t group) { reinterpret_cast<CLBFlyingMeshGroup*>(group)->DeleteFlyingMeshInstanceListEntries(); }
// (through the vtable, as the instances do)
void tpw_fm_group_register(uintptr_t group, uintptr_t instance) {
    reinterpret_cast<CLBFlyingMeshGroup*>(group)->Register(reinterpret_cast<CLBFlyingMeshInstance*>(instance));
}
void tpw_fm_group_unregister(uintptr_t group, uintptr_t instance) {
    reinterpret_cast<CLBFlyingMeshGroup*>(group)->UnRegister(reinterpret_cast<CLBFlyingMeshInstance*>(instance));
}
void tpw_fm_group_removeall(uintptr_t group) { reinterpret_cast<CLBFlyingMeshGroup*>(group)->mInstances->RemoveAll(); }
uintptr_t tpw_fm_group_removehead(uintptr_t group) {
    return reinterpret_cast<uintptr_t>(reinterpret_cast<CLBFlyingMeshGroup*>(group)->mInstances->RemoveHead());
}
int32_t tpw_fm_group_instances(uintptr_t group, uintptr_t* out) {
    int32_t n = 0;
    for (auto* node = reinterpret_cast<CLBFlyingMeshGroup*>(group)->mInstances->mHead; node != nullptr; node = node->mNext)
        out[n++] = reinterpret_cast<uintptr_t>(node->mData);
    return n;
}

uintptr_t tpw_fm_instance_new(uintptr_t group, const char* folder, const char* name, const uint32_t* position,
                              const uint32_t* direction, uint32_t speed, uint32_t arrival) {
    void* memory = ::operator new(sizeof(CLBFlyingMeshInstance));
    std::memset(memory, 0xa5, sizeof(CLBFlyingMeshInstance));
    auto* instance = new (memory) CLBFlyingMeshInstance(reinterpret_cast<CLBFlyingMeshGroup*>(group), folder, name, Point(position),
                                                        Point(direction), FromBits(speed), FromBits(arrival));
    return reinterpret_cast<uintptr_t>(instance);
}
void tpw_fm_instance_delete(uintptr_t instance, int32_t flag) {
    auto* object = reinterpret_cast<CLBFlyingMeshInstance*>(instance);
    if (flag > 0) delete object;
    else object->~CLBFlyingMeshInstance();
}
void tpw_fm_instance_tick(uintptr_t instance) { reinterpret_cast<CLBFlyingMeshInstance*>(instance)->Tick(); }

// The lobby window's message function, run on lobby `which` with gLobby set to lobby 0, lobby 1 or nothing (mode
// 0, 1, 2) and the window 0 or 1.
int32_t tpw_fm_lobby_message(int32_t which, int32_t mode, int32_t window, int32_t message, int32_t a, int32_t b) {
    gLobby = mode == 0 ? Lobby(0) : mode == 1 ? Lobby(1) : nullptr;
    const int32_t answer = Lobby(which)->WindowMessagesFn(reinterpret_cast<InterfaceWindow*>(gWindowStore[window]), message, a, b);
    gLobby = Lobby(0);
    return answer;
}

// The particle system's words: per emitter its in-use word and its count; the emitters 0.. in order.
void tpw_fm_particles(int32_t running, int32_t count, const int32_t* inUse, const int32_t* particles) {
    gParSystemRunning = running;
    std::memset(gParEmitters, 0xa5, sizeof gParEmitters);
    for (int32_t i = 0; i < count && i < 126; ++i) {
        gParEmitters[i].mInUse = static_cast<int16_t>(inUse[i]);
        gParEmitters[i].mParticles = static_cast<uint16_t>(particles[i]);
    }
}
int32_t tpw_fm_particle_count(uint32_t emitter) { return PAR_GetEmitterParticleCount(emitter); }

// ---- the state
// A group: shown, maximum, made, hashes of the folder and the name, the box (6 floats), speed, limit, then the
// instance list (count, the instances' addresses).
int32_t tpw_fm_group_state(uintptr_t address, int64_t* out) {
    auto* group = reinterpret_cast<CLBFlyingMeshGroup*>(address);
    int64_t* p = out;
    *p++ = group->mShown;
    *p++ = group->mMax;
    *p++ = group->mCount;
    *p++ = Hash(group->mFolder.mData);
    *p++ = Hash(group->mName.mData);
    for (float v : {group->mPosition.x, group->mPosition.y, group->mPosition.z, group->mExtent.x, group->mExtent.y,
                    group->mExtent.z, group->mSpeed, group->mArriveDistSq})
        *p++ = Word(v);
    *p++ = group->mInstances->mCount;
    for (auto* node = group->mInstances->mHead; node != nullptr; node = node->mNext) *p++ = Number(node->mData);
    return static_cast<int32_t>(p - out);
}

// An instance: its group's address, the position of its mesh in the mesh system's list (-1 when it is not
// there), the mesh's ride instance, shown flag and folder and name hashes, its own shown flag, and 20 floats
// (the box, speed, limit, place, goal and direction, then the goal direction, which is not defined until the
// first Tick: `ticked` says whether to read it).
int32_t tpw_fm_instance_state(uintptr_t address, int32_t ticked, int64_t* out) {
    auto* instance = reinterpret_cast<CLBFlyingMeshInstance*>(address);
    int64_t* p = out;
    *p++ = Number(instance->mGroup);
    int32_t index = -1, at = 0;
    for (auto* node = gLobbyMeshes->mInstances->mHead; node != nullptr; node = node->mNext, ++at)
        if (node->mData == instance->mMesh) index = at;
    *p++ = index;
    *p++ = instance->mMesh->mInstance;
    *p++ = instance->mMesh->mShown;
    *p++ = Hash(instance->mMesh->mFolder.mData);
    *p++ = Hash(instance->mMesh->mName.mData);
    *p++ = instance->mShown;
    for (const s3DPoint* v : {&instance->mCentre, &instance->mExtent}) {
        *p++ = Word(v->x);
        *p++ = Word(v->y);
        *p++ = Word(v->z);
    }
    *p++ = Word(instance->mSpeed);
    *p++ = Word(instance->mArriveDistSq);
    for (const s3DPoint* v : {&instance->mPosition, &instance->mGoal, &instance->mDirection}) {
        *p++ = Word(v->x);
        *p++ = Word(v->y);
        *p++ = Word(v->z);
    }
    *p++ = ticked ? Word(instance->mGoalDirection.x) : 0;
    *p++ = ticked ? Word(instance->mGoalDirection.y) : 0;
    *p++ = ticked ? Word(instance->mGoalDirection.z) : 0;
    return static_cast<int32_t>(p - out);
}

// The world: the sets' matrices (bit for bit, any NaN as one value), the two lobbies' words, the generator,
// and operator new's and delete's counts.
int32_t tpw_fm_world_state(int64_t* out) {
    int64_t* p = out;
    for (int i = 0; i < kSets; ++i)
        for (int k = 0; k < 16; ++k) *p++ = Word(gNodes[i]->mMatrix.m[k / 4][k % 4]);
    for (int which = 0; which < 2; ++which) {
        const CLBLobby* lobby = Lobby(which);
        *p++ = lab(lobby->mWindow);
        *p++ = lobby->mWidth;
        *p++ = lobby->mHeight;
        *p++ = lobby->mF1C;
        *p++ = lobby->mStarted;
    }
    *p++ = tpw_cl_heap[0];
    *p++ = tpw_cl_heap[2];
    return static_cast<int32_t>(p - out);
}
}
