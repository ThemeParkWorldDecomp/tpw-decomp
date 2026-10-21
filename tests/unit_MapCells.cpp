// Differential library for unit w139: the six CWorld::Register overloads (src/game/World.cpp), CMap::Init (src/game/park/Map.cpp),
// CMapCell::SetHFieldTile (src/game/park/MapCell.cpp), MapDelta::Rotate (src/game/park/MapDelta.cpp), GetMapCellFromPointerWorldPosn
// (src/game/TPPointer.cpp) and CTrackCell::ModifyType (src/game/park/TrackCell.cpp).
//
// Every callee outside them is a fake that records the same event as the PowerPC side's stub (see tests/test_MapCells.py): GetThingId, GetTile,
// SingleHFieldTextureChange, the queue mesh calls, IsXYValid, the track cost getters, ValidTypeChange, IsJustTrack, GetMapCellEquiv,
// GetNeighbouringCell, Delete, the map's time stamps, the bank account (GetBankAccount, GetBalance, MakeWithdrawal) and the assertion.
// The blocks the functions work on are static storage (tpw_mc_addr gives the test their addresses); the test loads and dumps their bytes.
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "ActionRecorder.h"
#include "CellBase.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "intrface.h"
#include "m3d2.h"
#include "platform/arena.h"

using namespace calllog;

// ---- the main balance (raw storage under the symbol's name)
#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};

// ---- the unit's globals (the launcher gets zero definitions of the first three)
CCellBase::CELL_IndexType gLLCC4824;
char gTrackCostFree;
int32_t gLLCC4858;
CWorld* gGameWorld;
tag_sM3D2Header* gHeightfieldHeader;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
CIntrface gIntrface;
CActionRecorder gActionRecorder;

namespace {

constexpr uint32_t kInitSize = 0x1d9170;

alignas(16) unsigned char gRegTail[16];             // the world's bytes 0x1da738 to 0x1da748
alignas(16) unsigned char gRegThing[0x420];
alignas(16) unsigned char gInitBlock[kInitSize + 0x20];
alignas(16) unsigned char gTileBlock[0x20 + 0x44 + 0x20];
alignas(16) unsigned char gTrackBlock[0x20 + 0x28 + 0x20];
alignas(16) unsigned char gWorldBlock[0x300];        // the world a ModifyType case sees: only its address is used
alignas(16) unsigned char gAccount[0x20];
alignas(16) unsigned char gNeighbours[4][0x44];

CMapCell* Neighbour(int64_t k) { return k >= 1 && k <= 4 ? reinterpret_cast<CMapCell*>(gNeighbours[k - 1]) : nullptr; }
CWorld* RegWorld() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gRegTail) - 0x1da738); }

uint32_t Word(const void* p, int32_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + offset, 4);
    return v;
}

}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId", 0));
    return id;
}

uint32_t CMapCell::GetTile(int32_t* rotation) {
    ev("GetTile", {lab(this), static_cast<int32_t>(mType)});
    *rotation = static_cast<int32_t>(ret("rotation", 0));
    return static_cast<uint32_t>(ret("tile", 0));
}
void SingleHFieldTextureChange(int32_t x, int32_t y, const CCellBase::CELL_IndexType* tile) {
    ev("SingleHFieldTextureChange", {x, y, lab(tile), tile->mKind, tile->mIndex, tile->mRotation});
}
uint32_t LLCCM_CreateQMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    ev("LLCCM_CreateQMesh", {x, y, index.mKind, index.mIndex, index.mRotation, restore});
    return static_cast<uint32_t>(ret("mesh", 0));
}
void LLCCM_DestroyQMesh(uint32_t mesh) { ev("LLCCM_DestroyQMesh", {mesh}); }

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid", 0));
}
uint32_t CMap::MakeNewTimeStamp() {
    ev("MakeNewTimeStamp", {lab(this)});
    return static_cast<uint32_t>(ret("stamp", 0));
}
void CMap::SetTimeStamp(uint32_t stamp, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetTimeStamp", {lab(this), stamp, cell.mIndex});
}

