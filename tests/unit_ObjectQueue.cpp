// Differential library for unit w197, the ride's queue (src/game/Object.cpp): the fifteen CObject members AddPersonToQueue, RemovePersonFromQueue,
// GetNumPeopleInQueue, GetPositionInQueue, GetFrontOfPhysicalQueue, GetBackOfPhysicalQueue, GetBackOfVirtualQueue, GetBackOfQueue, GetQCellBehind,
// RecalculateBackOfQueue, IsBOQConnected, IsExitCellConnected and the three Convert*QPositionToMapPos.
//
// ONE static block (gRegion) holds everything the functions touch, at the offsets the PowerPC side uses in its own mapped region (RB + offset): the call
// trace (a count at 0 and (tag, value) pairs from 0x100), the ride object, 64 person blocks, the out area, a script block and three world-sized slots
// (WG: gGameWorld, WW: gWorld and the block gWorldMap points into, WM: what CWorldSavvy::GetWorld and GetMap answer), each map at slot + 0x2d8. Every
// fake appends (tag, value) to the trace; a pointer is recorded as its offset in the block, so the two sides' traces compare word for word. The three
// CMapCell predicates answer from bits of the cell's halfword at +0x42 (IsPath bit 1, IsQueue bit 2, IsEntrance bit 3) with the case's truthy word.
// The cell code (CCellBase, CMapCell::GetNeighbouringCell, OPPOSITE, the time stamps) runs as translated; CThing::GetThingId (src/game/Thing.cpp, another unit) is a copy here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "SaveLoad.h"
#include "ObjectParts.h"
#include "TPCamera.h"
#include "TrackCell.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "rsse.h"

namespace {

constexpr uint32_t kTrace = 0x100;
constexpr uint32_t kTraceCap = 0x10000;
constexpr uint32_t kObj = 0x90000;
constexpr uint32_t kPers = 0x91000;
constexpr uint32_t kOut = 0x9a000;
constexpr uint32_t kScript = 0x9b000;
constexpr uint32_t kWG = 0xa0000;
constexpr uint32_t kWW = 0x280000;
constexpr uint32_t kWM = 0x460000;
constexpr uint32_t kRegionSize = 0x640000;

alignas(4096) unsigned char gRegion[kRegionSize];
uint32_t gTraceTag[kTraceCap];
uint32_t gTraceVal[kTraceCap];
uint32_t gTraceN;

int32_t gTruthy = 1;
int32_t gVar0;
int32_t gOpenable;
int32_t gTrackRide;
uint32_t gRand[16];
int32_t gRandN;
int32_t gRandAt;
struct Wall {
    uint8_t x, z;
    uint32_t mask;
};
Wall gWalls[16];
int32_t gWallN;
uint32_t gWallFill;

uint32_t Off(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(gRegion)); }

void Tr(uint32_t tag, uint32_t value) {
    if (gTraceN < kTraceCap) {
        gTraceTag[gTraceN] = tag;
        gTraceVal[gTraceN] = value;
    }
    gTraceN++;
}

uint32_t TextHash(const char* text) {
    uint32_t h = 0;
    for (; *text; text++) h = h * 31u + static_cast<uint8_t>(*text);
    return h;
}

int32_t Pred(uint32_t tag, const CMapCell* cell) {
    Tr(tag, Off(cell));
    return (cell->mF42 & (1u << (tag - 1))) != 0 ? gTruthy : 0;
}

CObject* Obj() { return reinterpret_cast<CObject*>(gRegion + kObj); }

using MapTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;
CWorld* gGameWorld;
CMap* gWorldMap;
uint8_t gLLCC4818;

// ---- link fakes: what the real cell code, the basic serialise instances and the map's other members name but no case reaches (never called)
alignas(16) unsigned char gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");   // raw bytes: the class has a vtable and constructors the unit does not link
CSaveLoad gSaveLoad;
void CSaveLoad::WriteSerialiseLog(int32_t, const char*, LogfileState) {}
TbError LbFile_Read(void*, void*, uint32_t, uint32_t*) { return TbError(-1); }
TbError LbFile_Write(void*, const void*, uint32_t, uint32_t*) { return TbError(-1); }
void TRACE(const char*, ...) {}
void PAR_KillEmitter(int32_t) {}
void SCR_KillScript(int32_t) {}
int32_t wallhug_trivial(wallhug_path*, NavMode) { return 0; }
void CMapCell::RemoveInstanceMesh() {}
void CMapCell::RefreshHFieldTile() {}
void CTrackCell::RemoveInstanceMesh() {}
void CTrackCell::RefreshHFieldTile() {}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const { return {mThingId}; }

