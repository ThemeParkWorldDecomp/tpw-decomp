// Test library for the cell system's left button release (unit v5883; libtpw_unit_LLCM_LeftMouseButtonUp.dylib,
// tests/test_LLCM_LeftMouseButtonUp.py), main app:
//   src/game/park/LLCellMouse.cpp   LLCM_LeftMouseButtonUp and the words gLLCC9F28, gLLCC9F2C, gLLCC9F44,
//   gLLCC9F48, gLLCCDB30, gLLCCDB44, gLLCCDB4C, gLLCCDB64
//
// Every callee is a fake that records what the real one reads and answers from the case's queues (cells are named by their offset
// from the world block, objects by label); the globals of other files the unit reads are defined here, zero at start like the Mac's bss.
// LLCC_DrawLine, LLCC_DrawTrackLine and LLCC_TraceTrack are declared void but the original reads their r3: their fakes answer an int
// under the declared function's symbol.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPCoaster.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
uint8_t gLLCC4818;
int32_t gLLCCWidth;
int32_t gLLCCHeight;
int32_t gLLCC4804;
int32_t gLLCC4800;
int32_t gLLCCC214;
int32_t gLLCCC208;
int32_t gLLCCDB6C;
uint16_t gLLCCDB68;
int32_t gLLCC47C8;
int32_t gHoardingRight;
int32_t gHoardingBottom;
uint8_t gLLCCE0CC;
int32_t gLLCCReviseAnswer;
int32_t gLLCC9F14;
uint16_t gLLCC9F24;
uint16_t gLLCC9F26;
int32_t gLLCCE288;
int32_t gLLCCE28C;
uint16_t gLLCCDB58;
uint16_t gLLCCDB5A;
int32_t gLLCCE0DC;
int32_t gLLCCFirstBlockX;
int32_t gLLCCFirstBlockY;
int32_t gLLCC9F18;
int32_t gLLCC9F1C;
int32_t gLLCC9F20;
int32_t gLLCCE034;
int32_t gLLCCE038;
CTrackCell* gLLCCDB48;
CObject* gLLCMCurrentObject;
int32_t gLLCCDE0B8;
uint8_t gLLCCDB34;
uint8_t gLLCCE090;
char gTrackCostFree;
int32_t gLLCCDB40;
int32_t gLLCCDB50;
int32_t gLLCCDB54;
int32_t gCalcNeighboursPrevA;
int32_t gCalcNeighboursPrevB;
CActionRecorder gActionRecorder;
UIInterface gUIInterface;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;

