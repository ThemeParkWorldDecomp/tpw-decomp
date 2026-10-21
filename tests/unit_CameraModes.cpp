// Test library for the camera modes and MAP_wall_in_way (unit w141; libtpw_unit_CameraModes.dylib, tests/test_CameraModes.py), main app:
//   src/game/TPCamera.cpp  SetCameraMode, LoadCamera, SaveCamera, ToggleFirstPersonWalkAround, Setup1stPersonRideMeshes,
//                           ToggleRideRideIn1stPerson, SetupRideRideMatrixAndSuch and the load-time copy of __sinit_TPCamera_cpp
//   src/game/park/Map.cpp   MAP_wall_in_way, gEntranceWalkEnd
//
// Fakes that record through calllog (same event names and arguments as the module's PowerPC stubs): every cell predicate (answers from a
// queue named after it, the cell as its index in the map), GetNeighbours, GetDirection, GetTrackCellEquiv, CTrackCell::IsBridge,
// CObject::GetDBInfo (a record whose cannot-ride value the case sets), IsRideRideable, DisableRideHighlight, IsCoaster,
// GetCoaster1stPersonCar, the two sprite switches, ShowFirstPersonWindow, the procedural texturing trio, both EnableMeshDraw overloads,
// FindDataNodeIndexByID, TPProcessEmbeddedData (faked: the real one is a different unit), LbFile_Read, LbFile_Write, UDebug and
// GAME_ASSERTMSG. The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "MapCell.h"
#include "RideDB.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint32_t gFirstPersonFlags;
tag_sRideAnimSet* gFirstPersonRideSet;
uint32_t gFirstPersonRideInstance;
sMatrix* gFirstPersonRideMatrix;
float gCameraF68;
float gCameraStartPosition[3] = {475.0f, 0.0f, 175.0f};
float gCameraPosition[3] = {1.5f, 2.5f, 3.5f};   // the load-time copy of the start position replaces these
float gCameraF5BF4;
float gCameraYaw;
float gFirstPersonTurn;
float gFirstPersonPitch;
tag_sRideAnimSet* gRideAnimSets[16];
uint32_t gRideCountA;
uint32_t gRideCountB;
s3DPoint gPointerWorldPosn;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
#ifdef __APPLE__
extern unsigned char gPlayerSettingsStorage[sizeof(CPlayerSettings)] __asm__("_gPlayerSettings");
#else
extern unsigned char gPlayerSettingsStorage[sizeof(CPlayerSettings)] __asm__("gPlayerSettings");
#endif
alignas(16) unsigned char gPlayerSettingsStorage[sizeof(CPlayerSettings)] = {};
UIInterface gUIInterface;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {
constexpr uint32_t kBlockSize = 0x1db000;
alignas(16) unsigned char gBlock[kBlockSize];
alignas(16) unsigned char gRideInfoStorage[sizeof(RideInfo)];

int64_t CellIndex(const void* cell) {
    return (static_cast<const unsigned char*>(cell) - (reinterpret_cast<const unsigned char*>(gGameWorld) + 0x2d8)) / 0x44;
}
int64_t TrackIndex(const void* cell) {
    return ((static_cast<const unsigned char*>(cell) - (reinterpret_cast<const unsigned char*>(gGameWorld) + 0x2d8)) - 0x110000) / 0x28;
}
int64_t Word(HFieldXZ v) { return (static_cast<int64_t>(v.mF00) << 16) | v.mF02; }
const char* Hex(const void* data, uint32_t length) {
    static char text[64];
    static const char digits[] = "0123456789abcdef";
    uint32_t n = length > 24 ? 24 : length;
    for (uint32_t i = 0; i < n; ++i) {
        text[2 * i] = digits[static_cast<const uint8_t*>(data)[i] >> 4];
        text[2 * i + 1] = digits[static_cast<const uint8_t*>(data)[i] & 15];
    }
    text[2 * n] = 0;
    return text;
}
}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), lab(buffer), length});
    for (uint32_t i = 0; i < length; ++i) static_cast<uint8_t*>(buffer)[i] = static_cast<uint8_t>(ret("RdB"));
    const int64_t n = ret("RdN", -1);
    *transferred = n < 0 ? length : static_cast<uint32_t>(n);
    return TbError(0);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), lab(buffer), length}, {Hex(buffer, length)});
    const int64_t n = ret("WrN", -1);
    *transferred = n < 0 ? length : static_cast<uint32_t>(n);
    return TbError(0);
}

