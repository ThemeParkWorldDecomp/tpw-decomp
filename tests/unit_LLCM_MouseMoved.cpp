// Test library for LLCM_MouseMoved (unit v5633; libtpw_unit_LLCM_MouseMoved.dylib, tests/test_LLCM_MouseMoved.py), main app:
//   src/LLCellMeshes_LLCM_RebuildUpgradeBridges_LLCM_MouseMoved.cpp   LLCM_MouseMoved and the ten words it defines
//
// Every callee is a fake that records the arguments the real one reads (cells as their byte offset from gGameWorld, objects as labels,
// the words behind out pointers) and answers from the test's queues; the line and boundary fakes move their out words by queued deltas.
// The globals of other files the function reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "CellBase.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
int32_t gLLCCC214;
int32_t gLLCCC210;
int32_t gLLCCC208;
int32_t gLLCCC204;
int32_t gLLCCC200;
int32_t gLLCCDB54;
int32_t gLLCCDB50;
char gTrackCostFree;
uint16_t gLLCCE0E0;
int32_t gLLCCE0DC;
int32_t gLLCC9F18;
int32_t gLLCC9F1C;
int32_t gLLCC47C8;
int32_t gLLCCDB6C;
CObject* gLLCMCurrentObject;
int32_t gDynamicCellF9BD4;
int32_t gLLCCWidth;
int32_t gLLCCHeight;
int32_t gHoardingRight;
int32_t gHoardingBottom;
uint16_t gLLCCDB68;
uint16_t gLLCC9F24;
uint16_t gLLCC9F26;
int32_t gLLCCReviseAnswer;
uint16_t gLLCCDB58;
uint16_t gLLCCDB5A;
uint8_t gLLCCE090;
int32_t gLLCC9F14;
int32_t gLLCC4804;
int32_t gLLCC4800;
CWorld* gGameWorld;
CActionRecorder gActionRecorder;
CThingArrayEntry gThingArray[kThingArraySize];
CRideDatabase gRideDatabase;

// The blocks the fakes answer (set by the test through tpw_mm_addr).
void* gMMFakeRide;
void* gMMFakeBank;

namespace {

int64_t Off(const void* p) {
    return static_cast<int64_t>(reinterpret_cast<const uint8_t*>(p) - reinterpret_cast<const uint8_t*>(gGameWorld));
}

}  // namespace

