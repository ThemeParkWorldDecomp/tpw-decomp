// Test library for LLCR_DeleteRide (unit v5999; libtpw_unit_LLCR_DeleteRide_2.dylib, tests/test_LLCR_DeleteRide_2.py), main app:
//   src/game/park/LLCellRide.cpp   LLCR_DeleteRide and the word gLLCRNoRide
//
// The callees outside the unit are fakes that record the arguments the real ones read (pointers as codes, see Code) and answer from the
// test's queues. The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "World.h"
#include "ActionRecorder.h"
#include "BMP_InsideArea.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];
CActionRecorder gActionRecorder;
int32_t gSoundBanks[8];
int32_t gLLCC47E0 = 0;
int32_t gLLCC47E4 = 0;

namespace {

struct Ctx {
    uintptr_t obj, info, bank, track, world, cells, nbr[3];
} gCtx;

// A pointer as a small code both sides compute alike: 0 NULL, 1 the ride, 2 its record, 3 the bank account, 4 the track cell, 5 the world,
// 100 + n the map cell n, 1000 + k the neighbour block k, else -1.
int64_t Code(const void* pointer) {
    uintptr_t a = reinterpret_cast<uintptr_t>(pointer);
    if (a == 0) return 0;
    if (a == gCtx.obj) return 1;
    if (a == gCtx.info) return 2;
    if (a == gCtx.bank) return 3;
    if (a == gCtx.track) return 4;
    if (a == gCtx.world) return 5;
    if (a >= gCtx.cells && a < gCtx.cells + 0x44u * 4400u && (a - gCtx.cells) % 0x44u == 0) return 100 + static_cast<int64_t>((a - gCtx.cells) / 0x44u);
    for (int k = 0; k < 3; k++)
        if (a == gCtx.nbr[k]) return 100000 + k;
    return -1;
}

int64_t Q(const char* key) { return static_cast<int32_t>(ret(key)); }

uint8_t Rol8(uint8_t value, int n) { return static_cast<uint8_t>((value << n) | (value >> (8 - n))); }

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
int32_t LLCC_SystemGetLastMode() {
    ev("GetLastMode", {});
    return static_cast<int32_t>(Q("mode"));
}

void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y) {
    ev("OverRide", {*x, *y});
    *out = static_cast<uint16_t>(Q("ovr"));
    *x = static_cast<int32_t>(Q("ox"));
    *y = static_cast<int32_t>(Q("oy"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    bool same = std::strcmp(format, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n") == 0;
    ev("Assert", {condition & 0xff, same ? 1 : 0});
}

int32_t CObject::GetResidualValuePercentage() const {
    ev("Residual", {Code(this)});
    return static_cast<int32_t>(Q("pct"));
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {Code(this)});
    return reinterpret_cast<RideInfo*>(gCtx.info);
}

int32_t LLCC_SystemConstructQueueClickStack(CObject* obj) {
    ev("ConstructQueue", {Code(obj), gLLCC47E0, gLLCC47E4});
    return 0;
}

void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("SetMode", {mode, flag ? 1 : 0, gLLCC47E0, gLLCC47E4}); }

void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("SetModeNR", {mode, flag ? 1 : 0, gLLCC47E0, gLLCC47E4}); }

int32_t LLCC_SystemBacktrack(CObject* obj, bool flag) {
    ev("Backtrack", {Code(obj), flag ? 1 : 0, gLLCC47E0, gLLCC47E4});
    return static_cast<int32_t>(Q("bt"));
}

int32_t LLCC_SystemGetQueueCost() {
    ev("QueueCost", {});
    return static_cast<int32_t>(Q("qcost"));
}

uint32_t LLCC_SystemGetKartTrackCost() {
    ev("KartCost", {});
    return static_cast<uint32_t>(Q("kcost"));
}

uint32_t LLCC_SystemGetWaterTrackCost() {
    ev("WaterCost", {});
    return static_cast<uint32_t>(Q("wcost"));
}

void LLCC_SystemResetClickStack() { ev("ResetClickStack", {gLLCC47E0, gLLCC47E4}); }

void LLCC_SystemConstructTrackClickStack(CObject* obj) { ev("ConstructTrack", {Code(obj), gLLCC47E0, gLLCC47E4}); }

void LLCC_SystemConstructReverseTrackClickStack(CObject* obj) { ev("ConstructReverse", {Code(obj), gLLCC47E0, gLLCC47E4}); }

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBank", {Code(this)});
    return reinterpret_cast<CBankAccount*>(gCtx.bank);
}