void UIInterface::ShowFirstPersonWindow(int32_t on) { ev("ShowFirstPersonWindow", {lab(this), on}); }
void DisableRideHighlight(int32_t flag) { ev("DisableRideHighlight", {flag}); }
int32_t IsCoaster(uint32_t instance) {
    ev("IsCoaster", {instance});
    return static_cast<int32_t>(ret("IsCoaster"));
}
uint32_t GetCoaster1stPersonCar(uint32_t instance) {
    ev("GetCoaster1stPersonCar", {instance});
    return static_cast<uint32_t>(ret("GetCoaster1stPersonCar"));
}
void ESP_SwitchToFirstPersonSprites() { ev("ESP_SwitchToFirstPersonSprites"); }
void ESP_SwitchToThirdPersonSprites() { ev("ESP_SwitchToThirdPersonSprites"); }
int32_t IsProceduralTexturingActive() {
    ev("IsProceduralTexturingActive");
    return static_cast<int32_t>(ret("IsProceduralTexturingActive"));
}
void EnableProceduralTexturing() { ev("EnableProceduralTexturing"); }
void DisableProceduralTexturing() { ev("DisableProceduralTexturing"); }
void EnableMeshDraw(tag_sMesh* mesh) { ev("EnableMeshDraw", {lab(mesh)}); }
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("EnableMeshDrawRange", {lab(mesh), Word(from), Word(to)}); }
void TPProcessEmbeddedData(tag_sMesh* mesh) { ev("TPProcessEmbeddedData", {lab(mesh)}); }
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), mask, id});
    return static_cast<uint32_t>(ret("Find", -1));
}
int32_t IsRideRideable(void* object, uint32_t animSet) {
    ev("IsRideRideable", {lab(object), animSet});
    return static_cast<int32_t>(ret("IsRideRideable"));
}
RideInfo* CObject::GetDBInfo() const {
    ev("CObject::GetDBInfo", {lab(this)});
    return reinterpret_cast<RideInfo*>(gRideInfoStorage);
}