// ---- the fakes
void LLCM_SetIllegalPlacement(int32_t placement) { ev("LLCM_SetIllegalPlacement", {placement}); }
void DC_ResetSquareFaceListPtr() { ev("DC_ResetSquareFaceListPtr", {}); }
int32_t LLCC_SystemIsMode(int32_t mode) {
    ev("LLCC_SystemIsMode", {mode});
    return ret("Mode") == mode ? 1 : 0;
}
int32_t LLCC_SystemGetMode() {
    ev("LLCC_SystemGetMode", {});
    return static_cast<int32_t>(ret("GetMode"));
}
int32_t LLCC_SystemGetLastMode() {
    ev("LLCC_SystemGetLastMode", {});
    return static_cast<int32_t>(ret("GetLastMode"));
}
void LLCC_DrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("LLCC_DrawLine", {static_cast<int32_t>(mode), x, y, *endX, *endY});
    *endX += static_cast<int32_t>(ret("LineDX"));
    *endY += static_cast<int32_t>(ret("LineDY"));
}
void LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("LLCC_DrawTrackLine", {static_cast<int32_t>(mode), x, y, *endX, *endY});
    *endX += static_cast<int32_t>(ret("LineDX"));
    *endY += static_cast<int32_t>(ret("LineDY"));
}
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("CRideDatabase::GetRideInfo", {lab(this), id.mIndex});
    return static_cast<RideInfo*>(gMMFakeRide);
}
bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("CGoldenTicketControl::HasBeenBought", {lab(this), type.mIndex});
    return (ret("Bought") & 0xff) != 0;
}
void LLCC_SystemSetSessionCost(int32_t cost) { ev("LLCC_SystemSetSessionCost", {cost}); }
CBankAccount* CWorld::GetBankAccount() {
    ev("CWorld::GetBankAccount", {lab(this)});
    return static_cast<CBankAccount*>(gMMFakeBank);
}
int32_t CBankAccount::GetBalance() const {
    ev("CBankAccount::GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("Balance"));
}
int32_t LLCC_SystemGetSessionCost() {
    ev("LLCC_SystemGetSessionCost", {});
    return static_cast<int32_t>(ret("SessionCost"));
}
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("CMap::IsXYValid", {x, y});
    return static_cast<int32_t>(ret("XYValid"));
}
int32_t LLCC_GetRideRotation() {
    ev("LLCC_GetRideRotation", {});
    return static_cast<int32_t>(ret("Rotation"));
}
void LLCM_TrackCellBoundary(int32_t x, int32_t y, int32_t* cellX, int32_t* cellY, int32_t* outX, int32_t* outY) {
    ev("LLCM_TrackCellBoundary", {x, y, *cellX, *cellY, *outX, *outY});
    *cellX += static_cast<int32_t>(ret("BX"));
    *cellY += static_cast<int32_t>(ret("BY"));
    *outX += static_cast<int32_t>(ret("BX2"));
    *outY += static_cast<int32_t>(ret("BY2"));
}
int32_t CTrackCell::IsTrackX() const {
    ev("CTrackCell::IsTrackX", {Off(this)});
    return static_cast<int32_t>(ret("IsTrackX"));
}
int32_t CTrackCell::IsTrack() const {
    ev("CTrackCell::IsTrack", {Off(this)});
    return static_cast<int32_t>(ret("TrackIsTrack"));
}
uint8_t CCellBase::GetDirection() const {
    ev("CCellBase::GetDirection", {Off(this)});
    return static_cast<uint8_t>(ret("Direction"));
}
void LLCC_SetRideRotation(int32_t rotation, bool flag) { ev("LLCC_SetRideRotation", {rotation, flag ? 1 : 0}); }
int32_t LLCR_PlaceRide(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t b, int32_t c) {
    ev("LLCR_PlaceRide", {x, y, a, rotation, b, c});
    return static_cast<int32_t>(ret("PlaceRide"));
}
int32_t LLCC_IsHoardingEditLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool charge) {
    ev("LLCC_IsHoardingEditLegal", {x1, y1, x2, y2, charge ? 1 : 0});
    return static_cast<int32_t>(ret("Legal"));
}
int32_t LLCC_IsClearLandLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    ev("LLCC_IsClearLandLegal", {x1, y1, x2, y2});
    return static_cast<int32_t>(ret("Legal"));
}
bool LLCC_IsPatrolAreaLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    ev("LLCC_IsPatrolAreaLegal", {x1, y1, x2, y2});
    return ret("Legal") != 0;
}
void LLCM_StartTrackEditMode(int32_t x, int32_t y, uint16_t unused) { ev("LLCM_StartTrackEditMode", {x, y, unused}); }
void LLCC_SystemConstructHoardingClickStack() { ev("LLCC_SystemConstructHoardingClickStack", {}); }
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("LLCM_LeftMouseButtonUp", {x, y, shift, flag}); }
void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("LLCC_SystemSetModeNR", {mode, flag ? 1 : 0}); }
int16_t CTrackCell::GetSegmentNumber() const {
    ev("CTrackCell::GetSegmentNumber", {Off(this)});
    return static_cast<int16_t>(ret("Segment"));
}
int32_t CTrackCell::ConnectionType(CELL_MODES mode, int32_t a, uint8_t b) {
    ev("CTrackCell::ConnectionType", {Off(this), static_cast<int32_t>(mode), a, b});
    return static_cast<int32_t>(ret("Conn"));
}
void LLCC_TraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) { ev("LLCC_TraceTrack", {Off(from), Off(to), flag}); }
int32_t IsValidPylonPosition(uint32_t x, uint32_t z, void* a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    ev("IsValidPylonPosition", {static_cast<int64_t>(x), static_cast<int64_t>(z), lab(a), static_cast<int64_t>(b), static_cast<int64_t>(c),
                                static_cast<int64_t>(d), static_cast<int64_t>(e)});
    return static_cast<int32_t>(ret("Pylon"));
}
void GetPotentialPylonPositionArea(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    ev("GetPotentialPylonPositionArea", {});
    *a = static_cast<uint32_t>(ret("AreaX0"));
    *b = static_cast<uint32_t>(ret("AreaY0"));
    *c = static_cast<uint32_t>(ret("AreaX1"));
    *d = static_cast<uint32_t>(ret("AreaY1"));
}
int32_t CMapCell::IsFlag(uint16_t flag) const {
    ev("CMapCell::IsFlag", {Off(this), flag});
    return static_cast<int32_t>(ret(flag == 0x40 ? "Flag40" : "Flag20"));
}
int32_t CMapCell::IsHoarding() const {
    ev("CMapCell::IsHoarding", {Off(this)});
    return static_cast<int32_t>(ret("Hoarding"));
}
int32_t CMapCell::IsTrack() const {
    ev("CMapCell::IsTrack", {Off(this)});
    return static_cast<int32_t>(ret("MapIsTrack"));
}
int32_t CMapCell::IsEntrance() const {
    ev("CMapCell::IsEntrance", {Off(this)});
    return static_cast<int32_t>(ret("Entrance"));
}
int32_t CMapCell::IsExit() const {
    ev("CMapCell::IsExit", {Off(this)});
    return static_cast<int32_t>(ret("Exit"));
}
int32_t CMapCell::IsQueue() const {
    ev("CMapCell::IsQueue", {Off(this)});
    return static_cast<int32_t>(ret("Queue"));
}
int32_t CMapCell::IsRide() const {
    ev("CMapCell::IsRide", {Off(this)});
    return static_cast<int32_t>(ret("IsRide"));
}
int32_t CMapCell::IsFreeform() const {
    ev("CMapCell::IsFreeform", {Off(this)});
    return static_cast<int32_t>(ret("Freeform"));
}
int32_t CMapCell::IsPath() const {
    ev("CMapCell::IsPath", {Off(this)});
    return static_cast<int32_t>(ret("Path"));
}
int16_t CMapCell::GetPylonIndex() const {
    ev("CMapCell::GetPylonIndex", {Off(this)});
    return static_cast<int16_t>(ret("PylonIndex"));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("CMapCell::GetObjectThingId", {Off(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("ThingId"));
    return id;
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void LLCC_SystemSetCursor() { ev("LLCC_SystemSetCursor", {}); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_mm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"C214", &gLLCCC214}, {"C210", &gLLCCC210}, {"C20C", &gLLCCC20C}, {"C208", &gLLCCC208}, {"C204", &gLLCCC204},
        {"C200", &gLLCCC200}, {"DB54", &gLLCCDB54}, {"DB50", &gLLCCDB50}, {"DE0B8", &gLLCCDE0B8}, {"DE0B4", &gLLCCDE0B4},
        {"4804", &gLLCC4804}, {"4800", &gLLCC4800}, {"CostFree", &gTrackCostFree}, {"E0E0", &gLLCCE0E0}, {"E0DC", &gLLCCE0DC},
        {"9F34", &gLLCC9F34}, {"9F18", &gLLCC9F18}, {"9F1C", &gLLCC9F1C}, {"47C8", &gLLCC47C8}, {"DB3C", &gLLCCDB3C},
        {"DB38", &gLLCCDB38}, {"F9BD4", &gDynamicCellF9BD4}, {"9F38", &gLLCC9F38}, {"9F3C", &gLLCC9F3C}, {"Width", &gLLCCWidth},
        {"Height", &gLLCCHeight}, {"HRight", &gHoardingRight}, {"HBottom", &gHoardingBottom}, {"DB68", &gLLCCDB68},
        {"9F40", &gLLCC9F40}, {"9F24", &gLLCC9F24}, {"9F26", &gLLCC9F26}, {"Revise", &gLLCCReviseAnswer}, {"DB58", &gLLCCDB58},
        {"DB5A", &gLLCCDB5A}, {"E090", &gLLCCE090}, {"9F14", &gLLCC9F14}, {"DB60", &gLLCCDB60},
        {"Recording", &gActionRecorder.mRecording}, {"DB6C", &gLLCCDB6C}, {"CurObj", &gLLCMCurrentObject},{"GameWorld", &gGameWorld}, {"ThingArray", &gThingArray[0]},
        {"RideDB", &gRideDatabase}, {"FakeRide", &gMMFakeRide}, {"FakeBank", &gMMFakeBank},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable).
uint64_t tpw_mm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_mm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    return n;
}

// Host sizes that differ from the Mac's: a thing table row, a ride record, and the host offset of a ride record's balance slot.
int64_t tpw_mm_size(int32_t which) {
    if (which == 0) return static_cast<int64_t>(sizeof(CThingArrayEntry));
    if (which == 1) return static_cast<int64_t>(sizeof(RideInfo));
    return 0;
}

int64_t tpw_mm_slot(int32_t slot) {
    RideInfo* ride = nullptr;
    return static_cast<int64_t>(reinterpret_cast<uintptr_t>(&ride->Value(static_cast<RideBalanceSlot>(slot))));
}

void tpw_mm_moved(int32_t x, int32_t y, uint32_t flag) { LLCM_MouseMoved(x, y, static_cast<uint16_t>(flag)); }

}  // extern "C"