uint32_t LLCC_SystemGetKartTrackCost() {
    ev("GetKartTrackCost");
    return static_cast<uint32_t>(ret("kart", 0));
}
uint32_t LLCC_SystemGetWaterTrackCost() {
    ev("GetWaterTrackCost");
    return static_cast<uint32_t>(ret("water", 0));
}
int32_t CTrackCell::ValidTypeChange(CELL_MODES mode) {
    ev("ValidTypeChange", {lab(this), static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("valid", 1));
}
int32_t CTrackCell::IsJustTrack() const {
    ev("IsJustTrack", {lab(this)});
    return static_cast<int32_t>(ret("just", 0));
}
CMapCell* CCellBase::GetMapCellEquiv() {   // the NON-const overload since unit w179 (0x100f49a0: ModifyType calls this one)
    ev("GetMapCellEquiv", {lab(this)});
    return Neighbour(ret("equiv", 0));
}
CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    ev("GetNeighbouringCell", {lab(this), delta->mX, delta->mY, flag});
    return Neighbour(ret("neighbour", 0));
}
void CMapCell::Delete(int32_t a, int32_t b) { ev("Delete", {lab(this), a, b}); }

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gAccount);
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("balance", 0));
}
void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("MakeWithdrawal", {lab(this), amount}); }

// ---- the test's entry points
extern "C" {

void* tpw_mc_addr(int32_t which) {
    switch (which) {
        case 0: return gRegTail;
        case 1: return gRegThing;
        case 2: return gInitBlock;
        case 3: return gTileBlock + 0x20;
        case 4: return gTrackBlock + 0x20;
        case 5: return gWorldBlock;
        case 6: return gAccount;
        case 7: return &gLLCC4824;
        case 8: return gTileBlock;
        case 9: return gTrackBlock;
        default: return gNeighbours[which - 10];
    }
}

void tpw_mc_register(int32_t kind) {
    CWorld* const w = RegWorld();
    void* const thing = gRegThing;
    switch (kind) {
        case 0: w->Register(static_cast<CHandyman*>(thing)); break;
        case 1: w->Register(static_cast<CMechanic*>(thing)); break;
        case 2: w->Register(static_cast<CEntertainer*>(thing)); break;
        case 3: w->Register(static_cast<CGuard*>(thing)); break;
        case 4: w->Register(static_cast<CResearcher*>(thing)); break;
        default: w->Register(static_cast<CObject*>(thing)); break;
    }
}

void tpw_mc_set_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }
void tpw_mc_init() { reinterpret_cast<CMap*>(gInitBlock)->Init(); }

void tpw_mc_tile() { reinterpret_cast<CMapCell*>(gTileBlock + 0x20)->SetHFieldTile(); }

void tpw_mc_rotate(int32_t x, int32_t y, int32_t rotation, int32_t* out) {
    const MapDelta d = {x, y, 0};
    const MapDelta r = d.Rotate(rotation);
    out[0] = r.mX;
    out[1] = r.mY;
}

// mode 0: no header; 1: a header without a field; 2: a header with a field (origin f0, f1, size f2, f3, counts c0, c1).
void tpw_mc_field(int32_t mode, float f0, float f1, float f2, float f3, uint32_t c0, uint32_t c1, int32_t width, int32_t height) {
    if (gHeightfieldHeader != nullptr) {
        if (gHeightfieldHeader->mHField != nullptr) tpw::ArenaFree(gHeightfieldHeader->mHField.get());
        tpw::ArenaFree(gHeightfieldHeader);
        gHeightfieldHeader = nullptr;
    }
    gHeightfieldWidth = width;
    gHeightfieldHeight = height;
    if (mode == 0) return;
    gHeightfieldHeader = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    if (mode == 1) return;
    sHField* const field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    field->mF08[0] = f0;
    field->mF08[1] = f1;
    field->mF08[2] = f2;
    field->mF08[3] = f3;
    field->mF18[0] = c0;
    field->mF18[1] = c1;
    gHeightfieldHeader->mHField = field;
}
void tpw_mc_set_f24(uint32_t v) { gIntrface.mF24 = static_cast<uint16_t>(v); }
uint32_t tpw_mc_get_f24() { return gIntrface.mF24; }
void tpw_mc_pointer(float x, float y, float z) {
    s3DPoint p;
    p.x = x;
    p.y = y;
    p.z = z;
    GetMapCellFromPointerWorldPosn(&p);
}

void tpw_mc_set_globals(int32_t trackFree, int32_t flag4858, int32_t recording) {
    gTrackCostFree = static_cast<char>(trackFree);
    gLLCC4858 = flag4858;
    gActionRecorder.mRecording = recording;
    gGameWorld = reinterpret_cast<CWorld*>(gWorldBlock);
}
void tpw_mc_modify(int32_t mode) { reinterpret_cast<CTrackCell*>(gTrackBlock + 0x20)->ModifyType(static_cast<CELL_MODES>(mode)); }

}  // extern "C"