void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("Withdraw", {Code(this), static_cast<int32_t>(amount), gLLCC47E0, gLLCC47E4}); }

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(Q("valid"));
}

int32_t CMapCell::IsRide() const {
    ev("IsRide", {Code(this)});
    return static_cast<int32_t>(Q("isride"));
}

int32_t CMapCell::IsEntrance() const {
    ev("IsEntrance", {Code(this)});
    return static_cast<int32_t>(Q("isent"));
}

int32_t CMapCell::IsExit() const {
    ev("IsExit", {Code(this)});
    return static_cast<int32_t>(Q("isexit"));
}

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {Code(this)});
    return static_cast<uint8_t>(Q("dir"));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Code(this)});
    return static_cast<uint8_t>(Q("nb"));
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return Rol8(direction, 4);
}

uint8_t ADD_DIRECTION(uint8_t first, uint8_t second) {
    ev("ADD_DIRECTION", {first, second});
    return Rol8(first, 2);
}

CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    int64_t self = Code(this);
    ev("GetNeighbouringCell", {self, direction});
    int32_t k = static_cast<int32_t>(Q(self >= 100 && self < 100000 ? "nbrA" : "nbrB"));
    return k == 0 ? nullptr : reinterpret_cast<CMapCell*>(gCtx.nbr[k - 1]);
}

int32_t CMapCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {Code(this), flag});
    return static_cast<int32_t>(Q("isflag"));
}

void CMapCell::UnSetFlag(uint16_t flag) { ev("UnSetFlag", {Code(this), flag}); }

void CMapCell::Delete(int32_t a, int32_t b) { ev("CellDelete", {Code(this), a, b}); }

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {Code(this)});
    return reinterpret_cast<CTrackCell*>(gCtx.track);
}

void CTrackCell::UnSetFlag(uint16_t flags) { ev("TrackUnSetFlag", {Code(this), flags}); }

void CTrackCell::Delete(int32_t a, int32_t b, bool c) { ev("TrackDelete", {Code(this), a, b, c ? 1 : 0}); }

void BMP_DestroyRide(int32_t ride) { ev("DestroyRide", {ride}); }

void CThing::KillMe() { ev("KillMe", {Code(this)}); }

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SendEvent", {entity, bank, event, a, b, c});
    return 0;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_dr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"e0", &gLLCC47E0},
        {"e4", &gLLCC47E4},
        {"things", gThingArray},
        {"world", &gGameWorld},
        {"recorder", &gActionRecorder},
        {"banks", gSoundBanks},
        {"norid", &gLLCRNoRide},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_dr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_dr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF24));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF3C));
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    const int64_t values = static_cast<int64_t>(offsetof(RideInfo, mBalance) + offsetof(CRideBalance, mValues));
    out[n++] = values + static_cast<int64_t>(sizeof(BalanceValue)) * kRideSlot_Shape;
    out[n++] = values + static_cast<int64_t>(sizeof(BalanceValue)) * kRideSlot_HasQueue;
    out[n++] = values + static_cast<int64_t>(sizeof(BalanceValue)) * kRideSlot_WhichTrackType;
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mStatusFlags));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mRecording));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase));
    return n;
}

// What the fakes name by code: the ride, its record, the bank account, the track cell, the world, the map cells and three neighbour blocks.
void tpw_dr_ctx(uint64_t obj, uint64_t info, uint64_t bank, uint64_t track, uint64_t world, uint64_t n0, uint64_t n1, uint64_t n2) {
    gCtx.obj = static_cast<uintptr_t>(obj);
    gCtx.info = static_cast<uintptr_t>(info);
    gCtx.bank = static_cast<uintptr_t>(bank);
    gCtx.track = static_cast<uintptr_t>(track);
    gCtx.world = static_cast<uintptr_t>(world);
    gCtx.cells = static_cast<uintptr_t>(world) + 0x2d8;
    gCtx.nbr[0] = static_cast<uintptr_t>(n0);
    gCtx.nbr[1] = static_cast<uintptr_t>(n1);
    gCtx.nbr[2] = static_cast<uintptr_t>(n2);
}

// ---- the function
void tpw_dr_delete(int32_t x, int32_t y, int32_t flag) { LLCR_DeleteRide(x, y, flag); }

}  // extern "C"
