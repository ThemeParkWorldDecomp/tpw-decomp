// Test library for the click stacks (unit w204; libtpw_unit_ClickStacks.dylib, tests/test_ClickStacks.py), main app:
//   src/game/park/LLCellSystem.cpp  LLCC_SystemBacktrack, the queue walk and the two track walks
//   src/game/park/TrackCell.cpp           CTrackCell::IsCrossRoad, IsStraight
//   src/game/park/MapCell.cpp            CMapCell::IsCorner
//   src/game/park/LLCellRide.cpp        LLCR_OffsetStartOfTrack, LLCR_OffsetEndOfTrack
// Run for real: src/game/park/CellBase.cpp (GetNeighbours, NumberOfNeighbours, NumberOfHVNeighbours, GetDirection, SetOverlapCounter), src/game/park/CellBase.cpp, src/game/park/LLCellSystem.cpp, src/game/park/MapCell.cpp, src/game/park/TPHField.cpp, src/game/park/TrackCell.cpp
// (CCellBase::RemoveNeighbour only), src/game/park/Map.cpp, src/game/park/MapDelta.cpp (OPPOSITE, ADD_DIRECTION, IsXYValid, CMapCell::GetNeighbouringCell), src/game/buildings/TrainingCentre.cpp
// (CTrackCell::GetNeighbouringCell) and src/game/park/TrackCell.cpp (CTrackCell::IsCorner). Every other callee is a fake that records the event the
// PowerPC stub records; cells are named in the log by their id (from the address), their answers come from one byte of the cell. The globals
// of other files are defined here. What the real files drag along but the walks never call is a no-op (LbFile_Read ... GetWorld).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CellBase.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files: the world, the cell system's words (src/LLCellSystem*.cpp, src/game/park/LLCellSystem.cpp) and the compass offsets (MapDelta)
CWorld* gGameWorld;
CWorld* gWorld;
CMap* gWorldMap;
tag_sM3D2Header* gHeightfieldHeader;
// gMainBalance is raw storage under the symbol's name (the class has a constructor and a vtable the unit does not link; nothing here reads it).
alignas(16) unsigned char gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
CSaveLoad gSaveLoad;
CThingArrayEntry gThingArray[kThingArraySize];

uint8_t gLLCC47C4;
uint8_t gLLCC4818;
uint8_t gLLCCE0CC;
int32_t gLLCC4800, gLLCC4804, gLLCC480C, gLLCCDB6C, gLLCCE288, gLLCCE28C, gLLCCFirstBlockX, gLLCCFirstBlockY, gLLCCClickStackCount;
LLCCClick gLLCCClickStack[0x400];

const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};
const MapDelta gMapDeltaMinusY = {0, -1, 0};
const MapDelta gMapDeltaPlusXMinusY = {1, -1, 0};
const MapDelta gMapDeltaMinusXPlusY = {-1, 1, 0};
const MapDelta gMapDeltaMinusX = {-1, 0, 0};
const MapDelta gMapDeltaMinusXMinusY = {-1, -1, 0};

namespace {
char* gWorldStore;   // the world block (gGameWorld and gWorld point to it)

// The 1 based id of a cell from its address (the logs name cells by id; an id of 0 is the cell before the first).
int64_t MapId(const void* p) { return (reinterpret_cast<const char*>(p) - gWorldStore - 0x2d8) / 0x44 + 1; }
int64_t TrackId(const void* p) { return (reinterpret_cast<const char*>(p) - gWorldStore - 0x1102d8) / 0x28 + 1; }
}  // namespace