#define PREDICATE(Class, Name) \
    int32_t Class::Name() const { ev(#Class "::" #Name, {CellIndex(this)}); return static_cast<int32_t>(ret(#Class "::" #Name)); }
PREDICATE(CMapCell, IsPath)
PREDICATE(CMapCell, IsQueue)
PREDICATE(CMapCell, IsRide)
PREDICATE(CMapCell, IsGrass)
PREDICATE(CMapCell, IsEntrance)
PREDICATE(CMapCell, IsExit)
PREDICATE(CMapCell, IsTrack)
PREDICATE(CMapCell, IsNoGo)
PREDICATE(CMapCell, IsWalkable)
PREDICATE(CMapCell, IsWalkableInFirstPerson)
PREDICATE(CMapCell, IsWater)
PREDICATE(CMapCell, IsTrackX)
int32_t CTrackCell::IsBridge() const {
    ev("CTrackCell::IsBridge", {TrackIndex(this)});
    return static_cast<int32_t>(ret("CTrackCell::IsBridge"));
}
uint8_t CCellBase::GetNeighbours() const {
    ev("CCellBase::GetNeighbours", {CellIndex(this)});
    return static_cast<uint8_t>(ret("CCellBase::GetNeighbours"));
}
uint8_t CCellBase::GetDirection() const {
    ev("CCellBase::GetDirection", {CellIndex(this)});
    return static_cast<uint8_t>(ret("CCellBase::GetDirection"));
}
const CTrackCell* CCellBase::GetTrackCellEquiv() const {
    ev("CCellBase::GetTrackCellEquiv", {CellIndex(this)});
    return reinterpret_cast<const CTrackCell*>(reinterpret_cast<const unsigned char*>(gGameWorld) + 0x2d8 + 0x110000 +
                                               ret("TrackEquiv") * 0x28);
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_cmo_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gFirstPersonFlags},         {"instance", &gFirstPersonRideInstance}, {"f68", &gCameraF68},
        {"start", gCameraStartPosition},       {"pos", gCameraPosition},                {"f5bf4", &gCameraF5BF4},
        {"yaw", &gCameraYaw},                  {"turn", &gFirstPersonTurn},             {"pitch", &gFirstPersonPitch},
        {"spos", gCameraSavedPosition},        {"syaw", &gCameraSavedYaw},              {"once0", &gRideRideOnceGuard},
        {"once1", &gRideRideOnceValue},        {"sentinel", &gEntranceWalkEnd},         {"width", &gHeightfieldWidth},
        {"height", &gHeightfieldHeight},       {"pointer", &gPointerWorldPosn},         {"countA", &gRideCountA},
        {"countB", &gRideCountB},              {"iso", &gPlayerSettings.mCurrent.mIsometricOn}, {"ui", &gUIInterface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The pointer globals (8 bytes here, 4 on the Mac): set and read by name.
void* tpw_cmo_ptr_slot(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    const Entry entries[] = {
        {"rideSet", &gFirstPersonRideSet},      {"matrix", &gFirstPersonRideMatrix}, {"flagword", &gFirstPersonRideFlagWord},
        {"node", &gFirstPersonRideNode},        {"headsSet", &gCameraHeadsRideSet},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}
void tpw_cmo_ptr_set(const char* name, uint64_t value) {
    *static_cast<uintptr_t*>(tpw_cmo_ptr_slot(name)) = static_cast<uintptr_t>(value);
}
uint64_t tpw_cmo_ptr_get(const char* name) { return static_cast<uint64_t>(*static_cast<uintptr_t*>(tpw_cmo_ptr_slot(name))); }
void tpw_cmo_ride_set(int32_t index, uint64_t address) { gRideAnimSets[index] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(address)); }
uint64_t tpw_cmo_ride_set_get(int32_t index) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRideAnimSets[index])); }

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cmo_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// A zeroed block the size of the map (0x1db000 bytes); the world pointer is set by tpw_cmo_world.
uint64_t tpw_cmo_block() {
    std::memset(gBlock, 0, sizeof gBlock);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gBlock));
}
void tpw_cmo_world(uint64_t block) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(block) - 0x2d8); }
void tpw_cmo_row(int32_t row, uint64_t address) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }
void tpw_cmo_ride_info(int32_t value) {
    std::memset(gRideInfoStorage, 0, sizeof gRideInfoStorage);
    reinterpret_cast<RideInfo*>(gRideInfoStorage)->Value(kRideSlot_CannotRide).mInteger = value;
}

// A ride anim set with a mesh, a header with nodes, mini nodes and embedded data, and (has28) an embedded set. s: setFlags, fc0 word, fc4
// word, has28, N full nodes, M mini nodes, mF46, E embedded entries, then N + M node flag words, E header embedded flags, E record
// flags, E block flags (nonzero: the record gets a 0x40 byte block), the embedded set's own flags. out: mesh, header, embedded set, E
// node addresses (EmbeddedNode), E record flag word addresses, E block addresses.
uint64_t tpw_cmo_build_set(const uint32_t* s, uint64_t* out) {
    const uint32_t N = s[4], M = s[5], F46 = s[6], E = s[7];
    const uint32_t* p = s + 8;
    auto* set = static_cast<tag_sRideAnimSet*>(tpw::ArenaAllocZeroed(sizeof(tag_sRideAnimSet)));
    auto* mesh = static_cast<tag_sMesh*>(tpw::ArenaAllocZeroed(sizeof(tag_sMesh)));
    auto* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    set->mFlags = s[0];
    set->mFC0.mF00 = static_cast<uint16_t>(s[1] >> 16);
    set->mFC0.mF02 = static_cast<uint16_t>(s[1]);
    set->mFC4.mF00 = static_cast<uint16_t>(s[2] >> 16);
    set->mFC4.mF02 = static_cast<uint16_t>(s[2]);
    set->mHeads = mesh;
    mesh->mHeader = header;
    header->mNodeCount = static_cast<uint16_t>(N);
    header->mF46 = static_cast<uint16_t>(F46);
    header->mEmbeddedCount = static_cast<uint16_t>(E);
    auto* nodes = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode) * (N + 1)));
    auto* minis = static_cast<tag_sMiniNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sMiniNode) * (M + 1)));
    header->mNodes.set(nodes);
    header->mMiniNodes.set(minis);
    for (uint32_t i = 0; i < N; ++i) nodes[i].mFlags = p[i];
    for (uint32_t i = 0; i < M; ++i) minis[i].mFlags = p[N + i];
    p += N + M;
    auto* embedded = static_cast<sEmbeddedData*>(tpw::ArenaAllocZeroed(sizeof(sEmbeddedData) * (E + 1)));
    header->mEmbedded.set(embedded);
    for (uint32_t i = 0; i < E; ++i) embedded[i].mFlags = p[i];
    p += E;
    auto* embeddedSet = static_cast<sEmbeddedSet*>(tpw::ArenaAllocZeroed(sizeof(sEmbeddedSet)));
    auto* records = static_cast<sEmbeddedRecord*>(tpw::ArenaAllocZeroed(sizeof(sEmbeddedRecord) * (E + 1)));
    embeddedSet->mRecords = records;
    for (uint32_t i = 0; i < E; ++i) {
        records[i].mFlags = p[i];
        if (p[E + i] != 0) records[i].mBlock = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(0x40));
    }
    embeddedSet->mFlags = p[2 * E];
    if (s[3] != 0) mesh->mF28 = embeddedSet;
    out[0] = reinterpret_cast<uintptr_t>(mesh);
    out[1] = reinterpret_cast<uintptr_t>(header);
    out[2] = reinterpret_cast<uintptr_t>(embeddedSet);
    for (uint32_t i = 0; i < E; ++i) {
        out[3 + i] = reinterpret_cast<uintptr_t>(EmbeddedNode(header, i));
        out[3 + E + i] = reinterpret_cast<uintptr_t>(&records[i].mFlags);
        out[3 + 2 * E + i] = reinterpret_cast<uintptr_t>(records[i].mBlock);
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(set));
}
// The values of a built set that a call may change: set flags, N + M node flag words, E record flags (the module reads them back).
uint32_t tpw_cmo_rd32(uint64_t address) { return *reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(address)); }