namespace {

uint8_t* gPeep;
uint8_t* gQueueBlock;
uint8_t* gTrackBlock;
uint8_t* gAdvisor;
uint8_t* gBank;

int64_t Ofs(const void* cell) {
    return static_cast<int64_t>(reinterpret_cast<const uint8_t*>(cell) - reinterpret_cast<const uint8_t*>(gGameWorld));
}

int32_t Ret(const char* key) { return static_cast<int32_t>(ret(key)); }

int32_t LineFake(const char* name, const char* moveKey, int32_t mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev(name, {mode, x, y, *endX, *endY});
    if (ret(moveKey) != 0) {
        *endX ^= 1;
        *endY ^= 1;
    }
    return Ret(name);
}

void MoveFake(const char* name, int32_t* x, int32_t* y) {
    if (ret(name) != 0) {
        *x ^= 1;
        *y ^= 1;
    }
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
int32_t FakeDrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) __asm__("__Z13LLCC_DrawLine10CELL_MODESiiPiS0_");
int32_t FakeDrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    return LineFake("DrawLine", "DrawLineMove", static_cast<int32_t>(mode), x, y, endX, endY);
}
int32_t FakeDrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) __asm__("__Z18LLCC_DrawTrackLine10CELL_MODESiiPiS0_");
int32_t FakeDrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    return LineFake("DrawTrackLine", "DrawTrackLineMove", static_cast<int32_t>(mode), x, y, endX, endY);
}
int32_t FakeTraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) __asm__("__Z15LLCC_TraceTrackP10CTrackCellS0_i");
int32_t FakeTraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) {
    ev("Trace", {Ofs(from), Ofs(to), flag});
    return Ret("Trace");
}

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void LLCM_MouseMoved(int32_t x, int32_t y, uint16_t flag) { ev("MouseMoved", {x, y, flag}); }
int32_t LLCM_GetIllegalPlacement() {
    ev("Illegal", {});
    return Ret("Illegal");
}
int32_t LLCC_SystemIsMode(int32_t mode) {
    ev("IsMode", {mode});
    return mode == Ret("mode") ? 1 : 0;
}
void CActionRecorder::RecordAction(RecordedAction action, uint8_t a, uint8_t b) { ev("Rec3", {static_cast<int32_t>(action), a, b}); }
void CActionRecorder::RecordAction(RecordedAction action, uint8_t a, uint8_t b, uint16_t c) {
    ev("Rec4", {static_cast<int32_t>(action), a, b, c});
}
CThing* UIInterface::GetSelectedPeep() {
    ev("Peep", {});
    return ret("Peep") != 0 ? reinterpret_cast<CThing*>(gPeep) : nullptr;
}
void LLCM_SetCurrentObject(bool flag) { ev("SetCurrentObject", {flag ? 1 : 0}); }
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND", {entity, bank, event, a, b, c});
    return 0;
}
int32_t CMapCell::ConnectionType(CELL_MODES mode, int32_t flag) {
    ev("MapCT", {Ofs(this), static_cast<int32_t>(mode), flag});
    return Ret("MapCT");
}
void LLCC_SystemResetClickStack() { ev("Reset", {}); }
int32_t LLCC_SystemAddToClickStack(int32_t x, int32_t y) {
    ev("Add", {x, y});
    return 1;
}
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("AdvReq", {static_cast<int32_t>(message), thingId.mIndex, static_cast<int64_t>(secondary), responseNo, overrideOnlyOnce ? 1 : 0, score});
}
CAdvisor* CWorld::GetAdvisor() {
    ev("GetAdvisor", {});
    return reinterpret_cast<CAdvisor*>(gAdvisor);
}
int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    ev("QueueRequest", {lab(this)});
    return 0;
}
int32_t LLCC_SystemGetClickStackEntries() {
    ev("Entries", {});
    return Ret("Entries");
}
int32_t LLCC_SystemGetMode() {
    ev("GetMode", {});
    return Ret("GetMode");
}
void CObject::RecalculateBackOfQueue() { ev("RecalcBOQ", {lab(this)}); }
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("SetMode", {mode, flag ? 1 : 0}); }
void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("SetModeNR", {mode, flag ? 1 : 0}); }
int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {Ofs(this)});
    return Ret("IsTrack");
}
int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {Ofs(this)});
    return Ret("IsTrackX");
}
int32_t CTrackCell::IsTrackStartBlock() const {
    ev("StartBlk", {Ofs(this)});
    return Ret("StartBlk");
}
int16_t CTrackCell::GetSegmentNumber() const {
    ev("Seg", {Ofs(this)});
    return static_cast<int16_t>(Ret("Seg"));
}
int32_t CTrackCell::ConnectionType(CELL_MODES mode, int32_t a, uint8_t b) {
    ev("TrackCT", {Ofs(this), static_cast<int32_t>(mode), a, b});
    return Ret("TrackCT");
}
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    ev("Nbr", {Ofs(this), direction, offset});
    return ret("Nbr") != 0 ? reinterpret_cast<CTrackCell*>(reinterpret_cast<uint8_t*>(this) + 0x28) : nullptr;
}
CMapCell* CCellBase::GetMapCellEquiv() {
    ev("MapEquiv", {Ofs(this)});
    const int64_t index = (Ofs(this) - 0x2d8 - 0x110000) / 0x28;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8 + index * 0x44);
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("ThingId", {Ofs(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("ThingId"));
    return id;
}
void CCellBase::ModifyDirection(uint8_t direction) { ev("ModDir", {Ofs(this), direction}); }
void CCellBase::ModifyNeighbours(uint8_t mask) { ev("ModNbr", {Ofs(this), mask}); }
uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return static_cast<uint8_t>((direction << 4) | (direction >> 4));
}
void BMP_FreeTrack(int32_t ride) { ev("FreeTrack", {ride}); }
int32_t LLCC_GetRideRotation() {
    ev("RideRot", {});
    return Ret("RideRot");
}
int32_t LLCR_PlaceRide(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t b, int32_t c) {
    ev("PlaceRide", {x, y, a, rotation, b, c});
    return Ret("PlaceRide");
}
// The ride placement takes the block whole from the body (unit v5L28): the fake answers the Mac's word under a high half no
// truncated word has, and the queue and track starts record whether the block they get kept it.
constexpr uintptr_t kPlacementHigh = static_cast<uintptr_t>(0x5a5a) << 32;
LLCRPlacement* LLCR_PlaceRideBlock(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t b, int32_t c) {
    ev("PlaceRide", {x, y, a, rotation, b, c});
    const uint32_t word = static_cast<uint32_t>(Ret("PlaceRide"));
    return word == 0 ? nullptr : reinterpret_cast<LLCRPlacement*>(kPlacementHigh | word);
}
// 1 for a block with the fake's high half, 0 for none, -1 for a word widened on its own.
int64_t WholeBlock(const LLCR_PRR* placement) {
    if (placement == nullptr) return 0;
    return (reinterpret_cast<uintptr_t>(placement) >> 32) == (kPlacementHigh >> 32) ? 1 : -1;
}
void LLCM_RebuildUpgradeBridges(int32_t x, int32_t y) { ev("Rebuild", {x, y}); }
uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffStart", {*x, *y, rotation});
    MoveFake("OffStartMove", x, y);
    return static_cast<uint8_t>(ret("OffStart"));
}
uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffEnd", {*x, *y, rotation});
    MoveFake("OffEndMove", x, y);
    return static_cast<uint8_t>(ret("OffEnd"));
}
int32_t LLCC_BuildTrackDB(bool flag) {
    ev("BuildDB", {flag ? 1 : 0});
    return Ret("BuildDB");
}
CObject* LLCM_GetCurrentObject() {
    ev("GetCurrent", {});
    if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C)) return gLLCMCurrentObject;
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
}
void LLCC_SetTrackAnim(CObject* object, bool flag) { ev("TrackAnim", {lab(object), flag ? 1 : 0}); }
void LLCC_SetTrackAnimReverse(CObject* object, bool flag) { ev("TrackAnimRev", {lab(object), flag ? 1 : 0}); }
int32_t CObject::IsOpenable() {
    ev("Openable", {lab(this)});
    return Ret("Openable");
}
void CObject::Reopen() { ev("Reopen", {lab(this)}); }
void LLCC_SetRideRotation(int32_t rotation, bool flag) { ev("SetRideRot", {rotation, flag ? 1 : 0}); }
int32_t LLCC_SystemGetRideQueue() {
    ev("RideQueue", {});
    return Ret("RideQueue");
}
int32_t LLCC_SystemGetRideTrack() {
    ev("RideTrack", {});
    return Ret("RideTrack");
}
CCellBase* LLCR_StartQueue(LLCR_PRR* placement, int32_t* x, int32_t* y) {
    ev("StartQueue", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(placement))), *x, *y, WholeBlock(placement)});
    *x = Ret("SQX");
    *y = Ret("SQY");
    return reinterpret_cast<CCellBase*>(gQueueBlock);
}
CTrackCell* LLCR_StartTrack(LLCR_PRR* placement, int32_t* x, int32_t* y, int32_t track) {
    ev("StartTrack", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(placement))), *x, *y, track, WholeBlock(placement)});
    *x = Ret("STX");
    *y = Ret("STY");
    return reinterpret_cast<CTrackCell*>(gTrackBlock);
}
uint8_t LLCR_FixBridges(CObject* ride, bool a, bool b, bool c) {
    ev("FixBridges", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return static_cast<uint8_t>(ret("FixBr"));
}
uint8_t LLCR_FixBridgesReverse(CObject* ride, bool a, bool b, bool c) {
    ev("FixBridgesRev", {lab(ride), a ? 1 : 0, b ? 1 : 0, c ? 1 : 0});
    return 0;
}
void LLCM_TrackCellBoundary(int32_t x, int32_t y, int32_t* cellX, int32_t* cellY, int32_t* outX, int32_t* outY) {
    ev("TCB", {x, y, *cellX, *cellY, *outX, *outY});
    const int32_t m = Ret("TCB");
    const int32_t vx = *cellX ^ m;
    const int32_t vy = *cellY ^ m;
    *outX = vx;
    *outY = vy;
}
void LLCC_CreateCurrentHoardingImage() { ev("HoardImage", {}); }
int32_t LLCC_IsHoardingEditLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool charge) {
    ev("HoardLegal", {x1, y1, x2, y2, charge ? 1 : 0});
    return Ret("HoardLegal");
}
void LLCC_ReviseHoarding(int32_t x1, int32_t y1, int32_t x2, int32_t y2) { ev("ReviseHoarding", {x1, y1, x2, y2}); }
int32_t LLCC_SystemGetSessionCost() {
    ev("Cost", {});
    return Ret("Cost");
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBank", {});
    return reinterpret_cast<CBankAccount*>(gBank);
}
void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("Withdraw", {lab(this), static_cast<int64_t>(amount)}); }
void UI_SetGameWindowCursor(int16_t cursor) { ev("Cursor", {cursor}); }
int32_t LLCC_IsClearLandLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    ev("Clear", {x1, y1, x2, y2});
    return Ret("Clear");
}
int32_t LLCC_Kill_Them_KILL_THEM_ALL(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    ev("Kill", {x1, y1, x2, y2});
    return 1;
}
void LLCR_DeleteRide(int32_t x, int32_t y, int32_t flag) { ev("DeleteRide", {x, y, flag}); }
void CStaff::SetPatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> bottomLeft, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> topRight) {
    ev("Patrol", {lab(this), bottomLeft.mIndex, topRight.mIndex});
}
int32_t LLCC_SystemConstructQueueClickStack(CObject* obj) {
    ev("QueueStack", {lab(obj)});
    return 0;
}
void LLCC_SystemConstructTrackClickStack(CObject* obj) { ev("TrackStack", {lab(obj)}); }
void LLCM_CoverUpgrade(int32_t* x, int32_t* y) {
    ev("Cover", {*x, *y});
    MoveFake("CovMove", x, y);
}
void LLCC_FixFirstBlock(bool flag) { ev("FixFirst", {flag ? 1 : 0}); }
void LLCC_SystemConstructHoardingClickStack() { ev("HoardStack", {}); }
void CoasterAcceptLeftMouseClick(int32_t down) { ev("CoasterClick", {down}); }
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, uint32_t* meshSlot) {
    ev("PlacePylon", {static_cast<int64_t>(x), static_cast<int64_t>(z), flag, Ofs(meshSlot)});
    return 0;
}
void EndTrack(int32_t flag) { ev("EndTrack", {flag}); }
uint32_t DoMovePylon(uint32_t x, uint32_t z) {
    ev("MovePylon", {static_cast<int64_t>(x), static_cast<int64_t>(z)});
    return static_cast<uint32_t>(ret("MovePylon"));
}
void AbortMovePylon() { ev("AbortPylon", {}); }
void LLCM_SetIllegalPlacement(int32_t placement) { ev("SetIllegal", {placement}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_llcmup_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"x0", &gLLCC4804},
        {"y0", &gLLCC4800},
        {"DB30", &gLLCCDB30},
        {"DE0B8", &gLLCCDE0B8},
        {"DB40", &gLLCCDB40},
        {"PrevA", &gCalcNeighboursPrevA},
        {"PrevB", &gCalcNeighboursPrevB},
        {"E288", &gLLCCE288},
        {"E28C", &gLLCCE28C},
        {"DB44", &gLLCCDB44},
        {"9F18", &gLLCC9F18},
        {"9F1C", &gLLCC9F1C},
        {"9F20", &gLLCC9F20},
        {"FBX", &gLLCCFirstBlockX},
        {"FBY", &gLLCCFirstBlockY},
        {"E034", &gLLCCE034},
        {"E038", &gLLCCE038},
        {"E0DC", &gLLCCE0DC},
        {"9F14", &gLLCC9F14},
        {"DB50", &gLLCCDB50},
        {"DB54", &gLLCCDB54},
        {"47C8", &gLLCC47C8},
        {"9F28", &gLLCC9F28},
        {"9F2C", &gLLCC9F2C},
        {"Revise", &gLLCCReviseAnswer},
        {"C214", &gLLCCC214},
        {"C208", &gLLCCC208},
        {"Width", &gLLCCWidth},
        {"Height", &gLLCCHeight},
        {"HoardRight", &gHoardingRight},
        {"HoardBottom", &gHoardingBottom},
        {"9F44", &gLLCC9F44},
        {"9F48", &gLLCC9F48},
        {"DB68", &gLLCCDB68},
        {"9F24", &gLLCC9F24},
        {"9F26", &gLLCC9F26},
        {"DB58", &gLLCCDB58},
        {"DB5A", &gLLCCDB5A},
        {"4818", &gLLCC4818},
        {"E0CC", &gLLCCE0CC},
        {"DB34", &gLLCCDB34},
        {"E090", &gLLCCE090},
        {"TCF", &gTrackCostFree},
        {"DB64", &gLLCCDB64},
        {"DB4C", &gLLCCDB4C},
        {"DB48", &gLLCCDB48},
        {"DB6C", &gLLCCDB6C},
        {"CurObj", &gLLCMCurrentObject},
        {"World", &gGameWorld},
        {"Rec", &gActionRecorder.mRecording},
        {"Bank3", &gSoundBanks[3]},
        {"Things", &gThingArray[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_llcmup_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The blocks the fakes answer: the selected peep, the queue start cell, the track start cell, the advisor and the bank account.
void tpw_llcmup_blocks(uint64_t peep, uint64_t queue, uint64_t track, uint64_t advisor, uint64_t bank) {
    gPeep = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(peep));
    gQueueBlock = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(queue));
    gTrackBlock = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(track));
    gAdvisor = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(advisor));
    gBank = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(bank));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_llcmup_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF24));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF28));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mMeshInstance));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

// ---- the function
void tpw_llcmup_call(int32_t x, int32_t y, uint32_t shift, int32_t flag) { LLCM_LeftMouseButtonUp(x, y, static_cast<uint16_t>(shift), flag); }

}  // extern "C"