// ---- what the real files drag along and the walks never call (the serialisers, the map reset's engine calls)
TbError LbFile_Read(void*, void*, uint32_t, uint32_t*) { return TbError(0); }
TbError LbFile_Write(void*, const void*, uint32_t, uint32_t*) { return TbError(0); }
template<> int32_t LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>*, const char*, int32_t) { return 0; }
template<> int32_t SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>*, const char*, int32_t) { return 0; }
template<> int32_t LOADVAR<MacULong>(void*, MacULong*, const char*, int32_t) { return 0; }
template<> int32_t SAVEVAR<MacULong>(void*, MacULong*, const char*, int32_t) { return 0; }
template<> int32_t LOADVAR<uint8_t>(void*, uint8_t*, const char*, int32_t) { return 0; }
template<> int32_t SAVEVAR<uint8_t>(void*, uint8_t*, const char*, int32_t) { return 0; }
template<> int32_t LOADVAR<int16_t>(void*, int16_t*, const char*, int32_t) { return 0; }
template<> int32_t SAVEVAR<int16_t>(void*, int16_t*, const char*, int32_t) { return 0; }
template<> int32_t LOADVAR<uint16_t>(void*, uint16_t*, const char*, int32_t) { return 0; }
template<> int32_t SAVEVAR<uint16_t>(void*, uint16_t*, const char*, int32_t) { return 0; }
void CSaveLoad::WriteSerialiseLog(int32_t, const char*, CSaveLoad::LogfileState) {}
void TRACE(const char*, ...) {}
void SCR_KillScript(int32_t) {}
void PAR_KillEmitter(int32_t) {}
int32_t wallhug_trivial(wallhug_path*, NavMode) { return 0; }
void CTrackCell::ModifyType(CELL_MODES) {}
void CTrackCell::RefreshHFieldTile() {}
void CTrackCell::RemoveInstanceMesh() {}
void CMapCell::RefreshHFieldTile() {}
void CMapCell::RemoveInstanceMesh() {}
void CMapCell::CalculateNeighbours(CELL_MODES, int32_t, int32_t, bool) {}
CWorld& CWorldSavvy::GetWorld() const { return *gWorld; }
CMap* CWorldSavvy::GetMap() const { return gWorld->GetMap(); }

// ---- the assertion (GetNeighbouringCell asserts on a direction that is not one compass bit)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    char shown[160];
    std::snprintf(shown, sizeof shown, "%.159s", format);
    ev("GAME_ASSERTMSG", {condition}, {shown});
}

// ---- the cells' questions: the answer is one bit of one byte of the cell (a map cell's +0x27: 1 queue, 2 entrance, 4 path; a track cell's +0x23:
// 1 just track, 2 track, 4 start block)
int32_t CMapCell::IsQueue() const { ev("IsQueue", {MapId(this)}); return (mF27 & 1) != 0; }
int32_t CMapCell::IsEntrance() const { ev("IsEntrance", {MapId(this)}); return (mF27 & 2) != 0; }
int32_t CMapCell::IsPath() const { ev("IsPath", {MapId(this)}); return (mF27 & 4) != 0; }
void CMapCell::SetHFieldTile() { ev("SetHFieldTile", {MapId(this)}); }
int32_t CTrackCell::IsJustTrack() const { ev("IsJustTrack", {TrackId(this)}); return (mF23 & 1) != 0; }
int32_t CTrackCell::IsTrack() const { ev("IsTrack", {TrackId(this)}); return (mF23 & 2) != 0; }
int32_t CTrackCell::IsTrackStartBlock() const { ev("IsTrackStartBlock", {TrackId(this)}); return (mF23 & 4) != 0; }

