// Test library for CMapCell::CalculateNeighbours (unit v5632; libtpw_unit_CMapCell_CalculateNeighbours.dylib,
// tests/test_CMapCell_CalculateNeighbours.py), main app:
//   src/MapDelta_map_CMapCell_CalculateNeighbours.cpp   CMapCell::CalculateNeighbours and the seven globals it keeps (gCalcNeighbours*)
//
// The callees outside the unit are fakes that record the arguments the real ones read: the two GetNeighbouringCell answer the cell the test's
// tables give for the caller and the offset (or direction bit), ADD_DIRECTION computes the real rotation, the getters answer from the queues.
// The globals of other files the unit reads are defined here, zero at start like the Mac's bss; the compass offsets have their real values.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld;
CActionRecorder gActionRecorder;
int32_t gSoundBanks[8];
uint16_t gLLCCDB68;
int32_t gLLCCDB6C;
CObject* gLLCMCurrentObject;
const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};
const MapDelta gMapDeltaMinusY = {0, -1, 0};
const MapDelta gMapDeltaPlusXMinusY = {1, -1, 0};
const MapDelta gMapDeltaMinusXPlusY = {-1, 1, 0};
const MapDelta gMapDeltaMinusX = {-1, 0, 0};
const MapDelta gMapDeltaMinusXMinusY = {-1, -1, 0};

namespace {

constexpr int32_t kMaxCells = 16;
int32_t gNbr[(kMaxCells + 1) * 9];   // caller label * 9 + (x + 1) * 3 + (y + 1) -> cell label (0 NULL)
int32_t gDir[(kMaxCells + 1) * 8];   // caller label * 8 + bit index -> cell label
uint64_t gWorldBase;                 // cell label i lies at world + 0x2d8 + i * 0x44

CMapCell* CellOf(int32_t label) {
    if (label <= 0) return nullptr;
    return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(gWorldBase + 0x2d8 + static_cast<uint64_t>(label) * 0x44));
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs (pointers as lab(p))
uint32_t CMap::MakeNewTimeStamp() {
    ev("MakeNewTimeStamp", {lab(this)});
    return static_cast<uint32_t>(ret("MakeNewTimeStamp"));
}

void CMap::SetTimeStamp(uint32_t stamp, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetTimeStamp", {lab(this), static_cast<int64_t>(stamp), cell.mIndex});
}

CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    const int32_t caller = static_cast<int32_t>(lab(this));
    ev("GetNeighbouringCellD", {caller, delta->mX, delta->mY, flag});
    if (caller < 1 || caller > kMaxCells) return nullptr;
    if (delta->mX < -1 || delta->mX > 1 || delta->mY < -1 || delta->mY > 1) return nullptr;
    return CellOf(gNbr[caller * 9 + (delta->mX + 1) * 3 + (delta->mY + 1)]);
}

CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    const int32_t caller = static_cast<int32_t>(lab(this));
    ev("GetNeighbouringCellB", {caller, direction});
    if (caller < 1 || caller > kMaxCells) return nullptr;
    for (int32_t i = 0; i < 8; ++i)
        if (direction == (1 << i)) return CellOf(gDir[caller * 8 + i]);
    return nullptr;
}

void CCellBase::ModifyNeighbours(uint8_t mask) { ev("ModifyNeighbours", {lab(this), mask}); }

void CCellBase::RemoveNeighbour(uint8_t mask) { ev("RemoveNeighbour", {lab(this), mask}); }

int32_t CMapCell::IsQueue() const {
    ev("IsQueue", {lab(this)});
    return static_cast<int32_t>(ret("IsQueue"));
}

void CMapCell::SetHFieldTile() { ev("SetHFieldTile", {lab(this)}); }

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {lab(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {lab(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}

// The real rotation (src/MapDelta_map.cpp), so the loop of the mode 1 path ends as on the Mac.
uint8_t ADD_DIRECTION(uint8_t a, uint8_t b) {
    ev("ADD_DIRECTION", {a, b});
    int32_t n = 0;
    int32_t first = a;
    while (static_cast<uint8_t>(first) > 1) {
        first = first >> 1;
        ++n;
    }
    const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(b) << n);
    if (v > 0x80) return static_cast<uint8_t>((v >> 8) & 0xff);
    return static_cast<uint8_t>(v & 0xff);
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"prevA", &gCalcNeighboursPrevA},
        {"prevB", &gCalcNeighboursPrevB},
        {"idMinusY", &gCalcNeighboursIdMinusY},
        {"idPlusX", &gCalcNeighboursIdPlusX},
        {"idPlusY", &gCalcNeighboursIdPlusY},
        {"idMinusX", &gCalcNeighboursIdMinusX},
        {"join", &gCalcNeighboursJoinEntry},
        {"db68", &gLLCCDB68},
        {"db6c", &gLLCCDB6C},
        {"curobj", &gLLCMCurrentObject},
        {"bank3", &gSoundBanks[3]},
        {"world", &gGameWorld},
        {"recording", &gActionRecorder.mRecording},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_cn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The fakes' tables of neighbours and the world the cells lie in.
void tpw_cn_tables(const int32_t* nbr, const int32_t* dir, uint64_t world) {
    std::memcpy(gNbr, nbr, sizeof(gNbr));
    std::memcpy(gDir, dir, sizeof(gDir));
    gWorldBase = world;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_cn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mNeighbours));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF32));
    out[n++] = static_cast<int64_t>(kMaxCells);
    return n;
}

void tpw_cn_calc(uint64_t cell, int32_t mode, int32_t a, int32_t b, int32_t flag) {
    reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell))->CalculateNeighbours(static_cast<CELL_MODES>(mode), a, b, flag != 0);
}

}  // extern "C"
