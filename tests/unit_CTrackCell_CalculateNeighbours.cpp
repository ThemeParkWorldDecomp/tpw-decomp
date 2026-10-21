// Test library for CTrackCell::CalculateNeighbours (unit v5192; libtpw_unit_CTrackCell_CalculateNeighbours.dylib), main app:
//   src/game/park/TrackCell.cpp
//
// The callees outside the unit (GetNeighbouringCell, IsTrack, NumberOfNeighbours, ModifyNeighbours) are recording fakes; a cell is named in the
// log by the word at +0x1c, which the test sets.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TrackCell.h"
#include "ObjectParts.h"
#include "LLCellSystem.h"

using namespace calllog;

int32_t gLLCCE288;
int32_t gLLCCE28C;
uint16_t gLLCCDB68;
const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};
const MapDelta gMapDeltaMinusX = {-1, 0, 0};
const MapDelta gMapDeltaMinusY = {0, -1, 0};

namespace {

CTrackCell* gNbr;

int64_t Tag(const void* p) { return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(p) + 0x1c); }

}  // namespace

CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("GetNeighbouringCell", {Tag(this), delta.mX, delta.mY, offset});
    return ret("neigh") ? gNbr : nullptr;
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {Tag(this)});
    return static_cast<int32_t>(ret("istrack"));
}

int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {Tag(this)});
    return static_cast<int32_t>(ret("nn"));
}

void CCellBase::ModifyNeighbours(uint8_t mask) { ev("ModifyNeighbours", {Tag(this), mask}); }

extern "C" {

uint64_t tpw_cn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"f485c", &gTrackNbrOn485C}, {"e288", &gLLCCE288}, {"e28c", &gLLCCE28C}, {"db68", &gLLCCDB68},
        {"g290", &gTrackNbr290},     {"g292", &gTrackNbr292}, {"g294", &gTrackNbr294}, {"g296", &gTrackNbr296},
        {"g298", &gTrackNbr298},     {"g29a", &gTrackNbr29A}, {"g29c", &gTrackNbr29C}, {"g29e", &gTrackNbr29E},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cn_layout(int64_t* out) {
    alignas(8) static char buf[0x40];
    CTrackCell* c = reinterpret_cast<CTrackCell*>(buf);
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&c->mType) - buf);
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&c->mParentID) - buf);
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

void tpw_cn_calc(uint64_t self, uint64_t nbr, int32_t mode, int32_t a, int32_t b) {
    gNbr = reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(nbr));
    reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(self))->CalculateNeighbours(static_cast<CELL_MODES>(mode), a, b);
}

}  // extern "C"