// ---- the cell system and the ride
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag ? 1 : 0}); }
void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("LLCC_SystemSetModeNR", {mode, flag ? 1 : 0}); }
void LLCC_FixFirstBlock(bool flag) { ev("LLCC_FixFirstBlock", {flag ? 1 : 0}); }
void LLCM_SetIllegalPlacement(int32_t value) { ev("LLCM_SetIllegalPlacement", {value}); }
void DeleteLastTrackBrace() { ev("DeleteLastTrackBrace"); }
uint8_t LLCR_FixBridges(CObject* ride, bool a, bool b, bool c) {
    ev("LLCR_FixBridges", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return static_cast<uint8_t>(ret("FixBridges", 0));
}
uint8_t LLCR_FixBridgesReverse(CObject* ride, bool a, bool b, bool c) {
    ev("LLCR_FixBridgesReverse", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return static_cast<uint8_t>(ret("FixBridgesReverse", 0));
}
void CObject::RecalculateBackOfQueue() { ev("RecalculateBackOfQueue", {lab(this)}); }

// The mouse up: the queue "UpCount" sets the stack's count (default: unchanged), then "UpPush" pushes the click ("UpX", "UpY") when it is nonzero.
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) {
    ev("LLCM_LeftMouseButtonUp", {x, y, shift, flag});
    int32_t count = static_cast<int32_t>(ret("UpCount", gLLCCClickStackCount));
    if (ret("UpPush", 0) != 0 && count >= 0 && count < 0x400) {
        gLLCCClickStack[count].mX = static_cast<int32_t>(ret("UpX", 0));
        gLLCCClickStack[count].mY = static_cast<int32_t>(ret("UpY", 0));
        ++count;
    }
    gLLCCClickStackCount = count;
}

extern "C" {

// The host address of a global by name (the Python module reads and writes them with the width it knows).
uint64_t tpw_ccs_gaddr(const char* name) {
#define G(n) if (std::strcmp(name, #n) == 0) return reinterpret_cast<uintptr_t>(&n)
    G(gLLCC4800); G(gLLCC4804); G(gLLCC480C); G(gLLCC47C4); G(gLLCC4818); G(gLLCCDB6C); G(gLLCCE0CC); G(gLLCCE288); G(gLLCCE28C);
    G(gLLCCFirstBlockX); G(gLLCCFirstBlockY); G(gLLCCClickStackCount); G(gLLCCClickStack);
#undef G
    if (std::strcmp(name, "gLLCCBacktrackX") == 0) return reinterpret_cast<uintptr_t>(&gLLCCBacktrackClick.mX);
    if (std::strcmp(name, "gLLCCBacktrackY") == 0) return reinterpret_cast<uintptr_t>(&gLLCCBacktrackClick.mY);
    if (std::strcmp(name, "world") == 0) return reinterpret_cast<uintptr_t>(gWorldStore);
    return 0;
}

// Once per process: the world (zero, never constructed), both world words pointing to it.
void tpw_ccs_init() {
    if (gWorldStore == nullptr) gWorldStore = static_cast<char*>(std::calloc(1, 0x1b1000));
    gGameWorld = reinterpret_cast<CWorld*>(gWorldStore);
    gWorld = gGameWorld;
}

// A block for a test object (the pool is reused, zeroed).
uint64_t tpw_ccs_obj(int32_t label, uint32_t size) {
    static void* pool[64];
    const uint32_t want = size < 0x2000 ? 0x2000 : size;
    if (pool[label] == nullptr) pool[label] = std::calloc(1, want);
    std::memset(pool[label], 0, want);
    return reinterpret_cast<uintptr_t>(pool[label]);
}

int32_t tpw_ccs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mF27));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mF23));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF32));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(sizeof(LLCCClick));
    return n;
}

// ---- the functions
int32_t tpw_ccs_Backtrack(uint64_t obj, int32_t fix) { return LLCC_SystemBacktrack(reinterpret_cast<CObject*>(obj), fix != 0); }
int32_t tpw_ccs_QueueWalk(uint64_t obj) { return LLCC_SystemConstructQueueClickStack(reinterpret_cast<CObject*>(obj)); }
void tpw_ccs_TrackWalk(uint64_t obj) { LLCC_SystemConstructTrackClickStack(reinterpret_cast<CObject*>(obj)); }
void tpw_ccs_ReverseWalk(uint64_t obj) { LLCC_SystemConstructReverseTrackClickStack(reinterpret_cast<CObject*>(obj)); }
uint32_t tpw_ccs_OffsetStart(int32_t* xy, int32_t rotation) { return LLCR_OffsetStartOfTrack(&xy[0], &xy[1], rotation); }
uint32_t tpw_ccs_OffsetEnd(int32_t* xy, int32_t rotation) { return LLCR_OffsetEndOfTrack(&xy[0], &xy[1], rotation); }
uint32_t tpw_ccs_IsStraight(uint64_t cell) { return reinterpret_cast<const CTrackCell*>(cell)->IsStraight(); }
uint32_t tpw_ccs_IsCrossRoad(uint64_t cell) { return reinterpret_cast<const CTrackCell*>(cell)->IsCrossRoad(); }
uint32_t tpw_ccs_IsCorner(uint64_t cell) { return reinterpret_cast<const CMapCell*>(cell)->IsCorner(); }

}  // extern "C"