// ---- the fakes (the same tags and values as the PowerPC side's stubs)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    Tr(20, static_cast<uint32_t>(condition));
    Tr(21, TextHash(format));
}

void CTRACE(uint32_t level, const char* format, ...) {
    Tr(11, level);
    Tr(12, TextHash(format));
    va_list args;
    va_start(args, format);
    for (const char* p = format; *p; p++) {
        if (*p != '%') continue;
        p++;
        if (*p == '%') continue;
        if (*p == 's') {
            Tr(13, TextHash(va_arg(args, const char*)));
        } else {
            Tr(13, va_arg(args, uint32_t));
        }
    }
    va_end(args);
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t handle) {
    Tr(30, static_cast<uint32_t>(handle));
    return reinterpret_cast<RSSE_SCRIPT*>(gRegion + kScript);
}

int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    Tr(31, Off(script));
    Tr(32, static_cast<uint32_t>(index));
    return gVar0;
}

void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) {
    Tr(33, Off(script));
    Tr(34, static_cast<uint32_t>(index));
    Tr(35, static_cast<uint32_t>(value));
}

int32_t CPerson::InQueueingState() const {
    Tr(40, Off(this));
    int32_t state;
    std::memcpy(&state, reinterpret_cast<const char*>(this) + 0x220, sizeof state);
    return state;
}

void ObjectEvaluateQueueEdit(CPerson* person) __asm__("__ZN7CPerson17EvaluateQueueEditEv");
void ObjectEvaluateQueueEdit(CPerson* person) { Tr(41, Off(person)); }

bool CObject::IsClosed() const {
    Tr(42, Off(this));
    return mF64 == 0;
}

int32_t CObject::IsOpenable() {
    Tr(43, Off(this));
    return gOpenable;
}

int32_t CObject::IsTrackRide() const {
    Tr(44, Off(this));
    return gTrackRide;
}

void CObject::Reopen() { Tr(45, Off(this)); }

CWorld& CWorldSavvy::GetWorld() const {
    Tr(50, Off(this));
    return *reinterpret_cast<CWorld*>(gRegion + kWM);
}

CMap* CWorldSavvy::GetMap() const {
    Tr(51, Off(this));
    return reinterpret_cast<CMap*>(gRegion + kWM + 0x2d8);
}

uint32_t CWorld::GetRand() {
    Tr(52, Off(this));
    const uint32_t value = gRand[gRandAt < gRandN ? gRandAt : gRandN - 1];
    gRandAt++;
    return value;
}

int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode) {
    Tr(53, x);
    Tr(54, z);
    Tr(55, dir);
    Tr(56, static_cast<uint32_t>(mode));
    uint32_t mask = gWallFill;
    for (int32_t i = 0; i < gWallN; i++) {
        if (gWalls[i].x == x && gWalls[i].z == z) mask = gWalls[i].mask;
    }
    return dir < 8 ? static_cast<int32_t>((mask >> dir) & 1) : 0;
}

int32_t CMapCell::IsPath() const { return Pred(2, this); }
int32_t CMapCell::IsQueue() const { return Pred(3, this); }
int32_t CMapCell::IsEntrance() const { return Pred(4, this); }

extern "C" {

uint64_t tpw_oqu_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRegion)); }

void tpw_oqu_init() {
    gGameWorld = reinterpret_cast<CWorld*>(gRegion + kWG);
    gWorld = reinterpret_cast<CWorld*>(gRegion + kWW);
    gWorldMap = reinterpret_cast<CMap*>(gRegion + kWW + 0x2d8);
}