// The first value gCameraPosition had when the library was loaded (the load-time copy of the start position).
void tpw_cmo_sinit(uint32_t* out) {
    static float first[3];
    static bool taken = false;
    if (!taken) {
        std::memcpy(first, gCameraPosition, sizeof first);
        taken = true;
    }
    std::memcpy(out, first, sizeof first);
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_cmo_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC4));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMiniNode));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedData));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF46));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mEmbeddedCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mMiniNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mEmbedded));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mWho));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellNext));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF1C));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(reinterpret_cast<unsigned char*>(&gPlayerSettings.mCurrent.mIsometricOn) -
                                    reinterpret_cast<unsigned char*>(&gPlayerSettings));
    out[n++] = static_cast<int64_t>(reinterpret_cast<unsigned char*>(&reinterpret_cast<RideInfo*>(gRideInfoStorage)->Value(kRideSlot_CannotRide).mInteger) -
                                    gRideInfoStorage);
    return n;
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t)
int32_t tpw_cmo_setcameramode(int32_t mode, int32_t toggle, int32_t ride, int32_t force, uint64_t out, int32_t reset) {
    return SetCameraMode(mode, toggle, ride, force, reinterpret_cast<int32_t*>(static_cast<uintptr_t>(out)), reset);
}
int32_t tpw_cmo_load(uint64_t file) { return LoadCamera(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }
int32_t tpw_cmo_save(uint64_t file) { return SaveCamera(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }
void tpw_cmo_setupmeshes(int32_t ride) { Setup1stPersonRideMeshes(ride); }
int32_t tpw_cmo_ridematrix(int32_t ride, int32_t node) { return SetupRideRideMatrixAndSuch(ride, node); }
void tpw_cmo_toggleride(int32_t ride, int32_t a, int32_t b, uint64_t out, int32_t node, int32_t c) {
    ToggleRideRideIn1stPerson(ride, a, b, reinterpret_cast<int32_t*>(static_cast<uintptr_t>(out)), node, c);
}
void tpw_cmo_togglewalk() { ToggleFirstPersonWalkAround(); }
int32_t tpw_cmo_wall(uint32_t x, uint32_t z, uint32_t dir, int32_t mode) {
    return MAP_wall_in_way(static_cast<uint8_t>(x), static_cast<uint8_t>(z), static_cast<uint8_t>(dir), static_cast<NavMode>(mode));
}

}  // extern "C"
