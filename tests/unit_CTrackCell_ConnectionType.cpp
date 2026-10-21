// Test library for CTrackCell::ConnectionType (unit v5629; libtpw_unit_CTrackCell_ConnectionType.dylib, tests/test_CTrackCell_ConnectionType.py),
// main app:
//   src/game/park/TrackCell.cpp   CTrackCell::ConnectionType and the words gLLCCC20C, gLLCCDB40, gLLCCDB60, gLLCCE0B4, gLLCCE0B8
//
// Every callee is a fake that records the arguments the real one reads and answers from the test's queues; the cells, rides and the bank
// account they answer are looked up by label in a table the module fills (tpw_ct_label). The globals of other files the unit reads are
// defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gHoardingRight;    // 0x1029e03c
int32_t gHoardingBottom;   // 0x1029e040
int32_t gHoardingLeft;     // 0x1029e044
int32_t gHoardingTop;      // 0x1029e048
int32_t gLLCCC20C;
int32_t gLLCCDB60;
int32_t gLLCCE0B4;
int32_t gLLCCE0B8;
int32_t gLLCCC200;
int32_t gLLCCC204;
int32_t gLLCCC210;
int32_t gLLCCC214;
uint16_t gLLCCDB68;
char gTrackCostFree;
int32_t gLLCC4858;
int32_t gTrackNbrOn485C;
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;
CActionRecorder gActionRecorder;
CRideDatabase gRideDatabase;
extern const MapDelta gMapDeltaPlusX = {1, 0, 0};
extern const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
extern const MapDelta gMapDeltaPlusY = {0, 1, 0};

namespace {

uintptr_t gLabelled[64];

void* At(int64_t label) {
    if (label <= 0 || label >= 64) return nullptr;
    return reinterpret_cast<void*>(gLabelled[label]);
}

int32_t Ans(const char* key, const void* object) { return static_cast<int32_t>(ret(key, lab(object), 0)); }

}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs; pointers as lab(p))
CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {lab(this)});
    return static_cast<CMapCell*>(At(ret("GetMapCellEquiv")));
}

CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    ev("MapNbr", {lab(this), delta->mX, delta->mY, flag});
    return static_cast<CMapCell*>(At(ret("MapNbr")));
}

int32_t CMapCell::IsRide() const {
    ev("IsRide", {lab(this)});
    return Ans("IsRide", this);
}

int32_t CMapCell::IsFreeform() const {
    ev("IsFreeform", {lab(this)});
    return Ans("IsFreeform", this);
}

int32_t CMapCell::IsExit() const {
    ev("IsExit", {lab(this)});
    return Ans("IsExit", this);
}

int32_t CMapCell::IsEntrance() const {
    ev("IsEntrance", {lab(this)});
    return Ans("IsEntrance", this);
}

int32_t CMapCell::IsQueue() const {
    ev("IsQueue", {lab(this)});
    return Ans("IsQueue", this);
}

int32_t CMapCell::IsFlag(uint16_t flag) const {
    ev("MapIsFlag", {lab(this), flag});
    return Ans(flag == 0x40 ? "MapIsFlag40" : "MapIsFlag20", this);
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(Ans("GetObjectThingId", this));
    return id;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return static_cast<RideInfo*>(At(ret("GetRideInfo")));
}

uint32_t LLCC_SystemGetKartTrackCost() {
    ev("GetKartTrackCost", {});
    return static_cast<uint32_t>(ret("GetKartTrackCost"));
}

uint32_t LLCC_SystemGetWaterTrackCost() {
    ev("GetWaterTrackCost", {});
    return static_cast<uint32_t>(ret("GetWaterTrackCost"));
}

int32_t LLCC_SystemGetSessionCost() {
    ev("GetSessionCost", {});
    return static_cast<int32_t>(ret("GetSessionCost"));
}

void LLCC_SystemAddToSessionCost(int32_t cost) { ev("AddToSessionCost", {cost}); }

void LLCM_SetIllegalPlacement(int32_t placement) { ev("SetIllegalPlacement", {placement}); }

CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("TrackNbr", {lab(this), delta.mX, delta.mY, offset});
    return static_cast<CTrackCell*>(At(ret("TrackNbr")));
}

int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {lab(this)});
    return Ans("IsTrackX", this);
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {lab(this)});
    return Ans("IsTrack", this);
}

int32_t CTrackCell::IsBridge() const {
    ev("IsBridge", {lab(this)});
    return static_cast<int32_t>(ret("IsBridge"));
}

int32_t CTrackCell::IsFlag(uint16_t flag) const {
    ev("TrackIsFlag", {lab(this), flag});
    return static_cast<int32_t>(ret(flag == 0x40 ? "TrackIsFlag40" : "TrackIsFlag10"));
}

uint8_t CTrackCell::IsStraight() const {
    ev("IsStraight", {lab(this)});
    return static_cast<uint8_t>(ret("IsStraight"));
}

uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {lab(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}

uint8_t CTrackCell::IsCrossRoad() const {
    ev("IsCrossRoad", {lab(this)});
    return static_cast<uint8_t>(ret("IsCrossRoad"));
}

int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return static_cast<uint8_t>(ret("OPPOSITE"));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return static_cast<CBankAccount*>(At(ret("GetBankAccount")));
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ct_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gHoardingRight", &gHoardingRight},
        {"gHoardingBottom", &gHoardingBottom},
        {"gHoardingLeft", &gHoardingLeft},
        {"gHoardingTop", &gHoardingTop},
        {"gLLCCC200", &gLLCCC200},
        {"gLLCCC204", &gLLCCC204},
        {"gLLCCC20C", &gLLCCC20C},
        {"gLLCCC210", &gLLCCC210},
        {"gLLCCC214", &gLLCCC214},
        {"gLLCCDB40", &gLLCCDB40},
        {"gLLCCDB60", &gLLCCDB60},
        {"gLLCCDB68", &gLLCCDB68},
        {"gTrackCostFree", &gTrackCostFree},
        {"gLLCCE0B4", &gLLCCE0B4},
        {"gLLCCE0B8", &gLLCCE0B8},
        {"gLLCC4858", &gLLCC4858},
        {"gTrackNbrOn485C", &gTrackNbrOn485C},
        {"gThingArray", &gThingArray[0]},
        {"gGameWorld", &gGameWorld},
        {"recording", &gActionRecorder.mRecording},
        {"gRideDatabase", &gRideDatabase},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_ct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The object a fake answers for a label (0 clears it).
void tpw_ct_label(int32_t label, uint64_t address) {
    if (label > 0 && label < 64) gLabelled[label] = static_cast<uintptr_t>(address);
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_ct_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mDirection));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    // the host-only sizes (checked separately by the module): a thing table row, a ride record, the slot word of the overwrite priority
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    alignas(RideInfo) static unsigned char record[sizeof(RideInfo)];
    RideInfo* ride = reinterpret_cast<RideInfo*>(record);
    out[n++] = static_cast<int64_t>(reinterpret_cast<unsigned char*>(&ride->Value(kRideSlot_OverwritePriority).mInteger) - record);
    return n;
}

int32_t tpw_ct_call(uint64_t cell, int32_t mode, int32_t a, uint32_t b) {
    return reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(cell))->ConnectionType(static_cast<CELL_MODES>(mode), a, static_cast<uint8_t>(b));
}

}  // extern "C"