// The case's answers; gMapTimeStamp is the world's stamp counter (the PowerPC side writes it into the image).
void tpw_oqu_begin(int32_t truthy, int32_t var0, int32_t openable, int32_t trackRide, uint32_t stamp, uint32_t wallFill) {
    gTruthy = truthy;
    gVar0 = var0;
    gOpenable = openable;
    gTrackRide = trackRide;
    gMapTimeStamp = stamp;
    gWallFill = wallFill;
    gTraceN = 0;
    gRandN = 1;
    gRandAt = 0;
    gRand[0] = 0;
    gWallN = 0;
}
// The thing table's first n rows: a person block number, or -1 for no thing.
void tpw_oqu_rows(const int32_t* slots, int32_t n) {
    for (int32_t row = 0; row < n; row++) {
        gThingArray[row].mThing = slots[row] >= 0 ? static_cast<void*>(gRegion + kPers + 0x240u * static_cast<uint32_t>(slots[row])) : nullptr;
    }
}
void tpw_oqu_rands(const uint32_t* values, int32_t n) {
    gRandN = n;
    for (int32_t i = 0; i < n; i++) gRand[i] = values[i];
}
void tpw_oqu_wall(int32_t x, int32_t z, uint32_t mask) {
    gWalls[gWallN].x = static_cast<uint8_t>(x);
    gWalls[gWallN].z = static_cast<uint8_t>(z);
    gWalls[gWallN].mask = mask;
    gWallN++;
}
uint32_t tpw_oqu_stamp() { return gMapTimeStamp; }
uint32_t tpw_oqu_trace_n() { return gTraceN; }
const uint32_t* tpw_oqu_trace_tags() { return gTraceTag; }
const uint32_t* tpw_oqu_trace_vals() { return gTraceVal; }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_oqu_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF20));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF28));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF32));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF34));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF36));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mQueuedPerson));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF3C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF5A));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF64));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mPersonLoaded));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mF42));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mNeighbours));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mDirection));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mKind));
    return n;
}

// ---- the functions, one wrapper each with C types only. A tag is passed as a number, a tag result is the halfword.
void tpw_oqu_add(uint32_t person) { Obj()->AddPersonToQueue(ThingTag{static_cast<uint16_t>(person)}); }
void tpw_oqu_remove(uint32_t person) { Obj()->RemovePersonFromQueue(ThingTag{static_cast<uint16_t>(person)}); }
int32_t tpw_oqu_num() { return Obj()->GetNumPeopleInQueue(); }
int32_t tpw_oqu_position(uint32_t person) { return Obj()->GetPositionInQueue(ThingTag{static_cast<uint16_t>(person)}); }
uint32_t tpw_oqu_back() { return Obj()->GetBackOfQueue().mIndex; }
uint32_t tpw_oqu_front() { return Obj()->GetFrontOfPhysicalQueue().mIndex; }
uint32_t tpw_oqu_back_physical() { return Obj()->GetBackOfPhysicalQueue().mIndex; }
uint32_t tpw_oqu_back_virtual() { return Obj()->GetBackOfVirtualQueue().mIndex; }
void tpw_oqu_recalc() { Obj()->RecalculateBackOfQueue(); }
int32_t tpw_oqu_boq() { return Obj()->IsBOQConnected(); }
int32_t tpw_oqu_exit() { return Obj()->IsExitCellConnected(); }
uint32_t tpw_oqu_behind(uint32_t cell) { return CObject::GetQCellBehind(MapTag{static_cast<uint16_t>(cell)}).mIndex; }
int32_t tpw_oqu_convert(uint32_t pos) {
    return Obj()->ConvertQPositionToMapPos(pos, reinterpret_cast<MapTag*>(gRegion + kOut), gRegion + kOut + 4, gRegion + kOut + 5);
}
int32_t tpw_oqu_convert_physical(uint32_t pos) {
    return Obj()->ConvertPhysicalQPositionToMapPos(pos, reinterpret_cast<MapTag*>(gRegion + kOut), gRegion + kOut + 4, gRegion + kOut + 5);
}
int32_t tpw_oqu_convert_virtual(uint32_t pos) {
    return Obj()->ConvertVirtualQPositionToMapPos(pos, reinterpret_cast<MapTag*>(gRegion + kOut), gRegion + kOut + 4, gRegion + kOut + 5);
}

}  // extern "C"
