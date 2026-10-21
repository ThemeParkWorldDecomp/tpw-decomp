// Test library for CStaffHQ::DropStaff and ForceStrike (unit v5037; libtpw_unit_CStaffHQ_DropStaff.dylib), main app:
//   src/game/buildings/StaffHQ.cpp
//
// Callees GAME_ASSERTMSG, CTRACE and CStaff::DropMe are recording fakes; gThingArray is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

CThingArrayEntry gThingArray[kThingArraySize];

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

void CTRACE(uint32_t level, const char* format, ...) {
    ev("CTRACE", {static_cast<int64_t>(level)}, {format});
}

void CStaff::DropMe(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("DropMe", {lab(this), cell.mIndex});
}

extern "C" {

uint64_t tpw_hq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gThingArray", &gThingArray[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CStaffHQ));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mStrikes));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mF60));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mF64));
    return n;
}

void tpw_hq_row(int32_t index, uint64_t pointer) { gThingArray[index].mThing = reinterpret_cast<decltype(gThingArray[0].mThing)>(pointer); }

void tpw_hq_drop(uint64_t self, uint32_t cell) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(cell);
    reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(self))->DropStaff(id);
}

void tpw_hq_force(uint64_t self, int32_t type) {
    reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(self))->ForceStrike(static_cast<StaffType>(type));
}

}  // extern "C"
