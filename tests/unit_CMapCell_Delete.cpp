// Test library for CMapCell::Delete (unit v5211; libtpw_unit_CMapCell_Delete.dylib, tests/test_CMapCell_Delete.py), main app:
//   src/MapDelta_map_CMapCell_Delete.cpp   CMapCell::Delete, gDeleteNoParent, gDeleteNoThing
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of other
// files the unit reads (gGameWorld, gThingArray, gLLCC47E4) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];
int32_t gLLCC47E4 = 0;

namespace {

CBankAccount* gAccount = nullptr;
CTrackCell* gTrack = nullptr;

// The hash of a text (the PowerPC stubs compute the same from the string in emulator memory).
int64_t Hash(const char* text) {
    uint32_t h = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) h = h * 31u + *p;
    return h;
}

uint8_t Rotate(uint8_t value, uint8_t by) { return static_cast<uint8_t>((value << by) | (value >> (8 - by))); }

}  // namespace

// ---- the fakes
int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level, Hash(format)}); }

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {lab(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

void CCellBase::RemoveNeighbour(uint8_t mask) { ev("RemoveNeighbour", {lab(this), mask}); }

CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    ev("GetNeighbouringCell", {lab(this), direction});
    const int64_t k = ret("GetNeighbouringCell");
    if (k == 0) return nullptr;
    return &gGameWorld->mMapCells[k];
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return Rotate(direction, 4);
}

void CMapCell::SetHFieldTile() { ev("SetHFieldTile", {lab(this)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetObjectThingId"));
    return id;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition, Hash(format)}); }

void CObject::RecalculateBackOfQueue() { ev("RecalculateBackOfQueue", {lab(this)}); }

uint8_t ADD_DIRECTION(uint8_t first, uint8_t second) {
    ev("ADD_DIRECTION", {first, second});
    return Rotate(first, second);
}

void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {lab(this), direction}); }

void CMapCell::ModifyType(CELL_MODES mode) { ev("ModifyType", {lab(this), static_cast<int32_t>(mode)}); }

int32_t CCellBase::NumberOfHVNeighbours() const {
    ev("NumberOfHVNeighbours", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfHVNeighbours"));
}

int32_t CObject::GetResidualValuePercentage() const {
    ev("GetResidualValuePercentage", {lab(this)});
    return static_cast<int32_t>(ret("GetResidualValuePercentage"));
}

int32_t LLCC_SystemGetQueueCost() {
    ev("LLCC_SystemGetQueueCost", {});
    return static_cast<int32_t>(ret("LLCC_SystemGetQueueCost"));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return gAccount;
}

void CBankAccount::MakeDeposit(uint32_t amount) { ev("MakeDeposit", {lab(this), amount}); }

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {lab(this)});
    return gTrack;
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_cd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"things", &gThingArray[0]},
        {"llcc", &gLLCC47E4},
        {"noparent", &gDeleteNoParent},
        {"nothing", &gDeleteNoThing},
        {"account", &gAccount},
        {"track", &gTrack},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    return n;
}

void tpw_cd_delete(uint64_t cell, int32_t a, int32_t b) { reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell))->Delete(a, b); }

}  // extern "C"
